#!/bin/bash
# Measure one ds4-agent run on the ALPS task and summarise its trace.
#
#   ./run.sh <outdir> [--resident]
#
# Needs bench/alps/prompt.txt (build it with bench/alps/make_prompt.py) and a
# configured ~/.config/llm-switch/config.

set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ALPS="$HERE/../alps"
CONFIG="${LLM_SWITCH_CONFIG:-${XDG_CONFIG_HOME:-$HOME/.config}/llm-switch/config}"
[ -f "$CONFIG" ] && . "$CONFIG"

DS4_DIR="${DS4_DIR:-$HOME/git/ds4}"
DS4_AGENT_BIN="${DS4_AGENT_BIN:-$DS4_DIR/ds4-agent}"
DS4_CTX="${DS4_CTX:-32768}"

OUT="${1:?usage: run.sh <outdir> [--resident]}"
MODE="${2:-}"
[ -x "$DS4_AGENT_BIN" ] || { echo "ds4-agent not found: $DS4_AGENT_BIN" >&2; exit 1; }
[ -f "$ALPS/prompt.txt" ] || { echo "build $ALPS/prompt.txt first (make_prompt.py)" >&2; exit 1; }

# The rubric lives outside the sandbox so the agent cannot read or rewrite the
# rules it is scored against. Keep it out of /tmp: an agent with a shell tool
# runs `find / -maxdepth 4 -name verify.py`, and mktemp -d lands shallow
# enough to be caught by exactly that. Hiding is best effort - the leak check
# after the run is what actually guarantees the score.
RUBRIC="$(mktemp -d "$HOME/.cache/llm-switch-rubric.XXXXXX")/nested/deeper"
mkdir -p "$RUBRIC"
cp "$ALPS/verify.py" "$RUBRIC/"

rm -rf "$OUT"; mkdir -p "$OUT"
cp "$ALPS/profile.json" "$ALPS/check.py" "$OUT/"
{
  cat "$ALPS/prompt.txt"
  cat <<'EOF'

---
Work in the current directory. Write three files: fake.json, schema.json and
review.sql, with the contents described above. Then run

    python3 check.py

and fix whatever it reports until it prints ALL CHECKS PASSED.
Do not modify check.py or profile.json.
EOF
} > "$OUT/TASK.md"

extra=(--ssd-streaming)
[ "$MODE" = --resident ] && extra=()

echo "mode: $([ "$MODE" = --resident ] && echo resident || echo streaming), ctx $DS4_CTX"
start=$(date +%s)
# The rubric path reaches check.py through the environment. The agent's shell
# tool inherits it and can print it with `env`; it also reads check.py on its
# first move. Neither hiding scheme survives a shell tool, so do not pretend:
# the leak check below is what makes the score mean anything.
( cd "$DS4_DIR" && LLM_SWITCH_RUBRIC="$RUBRIC" "$DS4_AGENT_BIN" \
    -m "$DS4_MODEL" --ctx "$DS4_CTX" "${extra[@]}" \
    --chdir "$OUT" --non-interactive --prompt-file "$OUT/TASK.md" \
    --nothink --trace "$OUT/trace.log" ) > "$OUT/agent.log" 2>&1
end=$(date +%s)

echo "wall: $((end - start)) s"
echo "tool calls: $(grep -c '🛠️' "$OUT/agent.log")"
# Detection, not concealment, is what makes the score trustworthy: the agent
# reads check.py on its first move, so it can always learn where the rubric
# is. Match the filename *and* the directory - `cat $RUBRIC/*` never prints
# "verify.py" and would otherwise pass as clean. Flag mentions, then judge:
# noticing the path is not the same as reading the file.
pat="verify2?\.py|$RUBRIC"
leaks=$(grep -cE "$pat" "$OUT/agent.log")
if [ "$leaks" -gt 0 ]; then
  echo "RUBRIC TOUCHED: $leaks mention(s) - inspect before trusting the score"
  grep -nE "$pat" "$OUT/agent.log" | head -8 | sed 's/^/    /'
else
  echo "rubric mentions: 0"
fi
echo
python3 "$HERE/trace-stats.py" "$OUT/trace.log"
echo
echo "--- final score ---"
# Same path, now that the agent is gone and cannot see it.
( cd "$OUT" && LLM_SWITCH_RUBRIC="$RUBRIC" python3 check.py | tail -3 )
rm -rf "$(dirname "$(dirname "$RUBRIC")")"
