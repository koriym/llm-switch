# bench/

An objective harness for comparing local model output. Included less as a
benchmark suite than as a worked example of grading without reading the
output yourself.

Two task families:

- `c-tasks/` — five C functions, each with a prompt and a test program.
  Scoring is compile + run under ASan/UBSan, not inspection.
- `alps/` — an [ALPS](https://alps.io) profile in, three artifacts out: fake
  data, a JSON Schema, SQLite DDL. 31 checks, 10 of them negative.

## Grading by cross-check

The ALPS rubric never judges an artifact on its own. Each one is checked
against the others:

- the schema must validate every fake record
- the DDL must execute, and the same fake records must INSERT
- **negative tests**: a `rating` of 6 must be rejected by the schema *and* by
  a CHECK constraint; an unknown property, a missing required field, a
  malformed email and a duplicate primary key must all be rejected

Without the negative half, a permissive schema and a constraint-free table
score full marks. This is the part worth copying into your own harness.

## Running it

The prompt embeds a third-party skill document, which is not vendored here.
Build it first, pointing at your own copy:

```sh
cd bench/alps
python3 make_prompt.py --skill-doc /path/to/alps-to-sql/SKILL.md

python3 ask.py 8765 prompt.txt out.txt        # one shot against llm-switch's port
python3 loop.py demo 8765 --rounds 3          # generate -> grade -> feed failures back
python3 verify.py out/demo.r0.out             # grade an existing response
```

`--skill-doc` is optional; without it the prompt still runs, it just stops
testing whether the model can follow a supplied document.

A real run of `loop.py`, Qwen3.8-Flash-Next Q2_K_XL:

```
[demo] round 0: 28/31
    FAIL schema: every string has min/maxLength
    FAIL schema: maxLength >= observed longest
    FAIL schema: maxLength not arbitrary (<=4x observed)
[demo] round 1: 31/31
[demo] GREEN at round 1
```

C tasks:

```sh
cd bench/c-tasks
# send dedup.prompt.txt to a model, save the C function as dedup.impl.c, then:
cc -std=c11 -O1 -Wall -fsanitize=address,undefined -o t dedup.test.c dedup.impl.c && ./t
```

## Two things the loop gets right

**Feedback is not truncated.** An early version clipped the previous answer to
the last 6000 bytes, which dropped the first of three code blocks. The model
then re-submitted without it and looked like it could not converge. Whatever
you feed back, feed back all of it.

**Fixed points are detected.** At temperature 0, a round whose output is
byte-identical to the previous one will produce an identical prompt next time.
That is a fixed point, not a model still trying; the loop stops instead of
burning minutes re-asking.

## Grading an agent

An agent with a shell tool can read — and rewrite — any grader it can reach.
When the model produces files rather than a single response, copy `verify.py`
outside the agent's working directory:

```sh
LLM_SWITCH_RUBRIC=/somewhere/else python3 check.py
```

`check.py` refuses to run if that path holds no `verify.py`, and asserts that
the module actually imported came from there — Python puts a script's own
directory on `sys.path`, so a wrong path would otherwise fall back to the
co-located copy and report a score while the rubric was reachable all along.

Afterwards, grep the agent's log for reads of that path before trusting the
score. In one run here the agent read the rubric before writing anything, and
scored full marks; that number means something different from one earned
without it.

## Files

| file | role |
| --- | --- |
| `alps/profile.json` | the ALPS input |
| `alps/make_prompt.py` | builds the prompt, optionally embedding a skill doc |
| `alps/verify.py` | the 31 checks; also a CLI for grading saved responses |
| `alps/ask.py` | one request to an OpenAI-compatible server |
| `alps/loop.py` | generate → grade → feed failures back → repeat |
| `alps/check.py` | grade `fake.json` / `schema.json` / `review.sql` on disk |
| `c-tasks/*.prompt.txt` | task statements |
| `c-tasks/*.test.c` | test programs |
