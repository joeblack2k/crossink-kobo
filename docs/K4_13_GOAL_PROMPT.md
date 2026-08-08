# `/goal` prompt — CrossInk-Kobo Beta 4 hardening

Kopieer uitsluitend de inhoud van het onderstaande codeblok naar `/goal`.

```text
Werk in de bestaande repository `joeblack2k/crossink-kobo`. Maak geen nieuwe repository en werk niet direct op `main`.

Zet in je eerste bericht expliciet:
Gebruik maximaal 3 parallelle subagents waar dat nuttig is. Laat de subagents op Luna Max draaien. Werk zelf als hoofdagent op Luna Max, wacht op hun resultaten en controleer de uitkomst zelf.

Je opdracht is om de huidige werkende Kobo Glo HD N437-port gecontroleerd van Beta 3-niveau naar een aantoonbare Beta 4 release candidate te hardenen. Dit is een implementatie-, test-, hardwarevalidatie- en releasekwaliteitstaak; stop niet na analyse of advies.

Lees vóór iedere wijziging alle volgende bestanden in deze exacte volgorde:

1. docs/K4_00_READ_ME_FIRST.md
2. docs/K4_01_PROJECT_STATE.md
3. docs/K4_02_ISSUE_REGISTER.md
4. docs/K4_03_EXECUTION_ROADMAP.md
5. docs/K4_04_TOUCH_ARCHITECTURE.md
6. docs/K4_05_EVDEV_AND_INPUT_TESTS.md
7. docs/K4_06_MAIN_LOOP_POWER_WIFI.md
8. docs/K4_07_WEB_SECURITY_AND_ATOMIC_IO.md
9. docs/K4_08_DISPLAY_SUSPEND_RECOVERY.md
10. docs/K4_09_VALIDATION_MATRIX.md
11. docs/K4_10_CI_RELEASE_HYGIENE.md
12. docs/K4_11_NON_GOALS_AND_USER_GATES.md
13. docs/K4_12_EXECUTION_LOG.md
14. docs/K4_14_PREPARED_IMPLEMENTATION.md
15. docs/K4_15_LUNA_MINIMAL_WORK.md
16. docs/k4-prepared/README.md
17. docs/k4-prepared/PREPARED_CHANGELOG.md

Lees daarna de bestaande Kobo-audit, feature-parity, hardwarematrix, batchlog, changelog en relevante code. De actuele bron is leidend, maar verwijder geen gate zonder code- en testbewijs dat het risico al correct is opgelost.

MODEL- EN SUBAGENTREGELS

- Jij bent de Luna Max-hoofdagent en blijft eigenaar van architectuur, integratie, tests, commits en finale beoordeling.
- Gebruik maximaal drie parallelle Luna Max-subagents wanneer werk echt onafhankelijk is.
- Geschikte parallelle taken zijn broninventarisatie, callgraphanalyse, testontwerp, threat modeling en review van een afgebakende module.
- Laat subagents niet gelijktijdig overlappende kernbestanden wijzigen.
- Wacht op hun resultaten, inspecteer hun bewijs en accepteer geen conclusie of patch zonder eigen controle.
- Meld in het eerste bericht welke subagenttaken je start of waarom parallelisatie nog niet nuttig is.

PREPARED-CODE ROUTE

- De baseline-gebonden implementatie staat in `docs/k4-prepared/apply_prepared_changes.py`. Begin niet opnieuw vanaf een blanco analyse.
- Voer eerst check-only uit: `python3 docs/k4-prepared/apply_prepared_changes.py --phase all`.
- Pas daarna iedere fase afzonderlijk toe met `--apply`, inspecteer de diff zelf, formatteer, bouw, test en commit vóór de volgende fase.
- De voorbereide fasen zijn `touch-registry`, `touch-gestures`, `loop-power`, `websocket-atomic` en `ci`.
- Accepteer voorbereide code niet blind. Los compile- of callsiteproblemen zelf op en voeg de ontbrekende E2E/failure-injectiontests toe.
- Herimplementeer een voorbereide oplossing alleen wanneer bron- of testbewijs aantoont dat deze fout is; noteer dan exact waarom.
- Behandel hardware- en runtimeclaims pas als PASS na tests op de exacte ARM-binary.

WERKWIJZE

1. Leg eerst baseline vast in `docs/K4_12_EXECUTION_LOG.md`: repository, branch, HEAD, submodules, working tree, toolchain, bestaande testresultaten en eventueel verbonden N437-hardware.
2. Maak vanaf de actuele HEAD een branch, bij voorkeur `hardening/kobo-beta4` of een vrije variant daarvan. Als de `docs/K4_*.md`-bestanden nieuw en nog niet gecommit zijn, verlies of reset ze niet: neem ze op de nieuwe branch eerst op in een afzonderlijke documentatiecommit `docs: add Kobo Beta 4 hardening plan`.
3. Volg de fasen uit `docs/K4_03_EXECUTION_ROADMAP.md` in volgorde.
4. Behandel alle P0-issues achter elkaar en ga daarna door met de vereiste P1- en P2-gates.
5. Doe per issue oorzaakbevestiging, minimale fix, regressietest, relevante builds, hardwaretest wanneer vereist, logupdate en atomische commit.
6. Blijf werken zolang geen echte user gate nodig is. Een gate voor één issue mag onafhankelijk werk niet blokkeren.
7. Maak geen brede rewrite van stabiele boot-, recovery-, reader- of displaybasis.
8. Behoud simulator- en ESP32-compatibiliteit wanneer gedeelde code wordt gewijzigd, maar optimaliseer het productgedrag voor Kobo/Linux.
9. Een compile is geen hardwarebewijs. Presenteer nooit een onbewezen runtimeclaim als PASS.
10. Laat Safe refresh het defaultprofiel blijven. Activeer geen Max/A2/overclock en overschrijf nooit de originele Kobo-SD.
11. Commit geen secrets, private keys, persoonlijke Wi-Fi-/OPDS-data, firmware-images, cores of lokale hardware-artifacts.
12. Update `docs/K4_12_EXECUTION_LOG.md` na iedere fase en iedere commit.

VERPLICHTE IMPLEMENTATIEVOLGORDE

Fase 0: baseline, branch, beschermrails en bestaande tests.

Fase 1: pas eerst prepared phase `touch-registry` toe; voltooi TCH-01 en TCH-02 met build-, concurrency- en transitiontests.

Fase 2: pas prepared phase `touch-gestures` toe; voltooi TCH-03, TCH-04 en TCH-05, schrijf de ontbrekende routing-E2E-test en implementeer daarna TCH-10 als begrensde FIFO.

Fase 3: pas prepared phase `evdev-hardening` toe; valideer timestamps, `SYN_DROPPED`, typed errors, reconnect en suspendreset, en voltooi alleen nog de raw-evdev-tot-testactivity E2E- en hardwaretests.

Fase 4: pas prepared phase `loop-power` toe; valideer TCH-06, SYS-01, SYS-02 en NET-02 en voltooi daarna NET-01, NET-03 en MAINT-01 zonder blokkerende scan/DHCP.

Fase 5: pas prepared phase `websocket-atomic` toe en bewijs de EPUB/font-failurematrix; voltooi daarna WEB-01 en WEB-03 voor pairing, tokens en route-authorisatie.

Fase 6: pas prepared phases `suspend-resume` en `display-recovery` toe; valideer DSP-01/02/03 en PWR-01/02 op ARM en N437, en voltooi daarna CAL-01.

Fase 7: pas prepared phase `ci` toe en verifieer de echte workflowrun; voltooi daarna REL-01 en REL-02 met ARM/Buildroot en één finale RC-SHA-regressie.

HARD ACCEPTANCE

- Geen inputresolve tegen een half opgebouwd registryframe.
- Geen targetactivatie na activity- of framewissel.
- Swipe over een kaart opent de kaart nooit.
- Directe long-press opent de long-pressactie in één gesture.
- 25–71 px beweging verdwijnt niet stil maar wordt expliciet geannuleerd.
- Touch-only gebruik reset autosleep.
- Eventachterstand verandert de gemeten gestureduur niet.
- `SYN_DROPPED`, deviceverlies en reconnect produceren geen spookactie of blijvende held state.
- Twee snelle taps worden niet samengevoegd of overschreven.
- Wi-Fi-scan en DHCP blokkeren de main/inputthread niet secondenlang.
- De persistente webserver biedt geen onbeveiligde muterende LAN-routes.
- Een onderbroken overwrite behoudt het oorspronkelijke bestand byte-identiek.
- Suspend/wake laat geen stale touch achter.
- PR-CI draait echte Kobo-tests en faalt bij een Kobo-regressie.
- Beta 4 wordt pas genoemd wanneer alle P0-gates en de verplichte RC-gates op één release-SHA groen zijn.

COMMITS EN GITHUB

- Gebruik duidelijke, kleine commits per issue of coherente issuegroep.
- Geen force-push en geen history rewrite.
- Push de werkbranch wanneer GitHub-auth beschikbaar is.
- Open of update aan het einde een draft-PR met baseline, issue-ID's, tests, hardware-evidence, resterende risico's en releasebeslissing.
- Maak geen nieuwe publieke repository; gebruik uitsluitend de bestaande repository.

USER GATES

Volg `docs/K4_11_NON_GOALS_AND_USER_GATES.md`. Vraag alleen een gate voor een werkelijk risicovolle fysieke handeling, langdurige soak of secret/pairingtest. Rond eerst alle software-only en onafhankelijke taken af. Beschrijf bij een gate exact command, risico, rollback en reeds behaalde tests.

RAPPORTAGE

Geef tijdens de uitvoering compacte updates met concrete bevindingen, commits en testresultaten. Eindig niet met een plan maar met uitgevoerde wijzigingen.

Je finale bericht bevat:

1. baseline- en eind-SHA;
2. commits per issuegroep;
3. welke issues PASS, BLOCKED_USER_GATE of nog OPEN zijn;
4. automatische test- en buildresultaten;
5. N437-hardwarebewijs per relevante gate;
6. security- en dataverliesresultaten;
7. CI- en documentatiewijzigingen;
8. link/nummer van de draft-PR indien gemaakt;
9. expliciete releasebeslissing: `Beta 4 RC`, `nog niet Beta 4`, of exacte blocker.

Begin nu met het verplichte eerste bericht, de baseline-inventarisatie en maximaal drie nuttige Luna Max-subagents.
```
