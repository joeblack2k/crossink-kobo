#!/usr/bin/env python3
"""Reject credentials and private key material from the tracked public tree."""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path


SENSITIVE_NAMES = re.compile(
    r"(^|/)(\.env(?:\..*)?|id_(?:rsa|dsa|ecdsa|ed25519)|"
    r".*\.(?:pem|p12|pfx|jks|keystore|crt|key))$",
    re.IGNORECASE,
)
SECRET_PATTERNS = (
    ("private key", re.compile(r"-----BEGIN [A-Z0-9 ]*PRIVATE KEY-----")),
    ("GitHub token", re.compile(r"\b(?:gh[ps]_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,})\b")),
    ("OpenAI key", re.compile(r"\bsk-[A-Za-z0-9]{20,}\b")),
    ("AWS access key", re.compile(r"\bAKIA[0-9A-Z]{16}\b")),
    ("Google API key", re.compile(r"\bAIza[0-9A-Za-z_-]{20,}\b")),
)
ASSIGNMENT = re.compile(
    r"(?i)\b(?:api[_-]?key|access[_-]?token|auth[_-]?token|password|passwd|secret|private[_-]?key)"
    r"\b\s*[:=]\s*[\"']([^\"'\r\n]{8,})[\"']"
)
PLACEHOLDER = re.compile(r"(?i)^(?:secret|password|changeme|example|placeholder|"
                          r"<[^>]+>|\$\{[^}]+\}|YOUR_[A-Z0-9_]+)$")


def tracked_files() -> list[Path]:
    result = subprocess.run(
        ["git", "ls-files", "-z"],
        check=True,
        stdout=subprocess.PIPE,
    )
    return [Path(name) for name in result.stdout.decode().split("\0") if name]


def main() -> int:
    findings: list[str] = []
    for relative in tracked_files():
        if SENSITIVE_NAMES.search(relative.as_posix()):
            findings.append(f"{relative}: sensitive filename")
            continue
        path = Path(relative)
        if path.is_dir():
            continue
        try:
            data = path.read_bytes()
        except OSError as error:
            findings.append(f"{relative}: cannot read tracked file: {error}")
            continue
        if b"\0" in data:
            continue
        text = data.decode("utf-8", errors="replace")
        for label, pattern in SECRET_PATTERNS:
            match = pattern.search(text)
            if match:
                findings.append(f"{relative}: {label} pattern at line {text.count(chr(10), 0, match.start()) + 1}")
        for match in ASSIGNMENT.finditer(text):
            if not PLACEHOLDER.fullmatch(match.group(1).strip()):
                findings.append(
                    f"{relative}: credential-like assignment at line "
                    f"{text.count(chr(10), 0, match.start()) + 1}"
                )

    if findings:
        print("Public-tree secrets scan failed:", file=sys.stderr)
        print("\n".join(f"  - {finding}" for finding in findings), file=sys.stderr)
        return 1
    print(f"Public-tree secrets scan passed ({len(tracked_files())} tracked paths checked).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
