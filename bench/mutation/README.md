# bench/mutation

Score a test suite by how many mutants of the implementation it kills.

Coverage tells you which lines ran. It does not tell you whether running them
would have noticed anything, which is the only property that makes a test
worth keeping — and the one a model can satisfy trivially without.

```sh
./run.sh 8765                 # ask the model on llm-switch's port, score the result
python3 mutate.py impl/dedup_sorted.c my.test.c --gold ../c-tasks/dedup.test.c -v
```

## How it scores

A mutant is one small edit to the implementation: a comparison flipped, a
constant nudged, a branch inverted. A suite *kills* it if the suite passes on
the original and fails on the mutant.

Mutants nobody can kill — equivalent ones, or differences not observable
through the function's contract — would punish every suite equally, so they
are not in the denominator. The denominator is the set of mutants a
hand-written suite from `../c-tasks/` kills:

```
score = killed_by_candidate / killed_by_gold
```

A suite that does not compile, or that fails on the correct implementation,
is reported as `INVALID` rather than scored zero. Those are different
failures from a weak suite and collapsing them loses information.

## That it discriminates

The hand-written suites kill everything, by construction. A deliberately lazy
suite for `dedup_sorted` — one array, no duplicates, no empty input — scores
60%, and names what it missed:

```
survived: 0 -> 1 at 74      # if (n == 0) became if (n == 1)
survived: 1 -> 2 at 102     # size_t w = 1 became 2
survived: 1 -> 2 at 166     # a[w-1] became a[w-2]
```

Line coverage for that suite is 100%.

## Denominators are small

| task | killable mutants |
| --- | ---: |
| `parse_int_strict` | 31 |
| `utf8_truncate` | 17 |
| `dedup_sorted` | 10 |
| `lower_bound` | 7 |
| `str_replace_all` | 5 |

On `str_replace_all` one mutant is worth 20%, so its percentage is not
comparable with `parse_int_strict`'s. Read the fraction, not the percentage.
Widening the operator set (`+`↔`-`, `++`↔`--`, statement deletion) would help;
so would more implementations.

## The cap matters

Without a limit on the number of checks, the first model tried here emitted
132 cases and was still going when the token budget ran out. Mutation scoring
rewards choosing cases, not producing them, so the prompt caps the suite at
20 checks. With the cap the same model scored full marks on four tasks out of
five.

That gap — exhaustive enumeration when unconstrained, accurate selection when
bounded — is the more interesting result than the score.
