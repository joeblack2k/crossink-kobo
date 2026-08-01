# Code-audit CrossInk-Kobo — 12 juli 2026

## Conclusie

De Kobo-port heeft een bruikbare technische basis: moderne Linux/Buildroot,
native display, EPUB-lezen, per-boekpositie, schaalstanden, Wi-Fi, persistente
webupload, apparaatinfo, frontlight en een eerste directe-touchroute werken op
een echte N437. De port is nog geen feature-parity Beta 1. De belangrijkste
resterende risico's zitten niet meer in boot of basisdrivers, maar in
onvolledige touchmigratie en ESP/X4-codepaden die op Linux zichtbaar of
bereikbaar zijn zonder correcte Kobo-implementatie.

Oorspronkelijk geaudite release (historische baseline; de actuele status staat
in `kobo-feature-parity.md` en de testmatrix):

- release: `dev-direct-touch-015acdf001cd`;
- binary: `015acdf001cdf637db25453bcb2121d45f460392f82cb5e033b914d136bf6ccc`;
- kernel: Linux 6.19.0;
- runtime: ruim veertien uur actief, watchdog early-start-failures `0`;
- netwerk tijdens audit: test-SSID, RSSI circa `-61 dBm`;
- ARM-platformtests: 6/6 PASS.

## Bevindingen

### P0 — vóór verdere featureontwikkeling repareren

| ID | Bevinding | Codebewijs | Effect | Vereiste reparatie |
|---|---|---|---|---|
| A-01 | Direct touch was slechts in enkele activities geïmplementeerd. | zichtbare `GUI.drawList`-activities | Een tap kon via X4-stappen het verkeerde item raken. | **Afgerond in actieve ontwikkeling:** gedeelde `DirectListTouch`, direct-targetconsumenten en geen `queueLegacyNavigationFallback`; activitymatrix en modal/stale stress blijven releasewerk. |
| A-02 | Een niet-geconsumeerd direct touchtarget bleef over meerdere frames bestaan. | `MappedInputManager` + `TouchUiRegistry` | Een slider/tab/optie kon later in een ander scherm onverwacht worden uitgevoerd. | **De basisreparatie is gedaan:** frame-clear, single-use consume en registry-generation bestaan. Modal-, disabled- en rotatiestress blijven open in fase 1. |
| A-03 | De zichtbare ESP-updatemenu's hadden geen Kobo-updater. | `SettingsList.h`; OTA-sources | Dood of misleidend updatepad. | **Afgerond:** ESP OTA/SD-acties zijn op Kobo verborgen; een gesigneerde Kobo-updater blijft als echte latere feature open. |
| A-04 | Netwerkactivities konden Wi-Fi uitzetten of een stille restart forceren. | Wi‑Fi-kiezer, KOReader, Clock, OPDS, Calibre en fontflows | Dit kan altijd-aan Wi‑Fi en de uploadserver onderbreken. | **Gedeeltelijk afgerond:** Kobo paden doen geen ESP-teardown; de picker behoudt een bestaande stationverbinding tijdens scan en Clock Sync/HTTP bleven op N437 bereikbaar. Volledige flows voor OPDS, Calibre en KOReader blijven open. |

### P1 — featurepariteit en dagelijkse betrouwbaarheid

| ID | Bevinding | Codebewijs | Effect | Vereiste reparatie |
|---|---|---|---|---|
| A-05 | Nearby sync is nog ESP-NOW-code en zet de radio uit. | `NearbyStatsSyncActivity.cpp:205,240-265`; `NearbyBookPositionSyncActivity.cpp:603` | Compileerbaarheid via stubs is geen werkende Kobo-pariteit. | Bouw een Linux LAN discovery/sync-adapter of markeer de functie zichtbaar als niet beschikbaar totdat deze echt werkt. |
| A-06 | Statistiek- en nearby device-ID gebruikte op Kobo de vaste simulator-MAC `DE:AD:BE:EF:00:01`. | `GlobalReadingStats.cpp` | Alle Kobo's zouden dezelfde identiteit krijgen; syncbestanden konden botsen. | **Afgerond voor stats:** de actieve Kobo gebruikt de echte wlan0-MAC; N437-probe bevestigde `device_784561352b13.bin`. Nearby zelf blijft ongeporteerd en verborgen. |
| A-07 | De ESP `CrossPointWebServer`/WebDAV-build is uitgesloten; Kobo gebruikt de native simulator HTTP-server. Basisupload werkt, maar WebSocket, volledige settings-API en WebDAV zijn niet gelijkwaardig bewezen. De image bevat nu wel de curl-client met OpenSSL, waardoor de bestaande gedeelde HTTP-adapter niet meer direct faalt op een ontbrekende executable. | `platform/kobo/app/CMakeLists.txt:45,49`; `KoboWebTransferService.cpp`; native serverroutes in `vendor/crosspoint-simulator/src/CrossPointWebServer.cpp`; Buildroot-defconfig | De webpagina kan meer suggereren dan de Linux-backend ondersteunt. | Maak een expliciete Kobo HTTP-capabilitymatrix; implementeer en test ontbrekende routes, WebDAV en cancel/progress. |
| A-08 | De frontlightslider kon een bestaande globale waarde tussen de 10%-stappen tonen. | `EpubReaderMenuActivity.cpp` | Inconsistente UI en onduidelijke actieve stap. | **Basisreparatie gedaan:** Kobo normaliseert bij openen naar 0/10/.../100. De volledige 11-stappen hardwarematrix blijft open. |
| A-09 | Schaalbaarheid is fysiek bewezen voor Home en Settings op vier standen en File Transfer op 200%; overige schermen, thema's en oriëntaties zijn niet bewezen. | `artifacts/kobo/hardware/scale/20260711T1935Z/` | Overlap zoals de eerdere File Transfer-fout kan elders terugkomen. | Voer een gerichte kernschermmatrix uit; repareer alleen gevonden afwijkingen, bouw geen tweede layoutsysteem. |
| A-10 | Display gebruikt bewust veilige 1-bit fallback; hoogwaardige grayscale/covers zijn niet aanwezig. De refreshbenchmark mist 1000-update soak en profielkeuze. | `artifacts/kobo/hardware/display/refresh-benchmark.csv`; `HalDisplay::supportsStripGrayscale()` | Afbeeldingen/covers missen kwaliteit; “Max” kan niet veilig worden aangeboden. | Voeg gecontroleerde dither/grayscale toe en meet Veilig/Snel/Max met thermiek, ghosting en automatische fallback. |
| A-11 | EPUB 12/12 werkt, maar TXT/BMP/XTC, footnotes, TOC, bookmarks, clippings en foutpaden zijn nog niet als N437-workflow bewezen. | `artifacts/kobo/hardware/reader/n437-reader-corpus.tsv` | CrossInk-readerpariteit is nog incompleet. | Gebruik korte, gerichte smoke- en interactietests per functie; herhaal het hele corpus pas voor de releasecandidate. |

### P2 — releasekwaliteit

| ID | Bevinding | Effect | Vereiste reparatie |
|---|---|---|---|
| A-12 | Apparaatinfo gebruikt deels hardcoded Engelse labels. | Onvolledige lokalisatie. | Routeer labels via vertalingen. |
| A-13 | Oud bewijs is verspreid over meerdere binary-hashes. | Een regressie kan onopgemerkt blijven. | Draai één volledige Beta 1-regressie op exact dezelfde releasehash. |
| A-14 | Suspend is bewust uitgeschakeld en nog niet veilig geaccepteerd. | Geen dagelijkse powerflow of boekcover-sleepscreen. | Pas als laatste onder USER GATE testen; tot dan dev-image wakker houden. |
| A-15 | Gesigneerde updater, final USB-storage, secretscan en reproduceerbare final image ontbreken. | Geen veilig uitleverbare Beta 1. | Uitvoeren nadat functionaliteit stabiel is; lokale hardware-artifacts zijn niet onderdeel van deze publieke bronrepo. |

## Wat niet opnieuw geport moet worden

Herbouw niet opnieuw: kernel/boot/rootfs, DRM-basispad, touchkalibratie,
frontlightsysfs, batterij-uitlezing, EPUB-basispaginatie, per-boekpositie,
Home/Settings-schalen, bekende-Wi-Fi-autoconnect, statusicoon, USB-/Wi-Fi-
EPUB-upload, apparaatinfo, recovery/watchdog en SSH. Houd deze onderdelen als
regressiegates, niet als roadmapfases.

## Auditmethode en beperking

Deze audit combineert broninspectie, actieve-release-inventory, bestaande
hardwareartefacten, runtime-status en de zes Kobo-platformtests. Niet iedere
upstreamfeature is tijdens deze audit fysiek uitgevoerd. Daarom gebruikt het
pariteitsregister `PARTIAL` of `FAIL` waar alleen code of oud bewijs bestaat.
