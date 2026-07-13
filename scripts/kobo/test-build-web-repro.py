#!/usr/bin/env python3
"""Verify that the generated web headers are byte-stable across invocations."""

from __future__ import annotations

import hashlib
import pathlib
import subprocess
import sys


ROOT = pathlib.Path(__file__).resolve().parents[2]
GEN_DIR = ROOT / "src" / "network" / "html"


def generated_hashes() -> dict[str, str]:
    return {
        path.relative_to(GEN_DIR).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in sorted(GEN_DIR.rglob("*.generated.h"))
    }


def build() -> dict[str, str]:
    subprocess.run([sys.executable, "scripts/build_web.py"], cwd=ROOT, check=True)
    hashes = generated_hashes()
    if not hashes:
        raise RuntimeError("web generator produced no headers")
    return hashes


first = build()
second = build()
if first != second:
    changed = sorted(set(first) | set(second))
    changed = [name for name in changed if first.get(name) != second.get(name)]
    raise SystemExit(f"generated web headers are not reproducible: {', '.join(changed)}")

print(f"PASS: {len(first)} generated web headers are byte-stable")
