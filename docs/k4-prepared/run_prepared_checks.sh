#!/usr/bin/env bash
set -euo pipefail

ROOT="$(git rev-parse --show-toplevel)"
cd "$ROOT"

python3 docs/k4-prepared/apply_prepared_changes.py --phase all

git diff --check

cmake -S platform/kobo -B build/kobo-host -G Ninja \
  -DBUILD_TESTING=ON \
  -DKOBO_BUILD_DISPLAY=OFF \
  -DCROSSINK_ROOT="$ROOT"
cmake --build build/kobo-host --parallel
ctest --test-dir build/kobo-host --output-on-failure

printf '\nPrepared software-only checks passed. ARM/Buildroot and N437 validation remain authoritative.\n'
