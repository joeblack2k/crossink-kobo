#!/usr/bin/env python3
"""Fail a Kobo build when active objects retain ESP-only dependencies.

The audit deliberately consumes CMake's compile_commands.json instead of
grepping the source tree.  CrossInk still contains ESP32 implementations, and
some shared sources contain ESP-only branches guarded by ``#ifndef
KOBO_LINUX``.  Those must not make an otherwise valid Kobo object fail.

For every actual Kobo translation unit this script runs the recorded compiler
in preprocess-only mode.  Line markers preserve the active source/header that
introduced a prohibited token, so inactive branches are ignored.  The one
temporary native-web compatibility header is listed in
docs/kobo-compat-exceptions.txt and is reported as a warning, never silently
ignored.  Remove that exception together with PLAT-01's vendor replacement.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shlex
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


FORBIDDEN_TOKENS: tuple[tuple[str, re.Pattern[str]], ...] = (
    ("esp_now", re.compile(r"\besp_now(?:_[A-Za-z0-9_]+)?\b")),
    ("OTA partitions", re.compile(r"\besp_(?:partition|ota)(?:_[A-Za-z0-9_]+)?\b")),
    ("Preferences", re.compile(r"\bPreferences\b")),
    ("esp_deep_sleep", re.compile(r"\besp_deep_sleep(?:_[A-Za-z0-9_]+)?\b")),
    ("ESP.restart", re.compile(r"\bESP\s*\.\s*restart\s*\(")),
)
FORBIDDEN_HEADER_NAMES = {"WebServer.h", "ESPAsyncWebServer.h", "AsyncWebServer.h"}
LINE_MARKER = re.compile(r'^#\s*\d+\s+"([^"]+)"')
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^">]+)[">]')
RAW_CANDIDATE = re.compile(
    r"\besp_(?:now|partition|ota|deep_sleep)\b|\bPreferences\b|\bESP\s*\.\s*restart\b|\b(?:ESPAsync)?WebServer\b"
)


@dataclass(frozen=True)
class Violation:
    source: Path
    origin: Path
    label: str
    detail: str


def normal(path: str | Path) -> Path:
    return Path(path).expanduser().resolve()


def is_within(path: Path, root: Path) -> bool:
    try:
        path.relative_to(root)
        return True
    except ValueError:
        return False


def is_auditable_source(path: Path, root: Path) -> bool:
    """Third-party decoders are not platform adapters; app and compat code are."""
    return any(
        is_within(path, candidate)
        for candidate in (
            root / "src",
            root / "lib",
            root / "platform/kobo",
            root / "vendor/crosspoint-simulator",
        )
    )


def parse_allowlist(path: Path, root: Path) -> dict[Path, str]:
    """Read ``relative/path | reason`` entries, refusing undocumented paths."""
    allowed: dict[Path, str] = {}
    if not path.exists():
        return allowed
    for number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if "|" not in line:
            raise ValueError(f"{path}:{number}: expected 'path | temporary reason'")
        relative, reason = (part.strip() for part in line.split("|", 1))
        if not relative or not reason:
            raise ValueError(f"{path}:{number}: exception needs both path and reason")
        candidate = normal(root / relative)
        if not is_within(candidate, root):
            raise ValueError(f"{path}:{number}: exception leaves source root")
        allowed[candidate] = reason
    return allowed


def command_arguments(entry: dict[str, object], source: Path) -> list[str]:
    arguments = entry.get("arguments")
    if isinstance(arguments, list):
        args = [str(arg) for arg in arguments]
    else:
        command = entry.get("command")
        if not isinstance(command, str):
            raise ValueError(f"{source}: compile command lacks arguments/command")
        args = shlex.split(command)
    if not args:
        raise ValueError(f"{source}: empty compiler command")
    return args


def preprocess_command(arguments: list[str]) -> list[str]:
    """Convert a CMake compile command into a dependency-preserving ``-E`` run."""
    result: list[str] = []
    skip_next = False
    paired = {"-o", "-MF", "-MT", "-MQ"}
    for argument in arguments:
        if skip_next:
            skip_next = False
            continue
        if argument in paired:
            skip_next = True
            continue
        if argument in {"-c", "-MD", "-MMD"}:
            continue
        if argument.startswith("-o") and argument != "-o":
            continue
        if argument.startswith("-MF") or argument.startswith("-MT") or argument.startswith("-MQ"):
            continue
        result.append(argument)
    result.append("-E")
    return result


def include_directories(arguments: list[str], directory: Path) -> list[Path]:
    """Extract local include roots from the exact CMake compiler invocation."""
    result: list[Path] = []
    index = 0
    while index < len(arguments):
        argument = arguments[index]
        value: str | None = None
        if argument in {"-I", "-isystem"} and index + 1 < len(arguments):
            index += 1
            value = arguments[index]
        elif argument.startswith("-I") and len(argument) > 2:
            value = argument[2:]
        elif argument.startswith("-isystem") and len(argument) > len("-isystem"):
            value = argument[len("-isystem") :]
        if value:
            candidate = Path(value)
            result.append(normal(candidate if candidate.is_absolute() else directory / candidate))
        index += 1
    return result


def resolve_include(name: str, origin: Path, include_dirs: list[Path], root: Path) -> Path | None:
    for directory in [origin.parent, *include_dirs]:
        candidate = normal(directory / name)
        if candidate.is_file() and is_within(candidate, root):
            return candidate
    return None


def raw_candidate_origins(
    source: Path,
    include_dirs: list[Path],
    root: Path,
    memo: dict[Path, frozenset[Path]],
    visiting: set[Path],
) -> frozenset[Path]:
    """Find raw-risk files reachable from a translation unit.

    Multiple source files include ActivityManager.h, which in turn reaches the
    native transfer API.  Preprocessing every one would repeatedly audit the
    same headers.  The caller selects one representative target object per
    raw-risk origin; Kobo's CMake target gives all those objects the same
    KOBO_LINUX definitions.
    """
    if source in memo:
        return memo[source]
    if source in visiting:
        return frozenset()
    visiting.add(source)
    try:
        text = source.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        visiting.remove(source)
        return frozenset()
    found: set[Path] = set()
    if source.name in FORBIDDEN_HEADER_NAMES or RAW_CANDIDATE.search(text):
        found.add(source)
    for line in text.splitlines():
        match = INCLUDE.match(line)
        if not match:
            continue
        dependency = resolve_include(match.group(1), source, include_dirs, root)
        if dependency:
            found.update(raw_candidate_origins(dependency, include_dirs, root, memo, visiting))
    visiting.remove(source)
    resolved = frozenset(found)
    memo[source] = resolved
    return resolved


def active_origins(preprocessed: str, source: Path, root: Path) -> Iterable[tuple[Path, str]]:
    """Yield active project lines only; system headers cannot create Kobo debt."""
    current = source
    for line in preprocessed.splitlines():
        marker = LINE_MARKER.match(line)
        if marker:
            current = normal(marker.group(1))
            continue
        if is_within(current, root):
            yield current, line


def inspect_translation_unit(
    entry: dict[str, object], root: Path, allowed: dict[Path, str], timeout: int
) -> tuple[list[Violation], set[Path]]:
    directory = normal(str(entry.get("directory", root)))
    source = normal(directory / str(entry["file"])) if not os.path.isabs(str(entry["file"])) else normal(str(entry["file"]))
    if not is_auditable_source(source, root):
        return [], set()
    completed = subprocess.run(
        preprocess_command(command_arguments(entry, source)),
        cwd=directory,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout,
        check=False,
    )
    if completed.returncode != 0:
        error = completed.stderr.strip().splitlines()[-1] if completed.stderr.strip() else "no compiler diagnostic"
        return [Violation(source, source, "preprocessor", error)], set()

    violations: list[Violation] = []
    used_exceptions: set[Path] = set()
    seen_headers: set[Path] = set()
    for origin, line in active_origins(completed.stdout, source, root):
        if origin.name in FORBIDDEN_HEADER_NAMES:
            seen_headers.add(origin)
        for label, pattern in FORBIDDEN_TOKENS:
            if pattern.search(line):
                violations.append(Violation(source, origin, label, line.strip()[:180]))

    for header in seen_headers:
        if header in allowed:
            used_exceptions.add(header)
        else:
            violations.append(Violation(source, header, "ESP WebServer header", header.name))
    return violations, used_exceptions


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compile-commands", required=True, type=Path)
    parser.add_argument("--source-root", required=True, type=Path)
    parser.add_argument(
        "--exceptions",
        type=Path,
        default=None,
        help="temporary documented compatibility headers (default: docs/kobo-compat-exceptions.txt)",
    )
    parser.add_argument("--timeout", type=int, default=90, help="per-translation-unit preprocessing limit in seconds")
    parser.add_argument("--list-candidates", action="store_true", help="list the conservative preprocessing set and exit")
    args = parser.parse_args()

    root = normal(args.source_root)
    database = normal(args.compile_commands)
    exceptions = normal(args.exceptions) if args.exceptions else root / "docs/kobo-compat-exceptions.txt"
    if not database.is_file():
        parser.error(f"compile command database does not exist: {database}")
    try:
        entries = json.loads(database.read_text(encoding="utf-8"))
        if not isinstance(entries, list):
            raise ValueError("top level must be a JSON array")
        allowed = parse_allowlist(exceptions, root)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        parser.error(str(error))

    selected = [entry for entry in entries if isinstance(entry, dict) and "file" in entry]
    violations: list[Violation] = []
    used_exceptions: set[Path] = set()
    discovered = 0
    audited = 0
    candidates: list[dict[str, object]] = []
    memo: dict[Path, frozenset[Path]] = {}
    covered_origins: set[Path] = set()
    for entry in selected:
        source_text = str(entry["file"])
        source = normal(Path(str(entry.get("directory", root))) / source_text) if not os.path.isabs(source_text) else normal(source_text)
        if not is_auditable_source(source, root):
            continue
        discovered += 1
        try:
            include_dirs = include_directories(command_arguments(entry, source), normal(str(entry.get("directory", root))))
        except ValueError as error:
            violations.append(Violation(source, source, "compile command", str(error)))
            continue
        origins = raw_candidate_origins(source, include_dirs, root, memo, set())
        if origins.difference(covered_origins):
            candidates.append(entry)
            covered_origins.update(origins)

    if args.list_candidates:
        print(f"Kobo dependency audit: discovered {discovered} Kobo objects; {len(candidates)} dependency candidates")
        for entry in candidates:
            source_text = str(entry["file"])
            source = normal(Path(str(entry.get("directory", root))) / source_text) if not os.path.isabs(source_text) else normal(source_text)
            print(source.relative_to(root))
        return 0

    for entry in candidates:
        source_text = str(entry["file"])
        source = normal(Path(str(entry.get("directory", root))) / source_text) if not os.path.isabs(source_text) else normal(source_text)
        audited += 1
        try:
            found, used = inspect_translation_unit(entry, root, allowed, args.timeout)
        except (OSError, ValueError, subprocess.TimeoutExpired) as error:
            violations.append(Violation(source, source, "preprocessor", str(error)))
            continue
        violations.extend(found)
        used_exceptions.update(used)

    if discovered == 0:
        print(f"Kobo dependency audit: no Kobo source objects in {database}", file=sys.stderr)
        return 2

    print(f"Kobo dependency audit: discovered {discovered} Kobo objects; preprocessed {audited} dependency candidates")
    for header in sorted(used_exceptions):
        print(f"TEMPORARY COMPAT HEADER: {header.relative_to(root)} — {allowed[header]}")
    if violations:
        print("Kobo dependency audit FAILED:", file=sys.stderr)
        for violation in violations:
            try:
                source = violation.source.relative_to(root)
                origin = violation.origin.relative_to(root)
            except ValueError:
                source, origin = violation.source, violation.origin
            print(f"  {source}: active {violation.label} from {origin}: {violation.detail}", file=sys.stderr)
        return 1
    print("Kobo dependency audit PASS: no active ESP-only dependency escaped into the target.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
