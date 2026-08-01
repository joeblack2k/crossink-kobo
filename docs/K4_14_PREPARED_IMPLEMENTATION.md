# Prepared implementation package

## Purpose

The files under `docs/k4-prepared/` reduce the first implementation phases to a
review-and-validation job. They are bound to the public baseline
`bbe2f05d4a587d55fa2e6391f9825e376122a76f` and can be applied in separate,
reviewable phases.

## Required order

```bash
python3 docs/k4-prepared/apply_prepared_changes.py --phase all
python3 docs/k4-prepared/apply_prepared_changes.py --phase touch-registry --apply
# inspect, format, test, commit
python3 docs/k4-prepared/apply_prepared_changes.py --phase touch-gestures --apply
# inspect, format, test, commit
python3 docs/k4-prepared/apply_prepared_changes.py --phase evdev-hardening --apply
# inspect, format, run evdev/gesture tests, commit
python3 docs/k4-prepared/apply_prepared_changes.py --phase loop-power --apply
# inspect, format, test, commit
python3 docs/k4-prepared/apply_prepared_changes.py --phase suspend-resume --apply
# inspect, verify wake control flow, commit
python3 docs/k4-prepared/apply_prepared_changes.py --phase display-recovery --apply
# inspect, run packed-mono tests, ARM build and hardware gate before PASS
python3 docs/k4-prepared/apply_prepared_changes.py --phase websocket-atomic --apply
# inspect, format, test, commit
python3 docs/k4-prepared/apply_prepared_changes.py --phase ci --apply
# inspect, test workflow syntax, commit
```

## Prepared issue coverage

| Issue | Prepared state |
|---|---|
| TCH-01 | Implementation and host registry tests prepared |
| TCH-02 | Generation validation and transition invalidation prepared; explicit activity-ID is replaced by safe transition invalidation |
| TCH-03 | Swipe cannot activate captured target in prepared router |
| TCH-04 | Semantic direct long-press and Recent Books action route prepared |
| TCH-05 | Explicit cancellation plus boundary test prepared |
| TCH-06 | Autosleep activity fix prepared |
| TCH-07 | Monotonic kernel timestamps with validated fallback prepared |
| TCH-08 | `SYN_DROPPED` discard/resync and parser test prepared |
| TCH-09 | Typed errors, reconnect/back-off and suspend/orientation reset prepared |
| TCH-10 | One completed gesture per app frame prevents target overwrite; routing E2E and measured burst proof remain |
| SYS-01 | Refresh-marker hotpath fix prepared |
| SYS-02 | Battery/USB polling cadence prepared |
| NET-02 | Association-aware status prepared |
| NET-01/03 | Only polling reduction is prepared; worker/powersave lifecycle remains open |
| WEB-02 | WebSocket and custom-font staging/validation/activation prepared; failure-injection proof remains |
| CI-01 | Host-test workflow prepared |
| CI-02 | Deterministic evdev parser test prepared; full raw-event-to-testactivity harness remains |
| PWR-01 | One destination wake render with clean refresh prepared; physical latency/ghosting proof remains |
| PWR-02 | Touch state reset before suspend prepared; physical soak remains |
| DSP-01 | FBInk duplicate refresh budget removed; central scheduler remains authoritative |
| DSP-02 | Orientation-aware FBInk dirty region and host mapping tests prepared |
| DSP-03 | Guarded live DRM→FBInk failover prepared; ARM/N437 fault injection remains |

## Review rules

- Do not blindly commit the generated diff.
- Run clang-format on changed C/C++ files.
- Compile after every phase.
- Add the missing E2E tests before declaring the touch phases complete.
- Keep every issue below `PASS` until its mandatory hardware gate succeeds.
