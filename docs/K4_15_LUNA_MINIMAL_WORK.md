# Luna Max minimal-work checklist

## Software-only sequence

1. Read the K4 documents and record the baseline.
2. Commit the documentation bundle separately.
3. Run the prepared applier in check mode.
4. Apply `touch-registry`, inspect, format, run host tests, commit.
5. Apply `touch-gestures`, run host tests and commit.
6. Apply `evdev-hardening`, run the parser/gesture tests, finish the raw-event-to-activity E2E test and commit.
7. Apply `loop-power`, measure main-loop stalls, commit only after regression tests.
8. Apply `suspend-resume`, inspect the transition/render ordering and commit.
9. Apply `display-recovery`, run mapping tests and an ARM build; keep hardware rows PARTIAL until N437 fault-injection proof.
10. Apply `websocket-atomic`, add EPUB and font failure-injection tests, commit.
11. Apply `ci`, push and verify the first workflow run.
12. Continue only with the explicitly unprepared issues listed in the manifest.

## Commands

```bash
python3 docs/k4-prepared/apply_prepared_changes.py --list
python3 docs/k4-prepared/apply_prepared_changes.py --phase all
./bin/clang-format-fix
git diff --check
```

For native tests after source application:

```bash
cmake -S platform/kobo -B build/kobo-host -G Ninja   -DBUILD_TESTING=ON   -DKOBO_BUILD_DISPLAY=OFF   -DCROSSINK_ROOT="$PWD"
cmake --build build/kobo-host --parallel
ctest --test-dir build/kobo-host --output-on-failure
```

## What Luna must still decide

Luna remains responsible for resolving compiler errors, checking all callsites,
writing the remaining routing-E2E/failure-injection tests, completing web
security and network-worker issues, physically validating the prepared display/suspend paths, and validating the exact
ARM binary on the N437. The prepared code is a reviewed starting implementation,
not fabricated build or hardware evidence.
