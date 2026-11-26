#!/usr/bin/env python3
"""Validates all Mklipper JSON language packs against C++ tr() strings and schema rules."""
import glob
import json
import os
import re
import sys

ROOT = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
I18N_DIR = os.path.join(ROOT, "data", "i18n")
SRC_DIR = os.path.join(ROOT, "src")

PLACEHOLDER_RE = re.compile(r"(%[0-9]+|%s|<b>|</b>|<i>|</i>|<code>|</code>|<br>)")
TR_RE = re.compile(r'\b(?:tr\s*\(|translate\s*\(\s*"[^"]+"\s*,)\s*((?:"(?:[^"\\]|\\.)*"\s*)+)\)')


def unescape_cpp(raw_parts: str) -> str:
    parts = re.findall(r'"((?:[^"\\]|\\.)*)"', raw_parts)
    joined = "".join(parts)
    s = joined.encode("utf-8").decode("unicode_escape")
    return bytes(ord(c) for c in s).decode("utf-8")


def main() -> int:
    files = sorted(glob.glob(os.path.join(I18N_DIR, "*.json")))
    if len(files) < 2:
        print(f"ERROR: Expected at least 2 language packs in {I18N_DIR}, found {len(files)}")
        return 1

    packs = {}
    for path in files:
        with open(path, "r", encoding="utf-8") as fh:
            packs[os.path.basename(path)] = json.load(fh)

    if "en.json" not in packs or "es.json" not in packs:
        print("ERROR: Both es.json and en.json must exist")
        return 1

    cpp_keys = set()
    for cpp in sorted(glob.glob(os.path.join(SRC_DIR, "*.cpp"))):
        with open(cpp, "r", encoding="utf-8") as fh:
            content = fh.read()
        for match in TR_RE.finditer(content):
            cpp_keys.add(unescape_cpp(match.group(1)))

    en_keys = set(packs["en.json"]["strings"].keys())
    errors = 0

    missing_in_en = cpp_keys - en_keys
    if missing_in_en:
        print(f"ERROR: {len(missing_in_en)} C++ strings missing from en.json:")
        for key in sorted(missing_in_en):
            print("  MISSING:", repr(key))
        errors += len(missing_in_en)

    for fname, data in packs.items():
        meta = data.get("_meta", {})
        for field in ("code", "name", "nativeName"):
            if not isinstance(meta.get(field), str) or not meta[field].strip():
                print(f"ERROR: {fname} missing _meta.{field}")
                errors += 1

        sections = data.get("manualSections", [])
        card_counts = [len(s.get("cards", [])) for s in sections]
        if card_counts != [4, 4, 3, 3, 3]:
            print(f"ERROR: {fname} manualSections card counts={card_counts}, expected [4, 4, 3, 3, 3]")
            errors += 1
        for i, sec in enumerate(sections):
            for field in ("icon", "title", "intro"):
                if not sec.get(field, "").strip():
                    print(f"ERROR: {fname} section[{i}].{field} is empty")
                    errors += 1
            for j, card in enumerate(sec.get("cards", [])):
                for field in ("icon", "title", "body"):
                    if not card.get(field, "").strip():
                        print(f"ERROR: {fname} section[{i}].cards[{j}].{field} is empty")
                        errors += 1

        if meta.get("isSource"):
            continue

        strings = data.get("strings", {})
        keys = set(strings.keys())
        if keys != en_keys:
            print(f"ERROR: {fname} key mismatch: missing={len(en_keys - keys)}, extra={len(keys - en_keys)}")
            for m in sorted(en_keys - keys):
                print(f"  {fname} MISSING:", repr(m))
            for e in sorted(keys - en_keys):
                print(f"  {fname} EXTRA:", repr(e))
            errors += 1

        for k, v in strings.items():
            if not isinstance(v, str) or not v.strip():
                print(f"ERROR: {fname} empty translation for {k!r}")
                errors += 1
                continue
            if sorted(PLACEHOLDER_RE.findall(k)) != sorted(PLACEHOLDER_RE.findall(v)):
                print(f"ERROR: {fname} placeholder/HTML mismatch for {k!r} -> {v!r}")
                errors += 1
            if k.startswith(" ") != v.startswith(" "):
                print(f"ERROR: {fname} leading space mismatch for {k!r} -> {v!r}")
                errors += 1

    print(f"Validated {len(packs)} packs ({len(cpp_keys)} C++ strings, {len(en_keys)} JSON keys). Errors: {errors}")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
