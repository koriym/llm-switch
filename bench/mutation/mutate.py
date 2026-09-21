#!/usr/bin/env python3
"""Score a test suite by how many mutants of the implementation it kills.

Coverage says which lines ran. Mutation says whether running them would have
noticed anything, which is the property you actually want from a test and the
one a model can satisfy trivially without.

A mutant is a single small edit to the implementation: a comparison flipped,
a constant nudged, a branch inverted. A test suite *kills* it if the suite
passes on the original and fails on the mutant.

Mutants that no suite can kill (equivalent, or simply not observable through
this function's contract) would otherwise punish every suite equally. So the
denominator is not all mutants: it is the mutants a known-good hand-written
suite kills. Score = killed_by_candidate / killed_by_gold.
"""
import argparse, pathlib, re, shutil, subprocess, sys, tempfile

CC = ["cc", "-std=c11", "-O1", "-w"]

# (regex, replacement). Applied to one occurrence at a time. Anything that
# fails to compile is discarded, which absorbs most of the damage textual
# mutation does to C.
OPERATORS = [
    (r"(?<![<>=!+\-*/])<(?![<=])", "<="),
    (r"(?<![<>=!+\-*/])<=", "<"),
    (r"(?<![<>=!+\-*/])>(?![>=])", ">="),
    (r"(?<![<>=!+\-*/])>=", ">"),
    (r"==", "!="),
    (r"!=", "=="),
    (r"&&", "||"),
    (r"\|\|", "&&"),
]


def mutants(src):
    """Yield (mutated_source, description). One single-token edit each."""
    # Preprocessor lines are masked with spaces rather than removed: the mask
    # must be the same length as the source, or every offset after the first
    # #include splices into the wrong bytes and the mutant quietly becomes
    # unbuildable instead of interesting.
    masked = "\n".join(
        " " * len(line) if line.lstrip().startswith("#") else line
        for line in src.split("\n"))
    assert len(masked) == len(src)

    for pat, rep in OPERATORS:
        for m in re.finditer(pat, masked):
            yield (src[:m.start()] + rep + src[m.end():],
                   f"{m.group(0)} -> {rep} at {m.start()}")

    for m in re.finditer(r"(?<![\w.])(\d+)(?![\w.])", masked):
        n = int(m.group(1))
        for v in sorted({n + 1, n - 1 if n else 0, 0} - {n}):
            yield (src[:m.start(1)] + str(v) + src[m.end(1):],
                   f"{n} -> {v} at {m.start(1)}")


def build_and_run(workdir, test_c, impl_src, tag):
    """Returns 'pass', 'fail', or 'nobuild'."""
    impl = workdir / f"{tag}.c"
    impl.write_text(impl_src)
    exe = workdir / f"{tag}.bin"
    r = subprocess.run(CC + ["-o", str(exe), str(test_c), str(impl)],
                       capture_output=True)
    if r.returncode != 0:
        return "nobuild"
    try:
        # Mutating a loop bound turns a binary search into an infinite one, so
        # hangs are common and must be cheap. The suites finish in
        # milliseconds; anything near a second is already a detected change.
        r = subprocess.run([str(exe)], capture_output=True, timeout=5)
    except subprocess.TimeoutExpired:
        return "fail"
    return "pass" if r.returncode == 0 else "fail"


def score(impl_path, test_path, gold_path, verbose=False):
    impl_src = impl_path.read_text()
    work = pathlib.Path(tempfile.mkdtemp(prefix="mutation."))
    try:
        # A suite that does not pass on the correct implementation is not a
        # suite; scoring it against mutants would be meaningless. Say which
        # kind of not-a-suite it is: a build error and a wrong expectation
        # are different failures and get confused otherwise.
        for name, path in (("candidate", test_path), ("gold", gold_path)):
            if not path:
                continue
            state = build_and_run(work, path, impl_src, f"base_{name}")
            if state == "nobuild":
                return None, f"{name} suite does not compile"
            if state != "pass":
                return None, f"{name} suite fails on the reference implementation"

        killed_c = killed_g = viable = 0
        seen = set()
        for mut_src, desc in mutants(impl_src):
            if mut_src == impl_src or mut_src in seen:
                continue
            seen.add(mut_src)
            g = build_and_run(work, gold_path, mut_src, "g") if gold_path else "fail"
            if g == "nobuild":
                continue
            if g != "fail":
                continue        # gold cannot see it either: not a fair target
            viable += 1
            killed_g += 1
            c = build_and_run(work, test_path, mut_src, "c")
            if c == "fail":
                killed_c += 1
            elif verbose:
                print(f"  survived: {desc}")
        return (killed_c, viable), None
    finally:
        shutil.rmtree(work, ignore_errors=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("impl", type=pathlib.Path, help="reference implementation .c")
    ap.add_argument("test", type=pathlib.Path, help="candidate test program .c")
    ap.add_argument("--gold", type=pathlib.Path, required=True,
                    help="hand-written suite defining which mutants are killable")
    ap.add_argument("-v", "--verbose", action="store_true",
                    help="list the mutants the candidate missed")
    a = ap.parse_args()

    for p in (a.impl, a.test, a.gold):
        if not p.exists():
            sys.exit(f"no such file: {p}")

    result, err = score(a.impl, a.test, a.gold, a.verbose)
    if err:
        print(f"INVALID: {err}")
        return 2
    killed, viable = result
    pct = killed / viable * 100 if viable else 0
    print(f"killed {killed}/{viable} killable mutants ({pct:.0f}%)")
    return 0 if killed == viable else 1


if __name__ == "__main__":
    sys.exit(main())
