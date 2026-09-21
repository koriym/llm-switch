#!/bin/bash
# Ask a model for a test suite, then score it by mutation.
#
#   ./run.sh <port> [outdir]
#
# The model never sees the mutants or the hand-written suite. It sees the
# function, its contract, and nothing else - which is the situation a person
# is in when asked to write tests for existing code.

set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
PORT="${1:?usage: run.sh <port> [outdir]}"
OUT="${2:-/tmp/mutation-bench}"
TASKS="utf8_truncate:utf8_truncate dedup:dedup_sorted lower_bound:lower_bound parse_int:parse_int replace:replace"

mkdir -p "$OUT"
printf '%-16s %-10s %s\n' task result missed

for pair in $TASKS; do
  task="${pair%%:*}"; impl="${pair##*:}"
  src="$HERE/impl/$impl.c"
  prompt="$OUT/$task.prompt.txt"

  {
    echo "Here is a C function. Write a test program for it."
    echo
    echo '```c'
    cat "$src"
    echo '```'
    cat <<'EOF'

Requirements for the test program:
- a single self-contained C file with main()
- declare the function yourself; do not include the implementation
- exit 0 if every check passes, non-zero if any check fails
- use only <stddef.h>, <string.h>, <stdio.h>, <limits.h>, <stdlib.h>
- **at most 20 checks.** Choose them; do not enumerate cases.

The suite will be scored by mutation: single-token changes are made to the
implementation above - comparisons flipped, constants nudged, branches
inverted - and your tests are measured by how many of those altered versions
they reject. Tests that merely execute the code score nothing, and a
twenty-first case cannot rescue a badly chosen twentieth.

Output ONLY the C file in a single code block.
EOF
  } > "$prompt"

  python3 "$HERE/../alps/ask.py" "$PORT" "$prompt" "$OUT/$task.out" 8000 >/dev/null 2>&1

  python3 - "$OUT/$task.out" "$OUT/$task.test.c" <<'PY'
import pathlib, re, sys
txt = pathlib.Path(sys.argv[1]).read_text(errors="replace")
blocks = re.findall(r"```(?:c)?\n(.*?)```", txt, re.S)
code = next((b for b in reversed(blocks) if "main" in b), "")
pathlib.Path(sys.argv[2]).write_text(code)
PY

  if [ ! -s "$OUT/$task.test.c" ]; then
    printf '%-16s %-10s %s\n' "$task" "no-code" "-"
    continue
  fi

  res=$(python3 "$HERE/mutate.py" "$src" "$OUT/$task.test.c" \
          --gold "$HERE/../c-tasks/$task.test.c" -v 2>&1)
  score=$(echo "$res" | grep -o "killed [0-9]*/[0-9]*" | head -1)
  missed=$(echo "$res" | grep -c "survived:")
  printf '%-16s %-10s %s\n' "$task" "${score:-INVALID}" "$missed"
  echo "$res" > "$OUT/$task.score.txt"
done
