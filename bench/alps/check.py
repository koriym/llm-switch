#!/usr/bin/env python3
"""Grade fake.json / schema.json / review.sql in the current directory.

Use this when an agent produces files rather than a single response.

An agent with a shell tool can read - and rewrite - any grader it can reach,
so when grading an agent, copy verify.py somewhere outside the agent's working
directory and point LLM_SWITCH_RUBRIC at it. Afterwards, check the agent's log
for reads of that path before trusting the score.
"""
import os, pathlib, sys

HERE = pathlib.Path(__file__).parent.resolve()
EXPLICIT = "LLM_SWITCH_RUBRIC" in os.environ
RUBRIC = pathlib.Path(os.environ.get("LLM_SWITCH_RUBRIC", HERE)).resolve()

# Python always puts this script's own directory on sys.path, so a wrong
# LLM_SWITCH_RUBRIC would quietly import the co-located copy and report a
# score while the rubric was reachable by the agent all along. Check the path
# first, then confirm the module that actually loaded came from it.
if EXPLICIT and not (RUBRIC / "verify.py").is_file():
    sys.exit(f"LLM_SWITCH_RUBRIC={RUBRIC} contains no verify.py")

sys.path.insert(0, str(RUBRIC))
try:
    import verify
except ImportError:
    sys.exit(f"verify.py not found in {RUBRIC}")

loaded = pathlib.Path(verify.__file__).resolve()
if EXPLICIT and loaded.parent != RUBRIC:
    sys.exit(f"rubric isolation failed: imported {loaded}, expected it under {RUBRIC}")

WANT = ["fake.json", "schema.json", "review.sql"]


def main():
    missing = [f for f in WANT if not (HERE / f).exists()]
    if missing:
        print("MISSING FILES: " + ", ".join(missing))
        print("Create all three, then run this script again.")
        return 1

    combined = "\n".join(
        "```\n" + (HERE / f).read_text().strip() + "\n```" for f in WANT)
    tmp = HERE / ".combined.txt"
    tmp.write_text(combined)
    try:
        score, total = verify.report("artifacts", str(tmp))
    finally:
        tmp.unlink(missing_ok=True)

    if score == total:
        print("\nALL CHECKS PASSED")
        return 0
    print(f"\n{total - score} CHECK(S) FAILED - fix the files and run check.py again")
    return 1


if __name__ == "__main__":
    sys.exit(main())
