#!/usr/bin/env python3
"""Generate, grade, feed the failures back, repeat.

Scores rounds-to-green rather than one-shot output. Each round is a fresh
stateless request whose prompt carries the FULL previous answer plus the
verifier report, so nothing is silently dropped from the feedback - a
truncated feedback prompt makes the model look like it cannot converge when
really it was never shown what to fix.
"""
import argparse, contextlib, io, pathlib, sys, time

HERE = pathlib.Path(__file__).parent
sys.path.insert(0, str(HERE))
import verify                      # noqa: E402
from ask import ask                # noqa: E402


def score(path):
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        s, t = verify.report("round", str(path))
    return s, t, buf.getvalue()


def feedback(task, prev, report):
    fails = "\n".join(l for l in report.splitlines() if l.strip().startswith("FAIL"))
    return (task
            + "\n---\nAn automated checker ran against your previous answer and "
              "some checks FAILED.\n\nYour previous answer in full:\n"
            + prev.strip()
            + "\n\nChecks that failed:\n" + fails
            + "\n\nFull checker output:\n" + report.strip()
            + "\n\nProduce the three corrected code blocks. Keep everything that "
              "already passed; change only what is needed to fix the FAIL lines.\n")


def run(tag, port, prompt_file, outdir, rounds, max_tokens):
    task = pathlib.Path(prompt_file).read_text()
    outdir.mkdir(parents=True, exist_ok=True)
    hist = []
    for rnd in range(rounds):
        out = outdir / f"{tag}.r{rnd}.out"
        prompt = task if rnd == 0 else feedback(
            task, (outdir / f"{tag}.r{rnd-1}.out").read_text(errors="replace"), hist[-1][2])
        if rnd > 0:
            (outdir / f"{tag}.r{rnd}.prompt.txt").write_text(prompt)
        t0 = time.time()
        out.write_text(ask(port, prompt, max_tokens))
        print(f"[{tag}] round {rnd} generated in {time.time()-t0:.0f}s "
              f"({out.stat().st_size} bytes)")

        s, t, report = score(out)
        hist.append((s, t, report))
        print(f"[{tag}] round {rnd}: {s}/{t}")
        for line in report.splitlines():
            if line.strip().startswith("FAIL"):
                print("   ", line.strip())
        if s == t:
            print(f"[{tag}] GREEN at round {rnd}")
            break
        # Identical output means the next prompt would be identical too, and at
        # temperature 0 that is a fixed point, not a model still trying.
        if rnd > 0 and out.read_text() == (outdir / f"{tag}.r{rnd-1}.out").read_text():
            print(f"[{tag}] fixed point: output identical to previous round, stopping")
            break
    return hist


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("tag", help="label for the output files")
    ap.add_argument("port", type=int, help="port llm-switch is serving on")
    ap.add_argument("--prompt", default=str(HERE / "prompt.txt"),
                    help="prompt file (build it with make_prompt.py)")
    ap.add_argument("--outdir", type=pathlib.Path, default=HERE / "out")
    ap.add_argument("--rounds", type=int, default=4)
    ap.add_argument("--max-tokens", type=int, default=3000)
    a = ap.parse_args()

    if not pathlib.Path(a.prompt).exists():
        sys.exit(f"prompt not found: {a.prompt}\nBuild it first:\n"
                 f"    python3 {HERE / 'make_prompt.py'} --skill-doc PATH")

    hist = run(a.tag, a.port, a.prompt, a.outdir, a.rounds, a.max_tokens)
    best = max(s for s, _, _ in hist)
    green = next((i for i, (s, t, _) in enumerate(hist) if s == t), None)
    print(f"\n{a.tag}: best={best}/{hist[0][1]} green_at={green} rounds={len(hist)}")


if __name__ == "__main__":
    main()
