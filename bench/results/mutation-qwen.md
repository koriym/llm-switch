# Mutation score, Qwen3.8-Flash-Next UD-Q2_K_XL

llama.cpp on M3 Max 96 GiB, ctx 65536, temperature 0, suite capped at 20
checks. The model sees the function and its contract, never the mutants or
the hand-written suite.

| task | killed / killable | note |
| --- | ---: | --- |
| `utf8_truncate` | INVALID | does not compile |
| `dedup_sorted` | 10 / 10 | |
| `lower_bound` | 7 / 7 | |
| `parse_int_strict` | 31 / 31 | |
| `str_replace_all` | 5 / 5 | |

Four suites out of five kill every mutant a hand-written suite kills, inside
a twenty-check budget.

## The one failure is a C lexing trap, not a testing one

```c
check("\xC3\xA9a", 2, 2, "\xC3\xA9");
//         ^^^^ hex escape sequence out of range
```

C hex escapes have no digit limit, so `\xA9a` is read as a single escape and
overflows. `"\xC3\xA9" "a"` is the fix. Writing tests for a UTF-8 function
makes multi-byte string literals unavoidable, so the trap was reachable; the
test design was sound and the language rule was not known.

## This bench is saturated

Every compiling suite scored full marks, which means it cannot currently
separate one model from another — the same ceiling already diagnosed for the
ALPS task. Two denominators (5 and 7) are small enough that a single mutant
moves the percentage by 14–20%.

Use it as a regression check: when ds4, llama.cpp or a quantisation changes,
a drop from full marks is a real signal. Do not read a tie here as evidence
that two models are equally good at writing tests.
