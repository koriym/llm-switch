"""Strict verifier with negative tests.

Positive checks alone let a permissive artifact score full marks, so every
constraint that matters is probed with a record that must be REJECTED.
"""
import json, re, sqlite3, pathlib, datetime

FIELDS = ["reviewId","bookIsbn","reviewerEmail","headline","body",
          "rating","helpfulCount","isVerifiedPurchase","sourceUrl","createdAt"]
STRINGS = ["reviewId","bookIsbn","reviewerEmail","headline","body","sourceUrl","createdAt"]

def snake(s):
    return re.sub(r"(?<!^)(?=[A-Z])", "_", s).lower()

COLS = [snake(f) for f in FIELDS]

def blocks(txt):
    return re.findall(r"```[a-zA-Z]*\n(.*?)```", txt, re.S)

def classify(bs):
    fake = schema = ddl = None
    for b in bs:
        t = b.strip()
        if re.search(r"create\s+(table|index)", t, re.I):
            if ddl is None: ddl = t
            continue
        try:
            v = json.loads(t)
        except Exception:
            continue
        if isinstance(v, list) and fake is None:
            fake = v
        elif isinstance(v, dict) and schema is None:
            schema = v
    return fake, schema, ddl


def report(name, path):
    txt = pathlib.Path(path).read_text(errors="replace")
    fake, schema, ddl = classify(blocks(txt))
    passed, items = [], []

    def chk(label, cond, note=""):
        items.append((label, bool(cond), note))

    # ---------- fake data ----------
    chk("fake: JSON array of 6", isinstance(fake, list) and len(fake) == 6)
    chk("fake: exact 10 ids per record",
        isinstance(fake, list) and all(isinstance(r, dict) and set(r) == set(FIELDS) for r in fake))
    ok = False
    if isinstance(fake, list) and fake and all(isinstance(r, dict) for r in fake):
        try:
            ok = all(isinstance(r["rating"], int) and 1 <= r["rating"] <= 5
                     and isinstance(r["isVerifiedPurchase"], bool)
                     and isinstance(r["helpfulCount"], int) and r["helpfulCount"] >= 0
                     for r in fake)
        except KeyError:
            ok = False
    chk("fake: scalar types sane", ok)
    ok = False
    if isinstance(fake, list) and fake:
        try:
            ok = all(re.match(r"[^@\s]+@[^@\s]+\.[^@\s]+$", r["reviewerEmail"])
                     and re.match(r"https?://", r["sourceUrl"])
                     and datetime.datetime.fromisoformat(r["createdAt"].replace("Z", "+00:00"))
                     for r in fake)
        except Exception:
            ok = False
    chk("fake: email/url/datetime well-formed", ok)
    ok = False
    if isinstance(fake, list) and fake:
        try:
            bodies = {len(r["body"]) for r in fake}
            ok = len(bodies) >= 4 and max(bodies) >= 40
        except Exception:
            ok = False
    chk("fake: bodies varied, not placeholders", ok,
        "needs >=4 distinct body lengths and one >=40 chars")

    # ---------- schema ----------
    V = None
    try:
        from jsonschema import Draft202012Validator, FormatChecker
        Draft202012Validator.check_schema(schema)
        V = Draft202012Validator(schema, format_checker=FormatChecker())
        chk("schema: valid draft 2020-12", True)
    except Exception as e:
        chk("schema: valid draft 2020-12", False, f"{type(e).__name__}: {str(e)[:100]}")

    chk("schema: accepts all 6 records",
        V is not None and isinstance(fake, list)
        and all(not list(V.iter_errors(r)) for r in fake))

    props = (schema or {}).get("properties", {}) if isinstance(schema, dict) else {}
    chk("schema: additionalProperties false",
        isinstance(schema, dict) and schema.get("additionalProperties") is False)
    chk("schema: required lists all 10",
        isinstance(schema, dict) and set(schema.get("required", [])) == set(FIELDS))
    chk("schema: every string has min/maxLength",
        all(isinstance(props.get(f), dict)
            and "minLength" in props[f] and "maxLength" in props[f] for f in STRINGS))
    ok = False
    if isinstance(fake, list) and fake and props:
        try:
            ok = all(props[f]["maxLength"] >= max(len(r[f]) for r in fake) for f in STRINGS)
        except Exception:
            ok = False
    chk("schema: maxLength >= observed longest", ok)
    ok = False
    if isinstance(fake, list) and fake and props:
        try:
            ok = all(props[f]["maxLength"] <= 4 * max(max(len(r[f]) for r in fake), 8)
                     for f in STRINGS)
        except Exception:
            ok = False
    chk("schema: maxLength not arbitrary (<=4x observed)", ok,
        "Semantic-Ex says 1.5-2x observed, 4x is a generous ceiling")
    chk("schema: formats declared",
        props.get("reviewerEmail", {}).get("format") == "email"
        and props.get("sourceUrl", {}).get("format") == "uri"
        and props.get("createdAt", {}).get("format") == "date-time")
    chk("schema: rating/count bounds",
        props.get("rating", {}).get("minimum") == 1
        and props.get("rating", {}).get("maximum") == 5
        and props.get("helpfulCount", {}).get("minimum") == 0)

    # ---------- schema negative tests ----------
    def rejects(mutate, label):
        if V is None or not isinstance(fake, list) or not fake:
            chk(label, False, "no usable schema/data"); return
        bad = json.loads(json.dumps(fake[0]))
        mutate(bad)
        chk(label, bool(list(V.iter_errors(bad))))

    rejects(lambda r: r.__setitem__("rating", 6), "NEG schema: rejects rating=6")
    rejects(lambda r: r.__setitem__("rating", 0), "NEG schema: rejects rating=0")
    rejects(lambda r: r.__setitem__("helpfulCount", -1), "NEG schema: rejects negative count")
    rejects(lambda r: r.__setitem__("bogusField", "x"), "NEG schema: rejects unknown property")
    rejects(lambda r: r.pop("headline"), "NEG schema: rejects missing required")
    rejects(lambda r: r.__setitem__("reviewerEmail", "not-an-email"),
            "NEG schema: rejects malformed email")

    # ---------- sql ----------
    con = None
    try:
        con = sqlite3.connect(":memory:")
        con.executescript(ddl)
        chk("sql: DDL executes", True)
    except Exception as e:
        chk("sql: DDL executes", False, f"{type(e).__name__}: {str(e)[:120]}")
        con = None

    have = []
    if con:
        try:
            have = [r[1] for r in con.execute("PRAGMA table_info(review)")]
        except Exception:
            pass
    chk("sql: columns are snake_case ids", set(have) == set(COLS),
        f"missing={sorted(set(COLS)-set(have))} extra={sorted(set(have)-set(COLS))}" if have else "")

    pk = []
    notnull = set()
    if con and have:
        info = list(con.execute("PRAGMA table_info(review)"))
        pk = [r[1] for r in info if r[5]]
        notnull = {r[1] for r in info if r[3]}
    chk("sql: review_id is PRIMARY KEY", pk == ["review_id"])
    # SQLite does not imply NOT NULL for non-INTEGER PRIMARY KEY columns, and the
    # skill document's own examples omit it, so PK columns are not penalised here.
    chk("sql: core data columns NOT NULL",
        {"headline", "rating", "body"} <= notnull)
    chk("sql: created_at has default",
        con is not None and bool([r for r in con.execute("PRAGMA table_info(review)")
                                  if r[1] == "created_at" and r[4]]) if con and have else False)
    idx = []
    if con:
        try:
            idx = [r[0] for r in con.execute(
                "SELECT sql FROM sqlite_master WHERE type='index' AND sql IS NOT NULL")]
        except Exception:
            pass
    chk("sql: index on created_at", any("created_at" in (s or "") for s in idx))

    inserted = False
    if con and set(have) == set(COLS) and isinstance(fake, list):
        try:
            ph = ",".join("?" * len(COLS))
            con.executemany(f"INSERT INTO review ({','.join(COLS)}) VALUES ({ph})",
                            [[r[f] for f in FIELDS] for r in fake])
            con.commit()
            inserted = con.execute("SELECT COUNT(*) FROM review").fetchone()[0] == 6
        except Exception as e:
            chk("sql: 6 fake records INSERT", False, f"{type(e).__name__}: {str(e)[:100]}")
    if inserted or not con:
        chk("sql: 6 fake records INSERT", inserted)

    # ---------- sql negative tests ----------
    def sql_rejects(col, value, label):
        if not (con and inserted):
            chk(label, False, "no loaded table"); return
        row = dict(zip(COLS, [fake[0][f] for f in FIELDS]))
        row["review_id"] = "neg-" + col + str(value)
        row[col] = value
        try:
            con.execute(f"INSERT INTO review ({','.join(COLS)}) VALUES ({','.join('?'*len(COLS))})",
                        [row[c] for c in COLS])
            con.rollback()
            chk(label, False, "row was accepted")
        except sqlite3.IntegrityError:
            chk(label, True)
        except Exception as e:
            chk(label, False, f"{type(e).__name__}")

    sql_rejects("rating", 6, "NEG sql: CHECK rejects rating=6")
    sql_rejects("rating", 0, "NEG sql: CHECK rejects rating=0")
    sql_rejects("helpful_count", -1, "NEG sql: CHECK rejects negative count")
    if con and inserted:
        try:
            dup = [fake[0][f] for f in FIELDS]
            con.execute(f"INSERT INTO review ({','.join(COLS)}) VALUES ({','.join('?'*len(COLS))})", dup)
            con.rollback()
            chk("NEG sql: PK rejects duplicate id", False, "duplicate accepted")
        except sqlite3.IntegrityError:
            chk("NEG sql: PK rejects duplicate id", True)
    else:
        chk("NEG sql: PK rejects duplicate id", False, "no loaded table")

    score = sum(1 for _, ok, _ in items if ok)
    print(f"\n======== {name} ({len(txt)} bytes) ========")
    for label, ok, note in items:
        print(f"  {'ok  ' if ok else 'FAIL'} {label}" + (f"   [{note}]" if note and not ok else ""))
    print(f"  --> {score}/{len(items)}")
    return score, len(items)


if __name__ == "__main__":
    import sys
    files = sys.argv[1:]
    if not files:
        sys.exit(f"usage: {pathlib.Path(sys.argv[0]).name} FILE [FILE ...]\n"
                 "Each FILE is a model response containing three fenced blocks:\n"
                 "fake data, JSON Schema, SQLite DDL.")
    results = {}
    for path in files:
        if not pathlib.Path(path).exists():
            print(f"skipping missing file: {path}")
            continue
        results[path] = report(pathlib.Path(path).name, path)
    if len(results) > 1:
        print("\n=== summary ===")
        for k, (s, t) in results.items():
            print(f"{k:32} {s}/{t}")
    sys.exit(0 if results and all(s == t for s, t in results.values()) else 1)
