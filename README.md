# llm-switch

Run exactly one large local model at a time.

On a unified-memory Mac, two 70–80 GiB models cannot coexist. Starting the
second one while the first is still resident does not degrade gracefully — it
thrashes swap and takes the machine with it. `llm-switch` makes the mutual
exclusion explicit: every start stops the other backends first and verifies
they are gone before loading anything.

Two of them are servers speaking the OpenAI chat-completions API on one
shared port, so clients never need reconfiguring when the model changes. The
third, `ds4-agent`, is not a server at all — it loads the same weights into
its own process — which makes it just as exclusive and just as easy to
forget.

```
llm-switch llama              # llama.cpp server
llm-switch ds4                # DwarfStar server, SSD streaming
llm-switch ds4 --resident     # DwarfStar server fully resident (needs a raised wired limit)
llm-switch agent [args...]    # stop the servers, then run ds4-agent here
llm-switch stop
llm-switch status
```

A running agent makes the server commands refuse rather than load a second
copy, since you may be mid-session, and `stop` says so instead of reporting
success while 80 GiB is still held.

## Install

```sh
git clone https://github.com/koriym/llm-switch ~/git/llm-switch
mkdir -p ~/.config/llm-switch
cp ~/git/llm-switch/config.example ~/.config/llm-switch/config
$EDITOR ~/.config/llm-switch/config        # set model paths
export PATH="$HOME/git/llm-switch/bin:$PATH"
```

Requires `curl` and, per backend, a built
[`llama-server`](https://github.com/ggml-org/llama.cpp) and/or
[`ds4-server`](https://github.com/antirez/ds4).

## The wired-memory ceiling

Worth knowing even if you never use this script. Metal keeps GPU buffers
*wired* — they cannot be paged out — and macOS caps how much of RAM may be
wired. That cap, not total RAM, is the real limit on model size:

```
recommendedMaxWorkingSetSize = 83494.17 MB   # = 77.76 GiB, on a 96 GiB machine
```

Above it a model silently falls back to streaming, or fails to load.
`llm-switch ds4 --resident` refuses to start and prints the command rather
than raising the limit itself: at 86 GiB on this machine only ~10 GiB is
left for everything else, and swap grew from 1 GB to 9 GB while it was set.

```sh
sudo sysctl iogpu.wired_limit_mb=88064     # raise
sudo sysctl iogpu.wired_limit_mb=0         # restore
```

## Speed, roughly

M3 Max, 96 GiB. 2048-token prompt, 128 generated tokens, one model at a time.

| backend | model | size | prefill | decode |
| --- | --- | ---: | ---: | ---: |
| llama.cpp | Qwen3.8-Flash-Next UD-Q2_K_XL | 73.44 GiB | 285.8 t/s | 19.3 t/s |
| ds4-server (resident) | DeepSeek V4 Flash Q2 | 80.76 GiB | 173–182 t/s | 15.1–15.8 t/s |
| ds4-server (streaming) | DeepSeek V4 Flash Q2 | 80.76 GiB | 106.2 t/s | 4.2 t/s |

The same ds4 build is 3.7× faster at decode once the model fits under the
wired cap. Any comparison that puts one engine in streaming mode and the other
resident is measuring the memory ceiling, not the engines.

`ds4-agent` is absent from this table on purpose: a benchmark request and an
agent session are not measured the same way, and the derived agent figure is
2.5× the server's streaming decode for reasons not yet established.

Full numbers, KV-cache behaviour, and quality results: [NOTES.md](NOTES.md).

## Using it as a coding agent

The server is OpenAI-compatible, so any client that accepts a custom base URL
works. With [OpenCode](https://opencode.ai), merge this into
`~/.config/opencode/opencode.json`:

```json
{
  "provider": {
    "llm-switch": {
      "name": "llm-switch (local)",
      "npm": "@ai-sdk/openai-compatible",
      "options": { "baseURL": "http://127.0.0.1:8765/v1", "apiKey": "local" },
      "models": {
        "qwen3.8-flash-next": { "limit": { "context": 65536, "output": 16384 } },
        "deepseek-v4-flash":  { "limit": { "context": 32768, "output": 16384 } }
      }
    }
  }
}
```

One provider, one port; pick the model entry matching whatever is loaded.
Keep `limit.context` equal to `LLAMA_CTX` / `DS4_CTX`, or the client will pack
a prompt the server refuses and then retry in a loop.

```sh
llm-switch llama
opencode run --model llm-switch/qwen3.8-flash-next "..."
```

If a GUI front end such as Paseo still reports `Model not found` after you
edit the config, it is holding a long-lived `opencode serve` that read the
file at launch. Restarting the app does not recycle it:

```sh
pkill -f "opencode serve"      # respawns on the next request, with the new config
```

**Be realistic about the latency.** A one-line "create this file" task took
908 s end to end. An agent turn is many short model calls, each paying
~19 t/s for its own output, and decode dominates it: 570 s against 346 s of
prompt processing. Caching is not the problem — `llama-server` caches
prompts in host RAM by default, and those 124 calls cost 57k prompt tokens
rather than the 2.3M that reprocessing OpenCode's 18,634-token system prompt
each time would have. Local agents here are for work you can leave running.

### ds4-agent, the other shape

`ds4-agent` is not a server. It owns the loop and the tools itself, so the
caller hands it a task rather than driving it turn by turn:

```sh
llm-switch agent --chdir /path/to/work
llm-switch agent --chdir /path/to/work --non-interactive --prompt-file task.md
```

Streaming mode is usable here even though its decode rate is not. After the
first tool round the context is already cached, so each later round costs
only the tokens the model writes — and a tool call is a short write.
[Measured here.](bench/results/ds4-agent-streaming.md)

Choose by who owns the agent loop, not by task size. If the caller brings
its own tools, permissions and sub-agents — Paseo, OpenCode, Claude Code —
use a server; those can also run several sessions at once. If you want ds4's
loop, its tools and its session KV, use the agent: one process, one session.

## What to expect

Work with a goal and a mechanical check is mostly within reach and does not
need a frontier model. Two limits, observed rather than assumed: the task
has to be a transformation rather than a discovery — both models turned ALPS
descriptors into columns reliably, and each failed one task in five where
the answer had to be worked out — and the scope has to be bounded, since the
model that emitted 132 test cases unprompted scored full marks when capped
at twenty.

Differences between the two were real but small. Memory decided which to run
daily, not quality. [NOTES.md](NOTES.md) has the numbers.

## bench/

An objective harness for comparing model output: C functions graded by
compiling and running tests, an ALPS-to-artifacts task graded by 31 checks of
which 10 are negative, `bench/mutation/` scoring generated tests by what they
catch, and `bench/agent/` for summarising a `ds4-agent --trace`. See
[bench/README.md](bench/README.md).

## License

MIT
