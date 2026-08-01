# CrossInk-Kobo Beta 4 hardening — read first

## Doel

Deze documentset stuurt één samenhangende hardening-pass van `joeblack2k/crossink-kobo` aan. Het doel is niet om CrossInk opnieuw te ontwerpen, maar om de huidige werkende Kobo Glo HD N437-port van een goede **Beta 3-basis** naar een aantoonbaar robuuste **Beta 4 release candidate** te brengen.

De hoofdthema's zijn:

1. correcte en atomische touch-routing;
2. robuuste Linux-evdev-verwerking;
3. een responsieve main loop zonder blokkerende Wi-Fi- of sysfs-hotpaths;
4. veilig webbeheer en atomische uploads;
5. gecontroleerde e-ink refresh-, suspend- en recoverypaden;
6. verplichte Kobo-CI, hardware-evidence en consistente release-informatie.

## Repository en target

- Repository: `joeblack2k/crossink-kobo`
- Doelapparaat: Kobo Glo HD N437
- Hoofdtarget: Linux/Buildroot + native Kobo HAL
- Gedeelde code: CrossInk/CrossPoint readercode
- Niet het primaire producttarget: Xteink X3/X4 of de desktop simulator

De agent moet bij aanvang het actuele branch, commit-SHA, submodule-status, buildomgeving en aangesloten hardware opnieuw vastleggen. Deze documenten beschrijven de auditbaseline; de actuele broncode blijft leidend.

## Verplichte leesvolgorde

1. `K4_00_READ_ME_FIRST.md`
2. `K4_01_PROJECT_STATE.md`
3. `K4_02_ISSUE_REGISTER.md`
4. `K4_03_EXECUTION_ROADMAP.md`
5. `K4_04_TOUCH_ARCHITECTURE.md`
6. `K4_05_EVDEV_AND_INPUT_TESTS.md`
7. `K4_06_MAIN_LOOP_POWER_WIFI.md`
8. `K4_07_WEB_SECURITY_AND_ATOMIC_IO.md`
9. `K4_08_DISPLAY_SUSPEND_RECOVERY.md`
10. `K4_09_VALIDATION_MATRIX.md`
11. `K4_10_CI_RELEASE_HYGIENE.md`
12. `K4_11_NON_GOALS_AND_USER_GATES.md`
13. `K4_12_EXECUTION_LOG.md`
14. `K4_14_PREPARED_IMPLEMENTATION.md`
15. `K4_15_LUNA_MINIMAL_WORK.md`
16. `K4_13_GOAL_PROMPT.md`

Lees daarnaast de bestaande projectdocumentatie die door deze bestanden wordt genoemd, in het bijzonder:

- `docs/kobo-code-audit-2026-07-12.md`
- `docs/kobo-feature-parity.md`
- `docs/kobo-n437-test-matrix.md`
- `docs/kobo-batch-log.md`
- `CHANGELOG.md`
- de Kobo-CMake-, Buildroot-, input-, display-, suspend-, Wi-Fi- en webservercode.

## Harde werkregels

- Bewaar alles wat aantoonbaar werkt: boot, recovery, USB-SSH, basisdisplay, EPUB-lezen, leespositie, frontlight, bekende Wi-Fi, webupload en bestaande hardware-evidence.
- Een compile is geen runtimebewijs. Een simulatorresultaat is geen N437-bewijs.
- Geen status `PASS` zonder reproduceerbare test en bewijs op exact dezelfde commit/binary.
- Geen grote rewrite wanneer een afgebakende reparatie voldoende is.
- Geen stille fallback die een fout verbergt. Log oorzaak, gekozen fallback en resultaat.
- Geen productieclaim voor een functie die alleen via stubs, oude evidence of een andere binary is getest.
- Geen secrets, persoonlijke Wi-Fi-configuratie, private SSH-sleutels, firmware-images of lokale hardware-artifacts committen.
- Iedere fase eindigt met tests, documentatie, een atomische commit en een bijgewerkt uitvoerlog.
- Wanneer deze `K4_*.md`-bestanden nog uncommitted zijn, moeten ze op de nieuwe werkbranch als eerste afzonderlijke docs-only commit worden opgenomen; nooit resetten of weggooien.
- Werk door naar de volgende fase zolang er geen echte user gate of technisch onoplosbare blokkade is.

## Vereiste eindproducten

Aan het einde moeten minimaal bestaan:

- een werkbranch vanaf de vastgelegde baseline;
- de voorbereide codefasen onder `docs/k4-prepared/`, gecontroleerd en waar correct toegepast;
- alle geaccepteerde codewijzigingen;
- nieuwe en aangepaste tests;
- CI die de Kobo-hosttests uitvoert;
- waar haalbaar een gezaghebbende ARM/Buildroot-build;
- bijgewerkte project- en releasedocumentatie;
- `docs/K4_12_EXECUTION_LOG.md` met commits, tests, bewijs en resterende risico's;
- een duidelijke releasebeslissing: `Beta 4 RC`, `nog niet Beta 4`, of een afgebakende blocker met bewijs;
- een gepushte branch en draft-PR wanneer GitHub-authenticatie beschikbaar is.

## Definitie van klaar

Deze pass is pas klaar wanneer alle P0-issues uit `K4_02_ISSUE_REGISTER.md` zijn gerepareerd of met nieuw bewijs als niet-reproduceerbaar zijn afgesloten, alle verplichte automatische tests groen zijn en geen onbewezen hardwareclaim als `PASS` wordt gepresenteerd.
