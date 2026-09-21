# llm-switch

Run exactly one large local model at a time, always on the same port.

On a unified-memory Mac, two 70–80 GiB models cannot coexist. Starting the
second one while the first is still resident does not degrade gracefully — it
thrashes swap and takes the machine with it. `llm-switch` makes the mutual
exclusion explicit: every start stops the other backend first and verifies it
is gone before loading anything.

Both backends speak the OpenAI chat-completions API on one shared port, so
clients never need reconfiguring when the model changes.

```
llm-switch llama              # llama.cpp backend
llm-switch ds4                # DwarfStar (ds4), SSD streaming
llm-switch ds4 --resident     # DwarfStar fully resident (needs a raised wired limit)
llm-switch stop
llm-switch status
```

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

This is the part worth knowing even if you never use this script.

Metal keeps GPU buffers *wired* — they cannot be paged out. macOS caps how
much of RAM may be wired, and that cap, not total RAM, is the real limit on
model size. On a 96 GiB M3 Max:

```
recommendedMaxWorkingSetSize = 83494.17 MB   # = 77.76 GiB, on a 96 GiB machine
```

A model above the cap silently falls back to a slower streaming path, or fails
to load. `sysctl iogpu.wired_limit_mb` overrides it; `0` means "let macOS
decide".

`llm-switch ds4 --resident` refuses to start and prints the command rather
than raising the limit itself, because a too-high limit starves the OS.
Raising it to 86 GiB on a 96 GiB machine leaves ~10 GiB for everything else;
while it was set during testing, swap grew from 1 GB to 9 GB.

```sh
sudo sysctl iogpu.wired_limit_mb=88064     # raise
sudo sysctl iogpu.wired_limit_mb=0         # restore
```

## Speed, roughly

M3 Max, 96 GiB. 2048-token prompt, 128 generated tokens, one model at a time.

| backend | model | size | prefill | decode |
| --- | --- | ---: | ---: | ---: |
| llama.cpp | Qwen3.8-Flash-Next UD-Q2_K_XL | 73.44 GiB | 285.8 t/s | 19.3 t/s |
| ds4 (resident) | DeepSeek V4 Flash Q2 | 80.76 GiB | 173–182 t/s | 15.1–15.8 t/s |
| ds4 (streaming) | DeepSeek V4 Flash Q2 | 80.76 GiB | 106.2 t/s | 4.2 t/s |

The same ds4 build is 3.7× faster at decode once the model fits under the
wired cap. Any comparison that puts one engine in streaming mode and the other
resident is measuring the memory ceiling, not the engines.

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

Four guesses were spent on caches, registry ids and config keys before
`ps -eo pid,etime` showed the server had been up twelve hours — longer than
the config had existed.


**Be realistic about the latency.** A one-line "create this file" task took
**908 s** end to end here. The server log accounts for it:

| | tokens | time | calls |
| --- | ---: | ---: | ---: |
| prompt eval | 57,110 | 346 s | 124 |
| decode | 10,963 | 570 s | 124 |

Decode dominates, not prefill. An agent turn is many short model calls, and
each one pays ~19 t/s for its own output. Prompt caching is working — 124
calls cost 57k prompt tokens in total, against the 2.3M they would cost if
OpenCode's 18,634-token system prompt were reprocessed every time.

Local agents on this hardware are for work you are willing to leave running,
not for interactive back-and-forth.

`llama-server` caches prompts in host RAM by default (`--cache-prompt`), and
`--cache-reuse N` plus `--slot-save-path` with `POST /slots/{id}?action=save`
extend that to KV shifting and on-disk slots. ds4 takes a different route:
`ds4-agent` needs no server at all and reuses its KV across tool rounds — see
[NOTES.md](NOTES.md#agent-tool-loops-reuse-the-kv-cache).

## bench/

An objective harness for comparing model output: C functions graded by
compiling and running tests, and an ALPS-to-artifacts task graded by 31 checks
of which 10 are negative. See [bench/README.md](bench/README.md).

## License

MIT
