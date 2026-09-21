#!/usr/bin/env python3
"""Summarise a ds4-agent --trace log.

Reports what an agent run actually costs: how much of each round's prompt came
from cache, how long prefill and decode took, and how many tool round trips
there were.

This measures one engine's behaviour. It is not a comparison: two agents on
the same task issue different numbers of tool calls, so wall-clock totals
between different agent implementations say more about the agent than about
the model or the runtime.
"""
import argparse, pathlib, re, sys

PREFILL = re.compile(
    r"prefill tool_round=(\d+) transcript=(\d+) prompt=(\d+) cached=(\d+) suffix=(\d+)")
PREFILL_DONE = re.compile(
    r"prefill sync done tool_round=(\d+).*?([\d.]+) ms\s*$")
GEN_DONE = re.compile(r"generation finished tool_round=(\d+) generated=(\d+)")
SYSPROMPT = re.compile(r"sysprompt kv (hit|miss).*?tokens=(\d+)")
TOKEN = re.compile(r"^(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d\.\d+) token index=(\d+)")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("trace", type=pathlib.Path)
    a = ap.parse_args()
    if not a.trace.exists():
        sys.exit(f"no such trace: {a.trace}")

    rounds, prefill_ms, generated, sysprompt = {}, {}, {}, None
    tok_first = tok_last = None
    tok_count = 0

    for line in a.trace.read_text(errors="replace").splitlines():
        if (m := PREFILL.search(line)):
            r = int(m.group(1))
            rounds[r] = dict(prompt=int(m.group(3)), cached=int(m.group(4)),
                             suffix=int(m.group(5)))
        elif (m := PREFILL_DONE.search(line)):
            prefill_ms[int(m.group(1))] = float(m.group(2))
        elif (m := GEN_DONE.search(line)):
            generated[int(m.group(1))] = int(m.group(2))
        elif (m := SYSPROMPT.search(line)):
            sysprompt = (m.group(1), int(m.group(2)))
        elif (m := TOKEN.match(line)):
            tok_count += 1
            tok_last = m.group(1)
            if tok_first is None:
                tok_first = m.group(1)

    if not rounds:
        sys.exit("no prefill records found; was --trace passed to ds4-agent?")

    if sysprompt:
        print(f"system prompt kv {sysprompt[0]}: {sysprompt[1]} tokens")
    print(f"tool rounds: {len(rounds)}\n")
    print(f"{'round':>5} {'prompt':>8} {'cached':>8} {'prefilled':>10} "
          f"{'reuse':>6} {'prefill s':>10} {'generated':>10}")
    tot_pref = tot_suffix = tot_gen = 0
    for r in sorted(rounds):
        d = rounds[r]
        ms = prefill_ms.get(r, 0.0)
        reuse = d["cached"] / d["prompt"] * 100 if d["prompt"] else 0
        tot_pref += ms
        tot_suffix += d["suffix"]
        tot_gen += generated.get(r, 0)
        print(f"{r:>5} {d['prompt']:>8} {d['cached']:>8} {d['suffix']:>10} "
              f"{reuse:>5.0f}% {ms/1000:>10.1f} {generated.get(r, 0):>10}")

    print(f"\nprefilled tokens : {tot_suffix}")
    print(f"prefill time     : {tot_pref/1000:.1f} s"
          + (f"  ({tot_suffix/(tot_pref/1000):.1f} tok/s)" if tot_pref else ""))
    print(f"generated tokens : {tot_gen}")
    if tok_first and tok_last:
        import datetime
        fmt = "%Y-%m-%d %H:%M:%S.%f"
        span = (datetime.datetime.strptime(tok_last, fmt)
                - datetime.datetime.strptime(tok_first, fmt)).total_seconds()
        decode_s = span - tot_pref / 1000
        print(f"span             : {span:.1f} s")
        if decode_s > 0:
            print(f"decode           : {tot_gen / decode_s:.1f} tok/s "
                  f"(span minus prefill)")
    else:
        print("span             : n/a (per-token lines stripped from this trace)")

    naive = sum(r["prompt"] for r in rounds.values())
    if naive:
        print(f"\nWithout cache reuse the same rounds would have prefilled "
              f"{naive} tokens instead of {tot_suffix} "
              f"({naive / max(tot_suffix, 1):.1f}x).")


if __name__ == "__main__":
    main()
