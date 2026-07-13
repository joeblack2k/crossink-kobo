# CrossInk → Kobo Glo HD N437 featurepariteit

Dit is de actuele productstatusbron. `PASS` vereist N437-bewijs, `PARTIAL`
betekent dat een bruikbaar deel werkt maar de upstreamfunctie niet compleet is,
`FAIL` betekent niet werkend/niet echt geport en `N/A-hardware` is uitsluitend
voor fysiek ontbrekende hardware.

Actieve auditrelease: `dev-ui05-20260713T055657Z`, binary-SHA-256
`845e7256d2786680b962c69567bfa743257e31b3c72a6399c0bb036b86172b8b`.
Zie [de code-audit](./kobo-code-audit-2026-07-12.md) voor de open defecten.

| Domein | Functie | Status | Huidig bewijs / ontbrekend deel |
|---|---|---:|---|
| Platform | Moderne N437-kernel, minimale rootfs, CrossInk-first | PASS | Linux 6.19/Buildroot en actieve N437-runtime; final cold-bootreeks blijft releasegate |
| Platform | Recovery/watchdog en USB-SSH devtoegang | PASS | watchdog early failures 0; recovery- en SSH-artefacten aanwezig |
| Platform | Expliciete Kobo/POSIX-grens | PASS | ARM bouwt met `CROSSPOINT_POSIX` + `KOBO_LINUX` en zonder `SIMULATOR`; onafhankelijke Debian-LXC simulator- en ESP32-builds zijn groen. [bewijs](../artifacts/kobo/hardware/platform-matrix/20260713T0131Z/manifest.txt) |
| Platform | N437 capabilitymodel | PASS | Gedeelde code gebruikt `DeviceCapabilities`; echte N437 rapporteert touch/frontlight/wifi/suspend en noemt zich nergens X4. ARM-capabilitytest draaide op de reader. [bewijs](../artifacts/kobo/hardware/platform-capabilities/20260713T0212Z/manifest.txt) |
| Platform | Linux memory, monotone clock, controlled restart and crash path | PASS | Kobo leest werkelijk 442–451 MiB via `/proc`/`sysinfo`; een controlled re-exec herstelde de reader en één afgeschermde SIGABRT-proef schreef PC/LR-crashinfo, herstelde naar CrashActivity en eindigde na de health window met watchdog 0. [bewijs](../artifacts/kobo/hardware/platform-system/20260713T0248Z/manifest.txt) |
| Platform | Target-aware dependency audit | PASS | Finale Kobo-compilecommands zijn actief ge-preprocessed; geen `esp_now`, ESP-OTA-partitie, `Preferences`, `esp_deep_sleep`, directe `ESP.restart` of ongeautoriseerde ESP-WebServerheader bereikt een Kobo-object. De enige expliciet gemelde tijdelijke POSIX-webserver-ABI blijft onder open PLAT-01. [bewijs](../artifacts/kobo/hardware/platform-dependency-audit/20260713T035130Z/manifest.txt) |
| Display | Native 1072×1448, juiste polariteit, full/partial/DU | PASS | fysieke reader en refreshbenchmark; geen terugkeer van negatieve EPUB-render |
| Display | Grayscale, covers en afbeeldingen | PARTIAL | veilige 1-bit fallback werkt; progressive JPEG-afbeeldingen in EPUB gebruiken op ARM een begrensde safe decoder en zijn op N437 cold/warm bewezen. Progressieve coverthumbnails worden nog afzonderlijk behandeld; hoogwaardige grayscale/dither blijft open. |
| Display | Veilig/Snel/Max-profielen | PARTIAL | Safe-profiel, scheduler, dirty-rects, GC16-herstel, CPUFreq-guard en runtimefallback zijn op N437 bewezen op `dev-refresh-executor-bd1202aa3da9` (`bd1202aa3da9…`). Fast/Max hebben nog geen device-specifieke kwalificatie of zichtbare instelling. |
| Touch | Kalibratie, readerzones en tweeknopsframe | PASS | fysieke kalibratie en readerzonebasis |
| Touch | Volledig directe touch zonder X4-stappen | PARTIAL | Gedeelde `DirectListTouch` migreerde de auditlijsten en de legacyqueue is verwijderd. N437 bewees System-tab, System→Wi‑Fi, saved-network-rij, OPDS-lijst→detail, toetsenbord, bookmarks, footnotes, clippings, directe woordbereikselectie, touch-Back én de Home-current-bookkaart in één aanraking. De actieve overlapmatrix bewijst bovendien Home, Library, modal, Settings, reader-menu, keyboard en option-dialog zonder verboden z-orderconflict. Overige activityfamilies blijven open. [UI-05](../artifacts/kobo/hardware/ui05/20260713T055657Z/manifest.txt) |
| Touch | Close Book naar Home | PASS | [bewijs](../artifacts/kobo/hardware/touch/20260711T2215Z/manifest.txt) op actieve release |
| UI | 100/150/200/250% schaalbaarheid | PARTIAL | Home/Settings alle standen en File Transfer 200% PASS; statuschrome én resterende lijst/menu/tab/covericonen volgen nu één Kobo-schaalroute. Lyra 200% is intern en via Photo Booth gecontroleerd en hersteld; Minimal 200% is intern gecontroleerd. De kern-overlapmatrix is nu PASS; overige kernschermen en oriëntaties blijven open. [UI-05](../artifacts/kobo/hardware/ui05/20260713T055657Z/manifest.txt) |
| UI | OPDS-first bibliotheekgrid | PARTIAL | Home → Library cached de OPDS All Books-feed onder `/.crosspoint/opds-catalog.json` en toont die als 3×2-grid, niet de manual SD-index. Remote titels zijn zichtbaar licht/dithered met Download; offline kopieën hebben normaal contrast. Header-sync vernieuwt de catalogus. Manual EPUB's blijven Browse Files/Recent Books. [bewijs](../artifacts/kobo/hardware/opds-library/20260712T154000Z/manifest.txt). Meerpaginafeeds, remote covers, catalogusfilters en locale-copy removal op N437 blijven open. |
| UI | Kobo Device Info + live netwerkdetails | PASS | [bewijs](../artifacts/kobo/hardware/device-info/20260711T1930Z/manifest.txt) |
| Lezen | EPUB 2/3 tekstbasis: openen, pagineren, herstel | PARTIAL | De actieve N437-release opent en herstelt *The Wise Man's Fear* op de bewaarde positie. Baseline JPEG/PNG, progressive JPEG en image↔text-rondtrips zijn fysiek correct en crashvrij; bredere EPUB2/3-CSS- en formatmatrix blijft open. [bewijs](../artifacts/kobo/hardware/epub-render-p0-force-20260713/retake/) |
| Lezen | EPUB-rendercorrectheid voor afbeeldingen | PASS | Op N437 `541266696da8` zijn baseline JPEG, kleine/grote PNG, de eerder crashende 616×1029 progressive JPEG, gemengde media en brede/staande zelfstandige afbeeldingspagina's correct. De image-only route vult het leesvlak, centreert en behoudt strikt de bronverhouding in portrait, landscape en inverted; PXC1 cold/warm cache, Photo Booth en watchdog 0 zijn bewezen. |
| Lezen | Per-boek leespositie bij boekwissel | PASS | A spine 5/B spine 2, afzonderlijk hersteld |
| Lezen | TXT, BMP, XTC/XTC-H | FAIL | nog geen N437-smokes |
| Lezen | TOC, footnotes, go-to-percent en CSS | PARTIAL | Go-to-Percent heeft een N437 directe 50%-touchtest en schaalvaste balk. Een EPUB met 15 footnotes opende en volgde een voetnoot in één tik. Beeldcorrectheid valt tijdelijk onder de afzonderlijke P0-rendergate; TOC en bredere CSS-matrix blijven open. [footnotes](../artifacts/kobo/hardware/reader-touch-lists/20260712T1300Z/manifest.txt) |
| Lezen | Bookmarks, clippings, stats, recents | PARTIAL | positie en corrupt-recents herstel PASS. Een bookmark springt direct uit de touchlijst terug naar de opgeslagen positie; een clipping opent direct zijn detail en sluit met Back. Directe woordbereikselectie werkt met eerste- en tweede-woordtap, zonder footer-Select. Stats en bredere foutpaden blijven open. [bewijs](../artifacts/kobo/hardware/clip-direct-touch/20260712T1320Z/manifest.txt) |
| Lezen | Custom fonts, hyphenation, kerning, Bionic/themes | PARTIAL | upstreamcode aanwezig; N437-regressie ontbreekt |
| Frontlight | Sysfs 0–100%, reader-menucontrol | PARTIAL | grenzen en directe 30%-tap PASS; de 30%-reader-slider ligt bewezen boven de vaste footer en zet `937/3124`; 10%-normalisatie en wake-restore open. [UI-batch](../artifacts/kobo/hardware/ui01-ui03/20260713T041943Z/manifest.txt) |
| Batterij | Percentage, laden, vol | PARTIAL | 99%/vol/adapter bewezen; ontladen, waarschuwingen en critical shutdown open |
| Power | Sleepscreen met cover/progress, suspend/wake | FAIL | bewust uit op dev-image; alleen als laatste onder USER GATE |
| Wi-Fi | Scan/join, bekende netwerk-autoconnect, statusicoon | PASS | `Legacy` autoconnect en live status bewezen |
| Wi-Fi | Altijd-aan lifecycle tijdens alle netwerkactivities | PARTIAL | N437 bewees een expliciete Wi‑Fi-scan met behoud van `Legacy`, SSH, `wpa_supplicant` en HTTP-upload vóór én na Back. Clock, fonts, OPDS-browser, Calibre en KOReader-auth zijn Kobo-no-teardown. KOReader-sync, webactivity en nearby blijven open. [bewijs](../artifacts/kobo/hardware/network/20260712T0843Z/manifest.txt) |
| Web | Persistente EPUB-upload via USB en Wi-Fi | PASS | HTTP 200, gelijke SHA, geen `.part`, boek geopend |
| Web | Browser file manager basis | PASS | list/upload/download/rename/delete native routes aanwezig; upload fysiek bewezen |
| Web | Progress/cancel/foutmatrix/WebSocket | PARTIAL | basisstatus bestaat; volledige browser- en failureacceptatie ontbreekt |
| Web | WebDAV | FAIL | ESP-handler uitgesloten; geen gelijkwaardige Kobo-implementatie bewezen |
| Netwerk | Calibre wireless | FAIL | ESP-server/lifecycle niet als Kobo E2E bewezen |
| Netwerk | OPDS configuratie, cache, download/TLS | PARTIAL | De Kobo-native route bewaart credentials als mode 0600/geobfusceerd en bewaart nu catalogusmetadata atomair. N437 bewees root → All Books → 39 cached titels, sync, een nieuwe download/open en legacy-downloadmigratie. Meerpaginafeeds, TLS-validatie/fout, cancel, offline foutpad en remote coverdownload blijven open. [bewijs](../artifacts/kobo/hardware/opds-library/20260712T154000Z/manifest.txt) |
| Sync | KOReader progress sync | FAIL | ESP-lifecycle schakelt Wi-Fi uit; E2E ontbreekt |
| Sync | Nearby stats/position | FAIL | ESP-NOW-stubs en vaste fake MAC zijn geen Linux-port; de niet-werkende actie is daarom niet zichtbaar op Kobo. |
| Tools | Screenshots en cachebeheer | PARTIAL | De dev-screenshot leest de canonical framebuffer onder renderlock, zodat bewijsbeelden nooit een render-race mengen. Beschadigde EPUB-omslagcache wordt automatisch gevalideerd, verwijderd en herbouwd op N437, zonder een laadpopup ná de framepresentatie te tekenen. UI-gestuurd cachebeheer en overige tools open. [cachebewijs](../artifacts/kobo/hardware/cover-cache/20260712T1023Z/manifest.txt), [framebewijs](../artifacts/kobo/hardware/library-cover-frame/20260712T1211Z/manifest.txt) |
| Tools | EPUB optimizer en custom-font webflow | FAIL | geen N437 E2E-bewijs |
| Updates | Gesigneerde atomische appupdater + rollback | FAIL | ESP `Check for Updates` en `SD Card Firmware Update` zijn op Kobo verborgen en fysiek afwezig in System; gesigneerde Kobo-updater/rollback ontbreekt nog. [bewijs](../artifacts/kobo/hardware/touch/20260712T0748Z/manifest.txt) |
| USB final | Veilige host-boekoverdracht zonder dubbele mount | FAIL | dev USB-netwerk werkt; final storageflow ontbreekt |
| Hardware | X3 tilt | N/A-hardware | N437 heeft geen tiltsensor |

## Statusdiscipline

Verander een status alleen samen met een regel in
`docs/kobo-n437-test-matrix.md`, exact release-ID, binaryhash en bewijsbestand.
Compileerbaarheid of een simulatorstub is nooit featurebewijs.
