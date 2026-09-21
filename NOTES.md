# Measurements and what they cost to get

All of this comes from one day on one machine: MacBook Pro M3 Max, 96 GiB,
macOS 26.6.2, llama.cpp `661643e`, ds4 at its 2026-09 HEAD. Small task counts
throughout. Treat these as orders of magnitude, not a benchmark.

## Throughput

2048-token prompt, 128 generated tokens, one model resident at a time.

| backend | model | size | prefill | decode |
| --- | --- | ---: | ---: | ---: |
| llama.cpp | Qwen3.8-Flash-Next UD-IQ1_S | 67.55 GiB | 309.7 t/s | 20.3 t/s |
| llama.cpp | Qwen3.8-Flash-Next UD-Q2_K_XL | 73.44 GiB | 285.8 t/s | 19.3 t/s |
| ds4 (resident) | DeepSeek V4 Flash Q2 | 80.76 GiB | 173–182 t/s | 15.1–15.8 t/s |
| ds4 (streaming) | DeepSeek V4 Flash Q2 | 80.76 GiB | 106.2 t/s | 4.2 t/s |

**A quantisation tier costs less than its size suggests.** IQ1_S → Q2_K_XL is
+5.9 GiB for −5% decode. For a MoE with ~6B active parameters, file size is a
poor predictor of decode speed: decode only reads the active experts.

**Streaming vs resident dominates everything else.** The same ds4 build is
3.7× faster at decode once the model fits under the wired cap. A first pass
here compared Qwen resident against ds4 streaming and found a 4.8× decode
gap. That number was the memory ceiling, not the engines. Resident against
resident it is 1.25×.

Quantisation sizes, for scale (`unsloth/Qwen3.8-Flash-Next-GGUF`):

| quant | size | vs BF16 |
| --- | ---: | ---: |
| UD-IQ1_S | 72.5 GB | 20.5% |
| UD-IQ1_M | 74.5 GB | 21.1% |
| UD-Q2_K_XL | 78.9 GB | 22.3% |
| UD-Q4_K_XL | 111.3 GB | 31.5% |
| Q8_0 | 188.2 GB | 53.2% |
| BF16 | 354 GB | 100% |

## Agent tool loops reuse the KV cache

`ds4-agent --trace` on a multi-tool task, SSD streaming:

| tool round | prompt tokens | cached | actually prefilled | prefill time |
| ---: | ---: | ---: | ---: | ---: |
| 0 | 4,853 | 1,839 | 3,014 | 41.9 s |
| 1 | 5,383 | 4,896 | 487 | 13.6 s |
| 2 | 5,741 | 5,426 | 315 | 8.0 s |

Per-round prefill cost nearly vanishes. This is why a 4.2 t/s decode rate is
still usable for agent work: tool round trips are short writes, not long
generations, and the context is not recomputed between them. The same trace
shows a system-prompt KV hit restoring 1,839 tokens from disk at startup.

That said, the full agent task took 444 s under streaming against 138 s for
the same task on a resident server. Usable is not the same as fast.

## Quality

Two task families, both graded mechanically. See [bench/](bench/).

**Five C functions**, scored by compiling and running a test program:

| task | Qwen Q2_K_XL | DeepSeek V4 Flash Q2 |
| --- | --- | --- |
| `utf8_truncate` | 22/22 | 10/22 — off-by-one finding the lead byte |
| `dedup_sorted` | pass | pass |
| `lower_bound` | pass | pass |
| `str_replace_all` | pass | pass |
| `parse_int_strict` | 0/8 accepts — left a `return 0; /* Placeholder */` | pass |
| | **4/5** | **4/5** |

The two failures are different in kind. DeepSeek wrote working code with a
wrong boundary, and a comment that confidently described what the code did
not do. Qwen talked itself into a corner on `LONG_MIN`, announced it would
restart, and shipped the placeholder. The second failure mode is louder and
therefore easier to catch in review.

**ALPS → fake data + JSON Schema + SQLite DDL**, 31 checks, 10 negative:

| | round 0 | round 1 |
| --- | --- | --- |
| DeepSeek V4 Flash Q2 | 31/31 | — |
| Qwen3.8-Flash-Next Q2_K_XL | 28/31 | 31/31 |

Both converge. On this task the models are close enough that the ordering
would flip with one more or one fewer check.

Worth noting what a structured-conversion task is: the answer is largely
present in the input. Descriptor ids and `def` URIs determine the column
types and names; the model applies a mapping. The C tasks asked for invariants
to be *discovered*. The gap between 4/5 on C and 31/31 on ALPS is a gap
between those two kinds of work, not a gap in the models.

## What this cost to measure

Four times, a difference that looked like a model capability gap turned out to
be a bug in the measurement:

1. **n=1.** One task — a UTF-8 truncation function — showed a clean 22/22 vs
   10/22 split, and a conclusion was drawn from it. Four more tasks brought it
   to 4/5 vs 4/5.

2. **A rule the task never stated.** The SQL rubric required `NOT NULL` on the
   primary key. SQLite does not imply it for non-INTEGER primary keys, so
   `PRAGMA table_info` reported `notnull=0` — but the skill document being
   tested also omits `NOT NULL` in its own examples. Both models were
   penalised for following the instructions. One of them patched it when told;
   the other did not, which read as "cannot self-correct". Removing the bogus
   check turned that into "both converge in at most one round".

3. **A missing `commit()`.** A rollback inside one negative test silently
   emptied the table, so a later test failed for an unrelated reason. It
   happened not to fire in the runs that were scored, which is worse: a latent
   grader bug that would have surfaced as a model difference on some other
   input.

4. **A one-turn harness.** `ds4-agent --non-interactive -p` runs a single
   turn. An agent ended its turn saying "let me write the file first" and
   never got another. Scored as a failure; it was the harness. The trace also
   showed `context_limited=1` on every round including two that ended
   normally, which briefly became a wrong explanation for the stop.

Packaging this repository then produced four more of the same kind, in the
tooling rather than the measurements: a `status` function that accidentally
executed `w(1)`, two README examples that could not run, and a rubric
isolation guard that silently fell back to the co-located copy it was meant
to bypass.

A second failure mode is worth separating from those eight, because it is not
a bug in a harness. Twice I wrote that ds4 had a capability llama.cpp lacked
— prompt caching, then an Anthropic-compatible endpoint — and both times
`llama.cpp/tools/server/README.md` said otherwise, in a section I had never
opened. I had read ds4's documentation down to its `--help` subtopics.

Both errors pointed the same way: toward the side I had studied. Reading one
participant's documentation and not the other's does not add noise, it adds
bias, and the bias is invisible from the inside because each individual claim
feels well grounded. The first one was published before it was caught.

The conclusion is not that the models are good. It is that two models of this
class, on this hardware, are close enough that a hand-built rig will produce
its own artefacts faster than it produces real differences.

**If a comparison shows a large gap, suspect the harness first — then check
whether you have read both sides.**

Real usage separates them faster than synthetic tasks do. That is what
`llm-switch` is for.
