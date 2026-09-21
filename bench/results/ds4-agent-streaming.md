# ds4-agent, SSD streaming, ALPS task

M3 Max 96 GiB, DeepSeek V4 Flash Q2 (0731), ctx 32768, `--nothink`.
Graded by the 31-check rubric in `bench/alps/` (10 of them negative).

| | |
| --- | ---: |
| score | 31/31 |
| wall | 657 s |
| tool rounds | 14 |
| prefill | 7,437 tok / 122.1 s (60.9 t/s) |
| decode | 5,682 tok, 10.7 t/s |
| cache saving | 18.9x (140,628 -> 7,437 prompt tokens) |

Cache reuse per round climbs 35% -> 96% -> 99-100% and stays there: after the
first round almost nothing is reprocessed. Full table in
`ds4-agent-streaming.txt`; the trace here has its per-token lines stripped
(1.2 MB -> 20 KB), which is why it reports no decode rate.

Two things worth flagging.

**Decode here is 10.7 t/s, against 4.23 t/s measured for ds4-server under the
same SSD streaming setting.** A long-lived agent process may keep the routed
expert cache warmer across tool rounds than a single benchmark request does,
but that is a hypothesis, not a finding. A resident run cannot settle it —
resident has no expert cache misses at all, so its number is consistent with
either explanation. The discriminating test is a sustained or back-to-back
generation against ds4-server in streaming mode: if its decode climbs from
4.23 toward 10.7, cache warmth is the cause.

**The agent could have read the rubric and chose not to.** It located the
directory, listed it, saw `verify.py`, and wrote: "Reading it would be visible
in agent.log and could compromise grading integrity. I should NOT read
verify.py." Isolation against a tool-using agent is not achievable; the
warning in `check.py` and the leak check after the run are what the score
rests on.
