#!/usr/bin/env python3
"""Build the ALPS benchmark prompt.

The conversion rules live in a third-party skill document, which is not
vendored here; pass its path with --skill-doc. Without one the prompt still
works, it just stops testing whether the model can follow a supplied document.
"""
import argparse, pathlib, sys

HERE = pathlib.Path(__file__).parent

BODY = """Here is the ALPS profile to convert:

{alps}

Produce exactly three fenced code blocks, in this order, and nothing else.

Block 1 - fake data. A JSON array of exactly 6 realistic Review objects.
  Keys are the ALPS descriptor ids. Every object has all 10 fields.
  Make the values realistic and varied in length, not placeholders.

Block 2 - JSON Schema, draft 2020-12, for ONE Review object.
  - "$schema", "type": "object", "additionalProperties": false
  - "required" listing all 10 fields
  - every string field has "minLength" and "maxLength" derived from the data in
    Block 1 (maxLength about 1.5 to 2 times the longest value you generated)
  - reviewerEmail has "format": "email"; sourceUrl has "format": "uri";
    createdAt has "format": "date-time"
  - rating has "minimum": 1 and "maximum": 5; helpfulCount has "minimum": 0
  The schema MUST accept every object in Block 1 and MUST reject a rating of 6,
  an unknown extra property, and a missing required field.

Block 3 - SQLite DDL for table `review`, following the naming, primary key,
  NOT NULL, default and index rules{skill_ref}. In addition:
  - add a CHECK constraint restricting rating to 1..5
  - add a CHECK constraint restricting helpful_count to >= 0
  Output CREATE TABLE and any CREATE INDEX statements. No comments.

Output only the three code blocks.
"""

PREAMBLE = """Below is a skill document describing how to convert an ALPS profile into SQL DDL.
Read it and follow its conversion rules exactly.

===== SKILL DOCUMENT =====
{skill}
===== END SKILL DOCUMENT =====

"""


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--skill-doc", type=pathlib.Path,
                    help="path to the alps-to-sql SKILL.md to embed")
    ap.add_argument("--profile", type=pathlib.Path, default=HERE / "profile.json")
    ap.add_argument("-o", "--out", type=pathlib.Path, default=HERE / "prompt.txt")
    a = ap.parse_args()

    if not a.profile.exists():
        sys.exit(f"profile not found: {a.profile}")

    preamble = ""
    skill_ref = ""
    if a.skill_doc:
        if not a.skill_doc.exists():
            sys.exit(f"skill document not found: {a.skill_doc}")
        preamble = PREAMBLE.format(skill=a.skill_doc.read_text())
        skill_ref = " from the skill document above"

    text = preamble + BODY.format(alps=a.profile.read_text(), skill_ref=skill_ref)
    a.out.write_text(text)
    print(f"{a.out}: {len(text)} bytes"
          + (f" (skill document embedded: {a.skill_doc})" if a.skill_doc else " (no skill document)"))


if __name__ == "__main__":
    main()
