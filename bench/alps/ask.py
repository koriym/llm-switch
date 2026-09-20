"""Single-turn chat completion against a local OpenAI-compatible server."""
import json, sys, urllib.request

def ask(port, prompt, max_tokens=3000, timeout=3600, no_think=True):
    payload = {
        "model": "local",
        "messages": [{"role": "user", "content": prompt}],
        "temperature": 0,
        "max_tokens": max_tokens,
        "stream": False,
    }
    if no_think:
        # ds4-server: thinking mode ignores client sampling knobs, so turn it off
        payload["thinking"] = {"type": "disabled"}
        payload["think"] = False
    body = json.dumps(payload).encode()
    req = urllib.request.Request(
        f"http://127.0.0.1:{port}/v1/chat/completions",
        data=body, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        d = json.load(r)
    return d["choices"][0]["message"]["content"]

if __name__ == "__main__":
    port, pfile, ofile = sys.argv[1], sys.argv[2], sys.argv[3]
    mt = int(sys.argv[4]) if len(sys.argv) > 4 else 3000
    txt = ask(int(port), open(pfile).read(), mt)
    open(ofile, "w").write(txt)
    print(f"{ofile}: {len(txt)} bytes")
