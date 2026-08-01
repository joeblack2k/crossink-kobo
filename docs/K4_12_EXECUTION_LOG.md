# Beta 4 execution log

> Dit bestand wordt tijdens de uitvoering continu bijgewerkt. Verwijder historische failures niet; voeg de herhaalde pass eronder toe.

## Baseline

```text
repository=joeblack2k/crossink-kobo
branch=hardening/kobo-beta4
head_sha=6aa44799c754e738cd3ccb98e71cf65e2c5c57da
submodule_status=freeink-sdk e93f67a4fb19c75fc6b3cb692f28ac3dec8d9e59; nested lucide c81680e066f45b640743ca78ae36cdedda3f0318
working_tree_clean=false; pre-existing modified/untracked files in source worktree
build_host=Darwin arm64; macOS
compiler=Apple clang 21.0.0 (clang-2100.1.1.101)
cmake=cmake 4.4.1
buildroot=2026.08-git
connected_n437=not reachable at http://192.168.7.2 during baseline probe
n437_model=not queried
n437_kernel=not queried
active_binary_sha=not available
recovery_available=not verified in this baseline
usb_ssh_available=not verified in this baseline
```

## Eerste agentbericht

```text
Gebruik maximaal 3 parallelle subagents waar dat nuttig is. Ik werk als Luna Max-hoofdagent
en blijf eigenaar van architectuur, integratie, tests, commits en finale beoordeling. Ik
start alleen afgebakende onafhankelijke inspecties voor touch/input, power/Wi-Fi/suspend
en web/security/CI; alle conclusies worden lokaal gecontroleerd.
```

## Prepared-code application

```text
check_only=FAILED: prepared baseline bbe2f05d4a587d55fa2e6391f9825e376122a76f is not an ancestor/object of current HEAD 6aa44799c754e738cd3ccb98e71cf65e2c5c57da; phases cannot be applied blindly
touch_registry_apply=
touch_gestures_apply=
loop_power_apply=
websocket_atomic_apply=
ci_apply=
```

## Issue-status

| ID | Status | Commit(s) | Automatische tests | Hardwarebewijs | Notities |
|---|---|---|---|---|---|
| TCH-01 | FIXED_LOCAL | 18eb8fce | `crossink-kobo-touch-ui-registry` PASS; staged-frame test | not required for local logic | Renderthread publishes a complete staging buffer under one commit; old committed frame remains visible during staging. |
| TCH-02 | FIXED_LOCAL | 18eb8fce | `crossink-kobo-touch-ui-registry` PASS; generation changes on commit/invalidate | N437 transition proof still required | Generation is checked before consuming an injected target; activity/target consumption invalidates the active generation. |
| TCH-03 | FIXED_LOCAL | 64fdeb98 | standalone gesture test PASS; routing E2E ontbreekt | N437 gesture proof ontbreekt | Touch target is captured on touchdown and only tap/long-press-compatible actions use it; swipe/navigation actions cannot activate the captured UI target. |
| TCH-04 | OPEN |  |  |  |  |
| TCH-05 | FIXED_LOCAL | 64fdeb98 | standalone gesture test PASS for explicit deadband cancellation | N437 proof ontbreekt | Movement outside tap slop and below swipe distance emits `Cancelled` instead of silently disappearing. |
| TCH-06 | FIXED_LOCAL | c0c511f4 | source review; host build blocked by missing Linux headers | N437 autosleep soak ontbreekt | Raw touch frames now mark the existing main-loop activity latch; a touch-only gesture resets the sleep timer. |
| TCH-07 | FIXED_LOCAL | 59cf1d50, 4fe51abe | `crossink-kobo-evdev-touch` PASS in GitHub run `30715081795` | ARM/N437 timestamp evidence ontbreekt | Kernel event timestamps are now preferred; the host parser test proves timestamp preservation and monotone fallback path coverage remains hardware-dependent. |
| TCH-08 | FIXED_LOCAL | 59cf1d50, 4fe51abe | `crossink-kobo-evdev-touch` PASS in GitHub run `30715081795` | N437 drop/resync evidence ontbreekt | Host eventstream proves `SYN_DROPPED` emits discontinuity and subsequent touch frames recover; physical device-loss/reconnect evidence remains open. |
| TCH-09 | OPEN |  |  |  |  |
| TCH-10 | FIXED_LOCAL | 795d20b7 | source review; FIFO stress test ontbreekt | N437 rapid-tap evidence ontbreekt | Semantic touch targets use an 8-entry bounded FIFO and log/drop the newest item on overflow instead of overwriting an earlier target. |
| NET-01 | IN_PROGRESS | afc4e3b2 | source review; latency test not available on macOS | N437 timing evidence ontbreekt | Background saved-network service is rate-limited to 500 ms; `iw` scan and DHCP failure/latency tests remain open. |
| WEB-01 | IN_PROGRESS | d0c28537 | source inspection only; no negative HTTP/WebDAV authorization tests yet | N437 exposure test ontbreekt | Kobo mutating HTTP/WebDAV requests now require the documented USB subnet; WebSocket mutators are not started on Kobo. Token/pairing is not implemented, so broader interface audit remains open. |
| WEB-02 | FIXED_LOCAL | d0c28537 | source-level atomic staging; failure-injection tests still required | N437 abort/disk-full test ontbreekt | HTTP, WebSocket, font and WebDAV PUT paths stage in same directory and do not remove the prior destination before activation. |
| SYS-01 | FIXED_LOCAL | 6d181167, 5581dea4 | GitHub Actions run `30716555316` PASS: Kobo host, CTest, dependency-audit, cppcheck en clang-format | N437 runtime I/O trace ontbreekt | Refresh qualification is cached per profile for the process lifetime and invalidated after a new marker is written; the main loop no longer rereads model, kernel, manifest and marker every frame. |
| SYS-02 | FIXED_LOCAL | 8c82cc9b | shell/Python checks pass; ARM build unavailable on macOS | N437 power trace ontbreekt | Battery/USB sysfs polling in HalGPIO is capped at one snapshot per second while key input remains per-frame. |
| NET-02 | OPEN |  |  |  |  |
| NET-03 | FIXED_LOCAL | 3ce995d5 | GitHub Actions run `30716683191` PASS: Kobo host, CTest, dependency-audit, cppcheck en clang-format | N437 radio-current trace ontbreekt | Kobo webserver disables Wi-Fi powersave only while active and restores powersave when stopped; ESP behavior remains unchanged. |
| WEB-03 | FIXED_LOCAL | d0c28537 | source-level staging/write/sync checks | N437 WebSocket disabled/HTTP fallback proof ontbreekt | WebSocket upload is staged and disabled on Kobo; font upload checks writes and magic before rename. |
| DSP-01 | OPEN |  |  |  |  |
| DSP-02 | OPEN |  |  |  |  |
| DSP-03 | OPEN |  |  |  |  |
| PWR-01 | FIXED_LOCAL | aff0b420, 7daf4a16 | GitHub Actions run `30716270944` PASS: Kobo host, CTest, cppcheck, clang-format, dependency audit | N437 wake/reconnect proof ontbreekt | Suspend preparation now resets gesture/capture/injected touch state and closes evdev; resume reopens the original device or rediscovers it. |
| PWR-02 | OPEN |  |  |  |  |
| CAL-01 | OPEN |  |  |  |  |
| CI-01 | FIXED_LOCAL | 100d2564, 5e8a5070, 3e3bf97c | GitHub Actions run `30714722189` PASS: Kobo build, CTest, dependency audit, clang-format, cppcheck, Test Status | not applicable | Hosted CI validates the Kobo host target and static checks on the published branch. |
| CI-02 | OPEN |  |  |  |  |
| REL-01 | OPEN |  |  |  |  |
| REL-02 | OPEN |  |  |  |  |
| MAINT-01 | OPEN |  |  |  |  |

## Faseverslagen

### Fase 0 — baseline

```text
status=
commands=
results=
pre_existing_failures=
commit=
```

### Fase 1 — atomische touchpublicatie

```text
status=FIXED_LOCAL
root_cause=Renderthread registratie schreef rechtstreeks in de actieve registry; input kon een half opgebouwd frame lezen. Generation werd niet veilig gepubliceerd/ongeldig gemaakt bij consumptie.
implementation=TouchUiRegistry kreeg stagingRegions, beginFrame/commitFrame, invalidate en generation-validatie. ActivityManager commit de registry na render; MappedInputManager weigert stale targets.
tests=cmake host target crossink-kobo-touch-ui-registry-test; ctest PASS (1/1). De volledige macOS Kobo-build blijft baseline-FAIL door ontbrekende Linux-headers en GPIO-linktest.
hardware=Niet uitgevoerd; geen N437-bereikbaarheid tijdens baseline-probe.
commit=18eb8fce
remaining_risk=Activity-instance-ID is vervangen door generation invalidation; directe raw-evdev-to-activity E2E en ARM/N437 bewijs ontbreken.
```

### Fase 2 — semantische touch en gestures

```text
status=IN_PROGRESS
root_cause=Release-point resolution allowed a swipe starting over a target to activate a different target; 25-71 px movement disappeared silently.
implementation=Touchdown captures the committed target generation; non-target navigation actions clear capture. Gesture deadband emits explicit Cancelled. Activity transitions invalidate the registry.
tests=Standalone clang++ gesture test PASS with deadband case. Full CMake gesture target is blocked on macOS missing linux/input.h; raw routing E2E still missing.
hardware=Niet uitgevoerd.
commit=64fdeb98
remaining_risk=Long-press remains button-like rather than a distinct semantic target; bounded FIFO and raw evdev E2E remain open.
```

### Fase 3 — evdev-hardening

```text
status=IN_PROGRESS
root_cause=readFrame ignored kernel event timestamps and had no SYN_DROPPED handling, so backlog/discontinuity could distort long-press timing or leave stale contact state.
implementation=EVIOCSCLOCKID(CLOCK_MONOTONIC) is requested; valid monotone event timestamps are used with fallback; SYN_DROPPED clears contact state and emits a discontinuity frame that resets gesture capture.
tests=No local compile because macOS lacks linux/input.h; parser fault-injection test still required.
hardware=Niet uitgevoerd.
commit=59cf1d50
remaining_risk=Typed read errors, bounded reconnect/backoff, multitouch resync and raw-evdev-to-activity E2E remain open.
```

### Fase 4 — main loop, power en Wi-Fi

```text
status=PARTIAL
root_cause=Touch frames were not part of the existing inactivity activity set.
implementation=c0c511f4 adds a one-shot touch activity latch in HalGPIO and marks it from the Kobo touch reader. 795d20b7 bounds semantic target delivery with a FIFO. 8c82cc9b caps battery/USB sysfs polling; afc4e3b2 rate-limits background Wi-Fi service.
latency_before=
latency_after=
tests=Source/diff review; full Kobo compile is blocked on macOS by missing linux/input.h.
hardware=
commit=c0c511f4, 795d20b7, 8c82cc9b, afc4e3b2
remaining_risk=Wi-Fi scan/DHCP latency and hardware validation remain open.
```

### Fase 5 — webbeveiliging en atomic I/O

```text
status=
threat_model=
implementation=
negative_tests=
hardware=
commit=
remaining_risk=
```

### Fase 6 — display en suspend

```text
status=
implementation=
tests=
hardware_soak=
commit=
remaining_risk=
```

### Fase 7 — CI en release

```text
status=CI_GREEN_RELEASE_BLOCKED
ci_jobs=GitHub Actions run 30715253109: all PR jobs PASS (Kobo host, CTest, dependency audit, clang-format, cppcheck, Test Status)
version=1.4.0-kobo-beta3 source snapshot; no Beta 4 binary release
rc_sha=not assigned
release_decision=nog niet Beta 4; physical N437 gates remain open
draft_pr=https://github.com/joeblack2k/crossink-kobo/pull/1
commit=100d2564, 5e8a5070, 3e3bf97c, 4fe51abe, f0699977, 448dcc5a
```

### Fase 6 vervolg — touchstate rond suspend

```text
status=FIXED_LOCAL
root_cause=De Kobo evdev-descriptor en gesturestate bleven over een RAM-suspend heen bestaan, waardoor oude frames of een stale down-state na wake konden worden verwerkt.
implementation=aff0b420 voegt een lifecycle-hook toe: vóór suspend worden gesture/capture/injected state gereset en wordt evdev gesloten; na wake wordt het device heropend of opnieuw ontdekt. 7daf4a16 corrigeert de clang-format-volgorde.
tests=GitHub Actions run 30716270944: Kobo host, CTest, dependency-audit, cppcheck en clang-format PASS.
hardware=Niet uitgevoerd; N437 suspend/wake/reconnect-soak blijft open.
commit=aff0b420, 7daf4a16
remaining_risk=Kernel wake-sourcegedrag, eerste wake-frame, frontlightherstel en fysieke stale-touch/reconnect moeten op de N437 worden gemeten.
```

### Fase 4 vervolg — refresh qualification

```text
status=FIXED_LOCAL
root_cause=syncRefreshProfilePreference() werd iedere hoofdloop uitgevoerd en las voor niet-Safe profielen telkens model, kernel, buildmanifest en qualification-marker opnieuw.
implementation=6d181167 voegt een kleine procescache per Fast/MaxBeta-profiel toe; recordKoboRefreshProfileQualification() invalideert alleen de betreffende entry na atomische marker-installatie. 5581dea4 volgt clang-format-21.
tests=GitHub Actions run 30716555316: Kobo host, CTest, dependency-audit, cppcheck en clang-format PASS.
hardware=Niet uitgevoerd; N437 runtime-I/O en energieprofiel blijven te meten.
commit=6d181167, 5581dea4
remaining_risk=Een extern gewijzigde of verwijderde marker wordt pas na procesrestart opnieuw gezien; dat is bewust gekoppeld aan de immutable kernel/binary identity van een firmwareproces.
```

### Fase 4 vervolg — Wi-Fi powersave rond webtransfer

```text
status=FIXED_LOCAL
root_cause=CrossPointWebServer::begin() zette powersave uit voor responsiviteit, maar stop() herstelde de radio-instelling niet.
implementation=3ce995d5 schakelt op KOBO_LINUX powersave opnieuw in nadat de webserver en transferresources zijn gestopt.
tests=GitHub Actions run 30716683191: Kobo host, CTest, dependency-audit, cppcheck en clang-format PASS.
hardware=Niet uitgevoerd; N437 idle/transfer current-draw vergelijking blijft open.
commit=3ce995d5
remaining_risk=De effectieve driverstand en batterijwinst moeten op de N437 met wlan0 en suspendmarkers worden gemeten.
```

## Commitlog

| Commit | Onderwerp | Issue-ID's | Tests |
|---|---|---|---|
|  |  |  |  |

## User gates

| Gate | Status | Risico | Vereiste actie | Resultaat |
|---|---|---|---|---|
| GATE-HW-01 | NOT_REACHED |  |  |  |
| GATE-PWR-01 | NOT_REACHED |  |  |  |
| GATE-DSP-01 | NOT_REACHED |  |  |  |
| GATE-SEC-01 | NOT_REACHED |  |  |  |

## Finale regressie

```text
repository_sha=
binary_sha256=
all_required_ci=
arm_build=
touch_regression=
network_regression=
web_security_regression=
suspend_soak=
display_soak=
secrets_scan=
watchdog_counter=
open_p0=
open_p1=
release_decision=
```

## Resterende vervolgissues

- 
