# Prepared-code bundle

## Baseline

This bundle targets commit:

```text
bbe2f05d4a587d55fa2e6391f9825e376122a76f
```

A documentation-only commit on top of that baseline is supported. The applier
refuses to continue when the baseline is not an ancestor or when an expected
source fragment no longer matches.

## Prepared phases

| Phase | Prepared code | Status before Luna validation |
|---|---|---|
| `touch-registry` | Atomic staging/commit registry, generation rejection, transition invalidation, registry tests | Code prepared; host/ARM/N437 validation required |
| `touch-gestures` | Pointer capture, swipe precedence, semantic long-press, explicit cancelled deadband, book-card long-press, gesture tests | Code prepared; E2E routing test still required |
| `evdev-hardening` | Monotonic event timestamps, `SYN_DROPPED` recovery, typed read results, reconnect/back-off, suspend/orientation reset, parser test and one-complete-gesture-per-frame backpressure | Code prepared; raw-evdev-to-activity E2E and N437 reconnect/suspend proof required |
| `loop-power` | Touch resets autosleep, refresh qualification no longer hits disk every frame, 1 Hz battery polling, throttled Wi-Fi status, association-aware status | Safe optimization prepared; DHCP/scan worker remains open |
| `suspend-resume` | Commits the destination activity before wake render, requests one clean presentation and coalesces deferred/synchronous renders | Code prepared; 100-cycle N437 wake soak required |
| `display-recovery` | Maps dirty regions into FBInk orientation, removes duplicate backend refresh policy and performs guarded live DRM→FBInk failover on real I/O failure | Code prepared and isolated mapping/source compiled; ARM/EPDC hardware proof required |
| `websocket-atomic` | WebSocket EPUB and custom-font uploads use staging, checked writes, sync, validation and same-directory activation; old destinations survive abort/failure. Kobo WebSocket scope is limited to EPUBs under `/Books`. | Code prepared; failure-injection tests and web authorization remain required |
| `ci` | Adds a Kobo host CMake/CTest job to GitHub Actions | Workflow prepared; first CI run must prove dependencies |

## Use

Check all exact replacements without writing:

```bash
python3 docs/k4-prepared/apply_prepared_changes.py --phase all
```

Apply one reviewable phase:

```bash
python3 docs/k4-prepared/apply_prepared_changes.py --phase touch-registry --apply
```

Then inspect and test:

```bash
git diff --check
git diff --stat
git diff
```

Do not classify prepared code as `PASS` until Luna has built it and, where the
matrix requires it, tested the exact binary on the N437.
