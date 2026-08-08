# Validatie- en bewijsplan

## Bewijsniveaus

| Niveau | Betekenis | Mag claimen |
|---|---|---|
| L0 | Broninspectie/diff | Alleen ontwerp of vermoedelijke oorzaak |
| L1 | Unit/static tests | Lokale logica werkt voor testcases |
| L2 | Host-Kobo integratietest | Linux/Kobo-codepad werkt zonder echt panel/device |
| L3 | ARM/Buildroot-build | Target compileert en linkt |
| L4 | N437 gerichte hardwaretest | Specifieke workflow werkt op één release-SHA |
| L5 | N437 soak/regressie | Releasekwaliteit voor het geteste domein |

Geen L4/L5-claim op basis van L1–L3.

## Verplichte automatische checks per fase

- formatter;
- `git diff --check`;
- static analysis volgens repositorybeleid;
- Kobo-host CMake-build;
- `ctest --output-on-failure`;
- relevante simulator-/ESP32-regressie wanneer gedeelde code is gewijzigd;
- target-aware dependency audit;
- ARM/Buildroot-build waar de omgeving dit ondersteunt.

## Touchmatrix

| Test | Niveau | Gate |
|---|---:|---:|
| Registry staging/atomic commit | L1/L2 | P0 |
| Stale frame/activity target | L1/L2 | P0 |
| Swipe over target | L2/L4 | P0 |
| Direct long-press | L2/L4 | P0 |
| Threshold boundary corpus | L1 | P0 |
| Two taps in one drain | L2 | P0 |
| `SYN_DROPPED` | L1/L2 | P0 |
| Device loss/reconnect | L2 | P0 |
| Touch-only autosleep activity | L2/L4 | P0 |
| Orientation + modal matrix | L4 | RC |

## Main-loop/netwerkmatrix

| Test | Niveau | Gate |
|---|---:|---:|
| No qualification read in hot loop | L1/L2 | P1 |
| Paced battery polling | L1/L2 | P1 |
| Non-blocking scan | L2/L4 | P0 |
| Non-blocking DHCP | L2/L4 | P0 |
| Association loss with stale IP | L2/L4 | P1 |
| Wi-Fi powersave returns after idle | L2/L4 | P1 |
| Input responsiveness during transfer | L4 | RC |

## Webmatrix

| Test | Niveau | Gate |
|---|---:|---:|
| Unauthenticated mutation denied | L2/L4 | P0 |
| Valid paired mutation succeeds | L2/L4 | P0 |
| Atomic overwrite failure corpus | L2/L4 | P0 |
| WebSocket disconnect corpus | L2/L4 | P0 |
| Font short-write/error | L2 | P1 |
| Protected paths/traversal | L2 | P0 |
| No secrets in log/API | L2/L4 | RC |

## Display/powermatrix

| Test | Niveau | Gate |
|---|---:|---:|
| Scheduler is single policy | L1/L2 | P1 |
| FBInk receives dirty region | L1/L2/L4 | P1 |
| DRM reopen/failover | L2/L4 | P1 |
| One wake-present | L4 | P1 |
| Touch reset around suspend | L2/L4 | P1 |
| 10-cycle development soak | L4 | P1 |
| 100-cycle RC soak | L5 | RC |
| 1.000 refresh/page-turn soak | L5 | RC |

## Release-evidence-layout

Gebruik bestaande repositoryconventies. Een aanbevolen structuur:

```text
artifacts/kobo/hardware/beta4/<issue-id>/<timestamp>/
  manifest.txt
  commands.txt
  test-results.txt
  relevant-log.txt
  hashes.txt
  screenshot-or-photo-reference.txt
```

Lokale grote/binaire evidence mag bewust buiten de publieke repo blijven. Commit dan wel een geanonimiseerd manifest of verwijzing volgens het bestaande projectbeleid.

## Manifestvereisten

```text
issue_id=
repository_sha=
binary_sha256=
kernel=
model=
test_start_utc=
test_end_utc=
commands=
expected=
observed=
status=PASS|FAIL
watchdog_counter=
crash_artifact=
```

## Regressieregel

Alle finale RC-gates moeten op exact dezelfde repository- en binary-SHA draaien. Oud bewijs van een andere binary blijft historische context en telt niet als final RC-pass.

## Falen

Bij een failure:

1. status onmiddellijk `FAIL` of `IN_PROGRESS`;
2. exacte test en evidence bewaren;
3. oorzaak niet invullen zonder bewijs;
4. kleinste reproducerende testcase maken;
5. fix + regressietest;
6. dezelfde test opnieuw uitvoeren;
7. geen ander issue ongemerkt als PASS meenemen.
