# Kobo iteratiebatches

Dit is het compacte uitvoerlog voor de snelle iteratiewerkwijze. Een batch
krijgt pas `PASS` na ARM-build, atomaire deploy en gericht N437-bewijs.

## B-0xx — display scheduler, telemetry en fallback — PARTIAL

- **Start:** 2026-07-13T00:40:00+02:00.
- **Roadmappunten:** DISP-03, DISP-05 en DISP-07.
- **Scope:** alleen bestaande N437 DRM/FBInk-routes en CPUFreq-OPP's. Geen
  spanning, VCOM, waveformblob, PLL of ongevalideerde klokwijziging.
- **Basisrelease:** `dev-phase0-ab6d6cc35ff8`, SHA
  `ab6d6cc35ff8c508d0ef433e8bb38778087534934706af6013f5002b1b0850e6`.
- **Implementatie:** `KoboRefreshScheduler` bepaalt diff-regio, coalescing,
  Safe-budget, recovery-full en profielterugval. De DRM-adapter kopieert en
  meldt alleen het veranderde 8-pixel-uitgelijnde gebied. Telemetrie noteert
  submitduur expliciet als submitduur; het gepinde DRM-UAPI biedt geen
  completionfence.
- **Hosttests:** refresh-profile, CPUFreq en refresh-scheduler PASS op LXC.
- **N437-release:** `dev-display-scheduler-0769b35b8572`, SHA
  `0769b35b8572b85f5fb24fe4a1f120cc7dba54cf3a6ee7581e1c8ada3bfe129c`.
- **N437-resultaat:** twaalf Home/Settings-overgangen bewezen het vijf-partial
  Safe-budget en de verplichte GC16-herstelrefresh; crashcounter bleef 0.
  DU-smoke 1/10/100/1000: 289 ms / 3.622 s / 42.378 s / 411.256 s;
  SoC 50,8→55,3 °C, nul EPDC/DRM-waarschuwingen. App herstartte gezond.
  De aanvullende `performance`-governorsoak op uitsluitend de bestaande 996
  MHz-OPP doorstond 1000 DU-updates in 385.281 s, piekte op 56,5 °C en
  herstelde aantoonbaar automatisch naar `ondemand` vóór de appstart.
  Een echte EPUB-readerreeks met tien pagina's vooruit en tien terug bleef
  op Safe/DU draaien, forceerde de verwachte GC16-herstelcycli en liet de
  crashcounter op 0. De eindpagina is zowel als PBM als via Photo Booth
  vastgelegd.
  De opvolgende executorrefactor draait op
  `dev-refresh-executor-bd1202aa3da9`; boot, DRM-presentatie, interne
  screenshot en Photo Booth bleven stabiel met crashcounter 0.
- **Vervolgrelease:** `dev-fast-profile-20260712T235527Z-29dd9a6056a5`, SHA
  `29dd9a6056a580327109cd8cf6baa011d9efd5e907ef2e2d029259d7f77fd1bf`;
  app- en afzonderlijke smoke-toolbuild PASS. De tool accepteert 0 ms delay
  en bindt een eventuele Fast-marker aan model, kernel, policy-ABI en actieve
  binaryhash. Na een normale app-stop opent de losse DRM-tool de kaart echter
  niet betrouwbaar opnieuw (FBInk errno 95); de volgende appstart gaf DRM
  errno 13 tot een SSH-reboot. Die reboot herstelde Safe/DRM, Photo Booth en
  watchdog 0. Er is daarom geen Fast-marker geschreven; DISP-03/04/05 blijven
  terecht DNF in de actieve roadmap.
- **Bewijs:** `artifacts/kobo/hardware/display/20260713T0042Z/` bevat de
  internal PBM-captures, log en Photo Booth-na-soak-capture.
- **Nog open:** Fast/MaxBeta zijn correct verborgen maar nog niet
  device-specifiek gesoakt; GC4/A2 hebben geen gedocumenteerde, veilige
  userspace-UAPI op deze gepinde driver.

## B-001 — EPUB-omslagcache-integriteit — PASS

- **Fix:** valideer pixeldataomvang van cached BMP's, zowel in de library als
  centraal vóór EPUB-thumbnailgeneratie.
- **Oorzaak:** een 64-byte BMP met geldige header werd door EPUB als volledig
  gezien; de renderer kreeg pas tijdens tekenen een short-read.
- **Modules:** `Bitmap`, `RecentBooksGridActivity`, `Epub`.
- **Lokale controle:** gerichte broninspectie en `git diff --check`.
- **Waarom ARM-build:** renderer/cacheopslag en echte N437-bestandsgedrag.
- **N437-resultaat:** PASS op `dev-cover-cache3-b5f6b1e4`, SHA
  `b5f6b1e4f5c1977288afc64647039fd285e0e176da3d1891d3aecb7f70efedd9`.
  De 64-byte testcache werd uit de EPUB herbouwd tot 18.062 bytes; framebuffer
  én Photo Booth bevestigen de herstelde omslag.
- **Bewijs:** [cover-cache manifest](../artifacts/kobo/hardware/cover-cache/20260712T1023Z/manifest.txt).

## B-002/B-003 — mockupbasis en lokale bibliotheekzoeker — PASS

- **Fixes:** Nederlandse mockuplabels; offline titel/auteur/pad-filtering;
  directe vergrootglasknop; elke nieuwe zoekactie start leeg; Nederlandse
  status `Ongelezen`.
- **Oorzaak:** de bestaande grid had X4/Engelse labels en geen lokale,
  touch-native zoekroute; een verborgen oude query maakte herhaald zoeken
  onbetrouwbaar.
- **Modules:** `RecentBooksGridActivity`, `KeyboardEntryActivity`-integratie,
  `TouchUiRegistry`.
- **Lokale controle:** broninspectie, `git diff --check`, targetregistratie.
- **Waarom ARM-build:** direct touch, keyboard-resultstack en e-ink-layout.
- **N437-resultaat:** PASS op `dev-library-search2-6ef1e9c4`, SHA
  `6ef1e9c493288cf2f87f52d566863262ab4f930b11afb0ac7b2a507174928c9b`.
  Query `all` reduceerde 16 lokale boeken naar twee zichtbare kaarten;
  no-result, framebuffer en Photo Booth-preview gecontroleerd.
- **Bewijs:** [library-search manifest](../artifacts/kobo/hardware/library-search/20260712T1100Z/manifest.txt).

## B-004 — functionele library-hamburger — PASS

- **Fix:** in-place tactiele modal met alleen bestaande activiteiten als
  bestemmingen; gedeelde compact-header ondersteunt een linker actie-inset.
- **Oorzaak:** de mockup vereiste primaire navigatie, maar een decoratief icoon
  zonder route was niet acceptabel.
- **Modules:** `CompactHeader`, `RecentBooksGridActivity`, `TouchUiRegistry`.
- **Lokale controle:** broninspectie, `git diff --check`, painter-ordercontrole.
- **Waarom ARM-build:** modal z-order en directe touchroutes zijn hardwaregedrag.
- **N437-resultaat:** PASS op `dev-library-menu-185d2c79`, SHA
  `185d2c79521cf438c3a1c7436a79a3a69567d12ae30fb6481132a2f1b2bdd99a`.
  Openen, outside-dismiss en Instellingenroute zijn bewezen.
- **Bewijs:** [library-menu manifest](../artifacts/kobo/hardware/library-menu/20260712T1120Z/manifest.txt).

## B-005 — librarycontrols en betrouwbare N437-capture — PASS

- **Fixes:** serialiseer de dev-framebuffercapture met `RenderLock`; maak
  `Ongelezen` semantisch correct (geen opgeslagen voortgang of 0%); houd de
  lege librarytekst in de Nederlandse Kobo-chrome consistent.
- **Oorzaak:** de diagnosekopie las de canonical framebuffer soms tijdens de
  renderthread erin schreef, waardoor bewijsbeelden een niet-bestaande
  gemengde UI konden tonen. Daarnaast gedroeg “Ongelezen” zich als “niet
  voltooid”, wat niet overeenkomt met de zichtbare label.
- **Modules:** `platform/kobo/app/kobo-main.cpp`,
  `RecentBooksGridActivity`.
- **Lokale controle:** `build-native-app-check` PASS; `git diff --check` PASS.
- **Waarom ARM-build:** de mutex werkt alleen betekenisvol tegen de echte
  FreeRTOS-renderthread en de nieuwe filter moet op echte progresscaches worden
  gecontroleerd.
- **N437-resultaat:** PASS op `dev-library-controls-8b4ca03d-1404`, SHA
  `6d3491ce39a6be99c43c70b797e8cf316f7708e64c571e80b81cd02cee041d26`.
  Volledig coherent all-grid, Ongelezen- en Voltooid-staat, terugzetten op
  Alles/Recent en een re-exec-persistentiecheck zijn bewezen.
- **Bewijs:** [library-controls manifest](../artifacts/kobo/hardware/library-controls/20260712T1205Z/manifest.txt).

## B-006 — omslagcache verandert geen gepresenteerde libraryframe — PASS

- **Fix:** verwijder de laadpopup- en progressdraws uit het
  post-present-cacheonderhoud; vraag na eventuele herbouw één nieuwe volledige
  activity-render aan.
- **Oorzaak:** `loadPageCovers()` liep na `displayBuffer()` en kon daardoor de
  gedeelde framebuffer alsnog wijzigen. Een volgende refresh of diagnose kon
  dan een popupfragment in plaats van de library tonen.
- **Module:** `RecentBooksGridActivity`.
- **Lokale controle:** `build-native-app-check` PASS; `git diff --check` PASS.
- **Waarom ARM-build:** alleen de echte N437-cache, renderthread en EPDC-pad
  kunnen aantonen dat onderhoud de gepresenteerde frame niet vervuilt.
- **N437-resultaat:** PASS op `dev-library-cover-frame-8b4ca03d-1410`, SHA
  `3aa0635e817e2059e57d96f8fac9f794b7ee9c0e64898764a1c91001705ef6aa`.
  Een verwijderde 300×450-cache is herbouwd tot 18.062 bytes; de captured
  library bleef volledig intact.
- **Bewijs:** [cover-frame manifest](../artifacts/kobo/hardware/library-cover-frame/20260712T1211Z/manifest.txt).

## B-007 — echte serie-/collectiemetadata voor librarytabs — PARTIAL

- **Fixes:** parseer Calibre `calibre:series`/`calibre:series_index` en EPUB3
  `belongs-to-collection`; bewaar metadata in metadata-cache v9 en
  `recent.json`; exposeer velden vanuit `Epub` aan de readeropslag.
- **Oorzaak:** een Series- of Collecties-tab zonder boekmetadata zou alleen
  decoratie zijn en niet voldoen aan de mockupacceptatie.
- **Modules:** `ContentOpfParser`, `BookMetadataCache`, `Epub`,
  `RecentBooksStore`, `EpubReaderActivity`.
- **Lokale controle:** `build-native-app-check` PASS; `git diff --check` PASS.
- **Waarom ARM-build:** cacheversie-upgrade en echte EPUB-herindexering moeten
  op de N437 worden bewezen.
- **N437-resultaat:** datalaag PASS op
  `dev-library-series-meta-8b4ca03d-1418`, SHA
  `15153322a57bfe02191c1ab14b84fa14bd1eeaccb75c202972a1872a134d0f7e`.
  De bestaande Tower of Swallows-EPUB herindexeerde veilig en staat persistent
  als `Witcher`. De zichtbare tab-UI en een corpus met EPUB3-collectie blijven
  expliciet open.
- **Bewijs:** [series-metadata manifest](../artifacts/kobo/hardware/library-series-metadata/20260712T1219Z/manifest.txt).

## B-008 — vier functionele librarytabs — PASS

- **Fixes:** persistente `seriesIndex`; lokale map als collectiefallback;
  RECENT/BOEKEN/SERIES/COLLECTIES-routes met Series-indexering en sortering.
- **Oorzaak:** de mockup had vier primaire bibliotheeksecties; de oude OPDS-tab
  week visueel af en bood geen local-series/collectionroute.
- **Modules:** `RecentBooksStore`, `RecentBooksGridActivity`,
  `ActivityManager`.
- **Lokale controle:** `git diff --check` PASS. De macOS native checker is
  extern geblokkeerd door Brew/libdrm en verouderde Xcode CLT; de gezaghebbende
  ARM-Buildroot-build PASS.
- **Waarom ARM-build:** cacheherindexering, directe touch en covergridgedrag
  zijn apparaatgedrag.
- **N437-resultaat:** PASS op `dev-library-tabs-index-8b4ca03d-1436`, SHA
  `69467e8f6e8155c13db351dbedbdd1fdfcf990d82eb9739fbb981fa02e780287`.
  Recent, Series en Collecties zijn rechtstreeks aangeraakt; echte content en
  metadata verschenen zonder restart of crash.
- **Bewijs:** [library-tabs manifest](../artifacts/kobo/hardware/library-tabs/20260712T1237Z/manifest.txt).

## B-009 — OPDS-browse en download op echte N437 — PASS (afgebakende slice)

- **Geen codewijziging:** dit is de ontbrekende hardware-E2E van de bestaande
  Kobo-native OPDS-client, na herstel van de testserver.
- **N437-resultaat:** de geconfigureerde HTTP Basic-auth catalogus leverde de
  rootfeed, All Books-catalogus en een echte titel. De native Download-actie
  schreef daarna één EPUB van 5.510.163 bytes weg, met SHA-256
  `83cf93e00ad703e7b4b0f5abdc5bd70c153d9b0f78c5c01e198dc7298996d277` en
  zonder achterblijvend `.part`-bestand.
- **Bewijs:** [OPDS E2E-manifest](../artifacts/kobo/hardware/network/20260712T1242Z/manifest.txt).
- **Expliciet niet afgerond:** zoeken, vervolgpagina's, TLS- en offline/cancel-
  foutpaden. Die blijven als één compacte open netwerkregel staan; geen verder
  OPDS-polijstwerk zonder aanleiding.

## B-010 — readerlijsten directe touch — PASS (afgebakende slice)

- **Fixes:** maak voetnoten, bookmarks en clippings echte één-tap-lijsten via
  `DirectListTouch`; reserveer het permanente 96px Kobo Back/Select-frame in
  plaats van daar nog rijen te tekenen.
- **Oorzaak:** deze readeractiviteiten behielden X4-focus-then-Select-semantiek,
  waardoor aanraken geen actie uitvoerde of kon botsen met de footer.
- **Modules:** `EpubReaderFootnotesActivity`, `EpubReaderBookmarkListActivity`,
  `EpubReaderClippingListActivity`.
- **Lokale controle:** gerichte broninspectie en `git diff --check` PASS.
- **Waarom ARM-build:** de wijzigingen raken inputrouting, renderrects en de
  echte N437 footersemantiek.
- **N437-resultaat:** PASS op `dev-reader-touch-lists`, SHA
  `a730fd4fe43d1afa70bf41792ad9fb3fb7983ce5c63242ccf7aafbdc28393119`.
  Bookmark-sprong, voetnoot-open en clipping-detail openden elk in één tik;
  Back sloot het detail zonder dead-end.
- **Bewijs:** [reader-touch-lists manifest](../artifacts/kobo/hardware/reader-touch-lists/20260712T1300Z/manifest.txt).
- **Open vervolg:** ClipSelection zelf blijft cursorgebaseerd en is bewust niet
  als touch-native claim opgenomen.

## B-011 — directe EPUB-woordselectie voor clippings — PASS

- **Fixes:** voeg `TextSelectionSurface` toe aan de Kobo-touchboodschap, geef
  de werkelijke tapcoördinaat door en hit-test woorden lokaal in
  `ClipSelectionActivity`. Eerste tik markeert, tweede tik bewaart de range.
- **Oorzaak:** een pagina kan meer woorden bevatten dan de vaste registry van
  64 regio's; de oude cursorroute was X4-only bediening.
- **Modules:** `TouchUiRegistry`, `MappedInputManager`, `kobo-main`,
  `ClipSelectionActivity` en registrytest.
- **Lokale controle:** `git diff --check` PASS; de macOS C++-test is door de
  verouderde Command Line Tools niet uitvoerbaar. ARM-Buildroot-build PASS.
- **N437-resultaat:** PASS op `dev-clip-direct-touch`, SHA
  `bcc9d5014f5c44bb7f78cd214c6a420faf923a874c6c6a9af13f414d30339095`.
  De N437 markeerde `Originally` en maakte na tik op `published` een
  opgeslagen twee-woorden-clipping zonder footer-Select.
- **Bewijs:** [clip direct-touch manifest](../artifacts/kobo/hardware/clip-direct-touch/20260712T1320Z/manifest.txt).

## B-012 — Browse Files-route, veilige librarypaginering en Engelse chrome — PASS (P0)

- **Fixes:** directe Home-taps krijgen nu de selectoroffset ná recente boeken;
  library-footerteksten zijn `Previous`, `Page x of y` en `Next`; de Kobo
  weigert progressieve JPEG's vóór de instabiele JPEGDEC-MCU-route.
- **Oorzaak:** Browse Files zette index 0 direct als selector en opende daardoor
  het eerste recente boek. De Nederlandse `Volgende`-tap laadde een progressieve
  omslag en kon de N437 met SIGSEGV herstarten.
- **N437-resultaat:** Browse Files ging direct naar `FileBrowser`. De
  bibliotheek-`Next >`-tap hield hetzelfde aantal crashrecords en het proces
  bleef actief; de probleemomslag is zichtbaar als veilige skip gelogd. De
  geconfigureerde OPDS-stroom root → All Books → titel downloadde eveneens
  zonder nieuwe crash en de appchrome is Engels.
- **Bewijs:** [P0-manifest](../artifacts/kobo/hardware/p0-browse-next/20260712T151143Z/manifest.txt).
- **Expliciete beperking:** progressieve JPEG-coverthumbnails zijn tijdelijk
  niet beschikbaar; dit is een veilige degradatie, geen volledige coverdecode.

## B-013 — OPDS-first Library en offline-cataloguscache — PARTIAL

- **Fixes:** persistente `OpdsCatalogStore`; OPDS-cover/acquisitionmetadata;
  Library-route met bootstrap via All Books; gestippelde remote kaarten;
  directe download naar `/Books/OPDS/<server-key>/`; syncactie; niet-destructieve
  koppeling van de oude rootdownloads.
- **N437-resultaat:** 39 echte catalogusboeken werden apart van manual EPUB's
  getoond. Eén remote kaart downloadde, werd offline beschikbaar en opende in
  de reader. Een oude Inkspell-download bleef op zijn originele pad en werd
  aan de juiste OPDS-entry gekoppeld. Handmatige sync bleef crashvrij.
- **Bewijs:** [OPDS-library manifest](../artifacts/kobo/hardware/opds-library/20260712T154000Z/manifest.txt).
- **Open vervolg:** meerpaginafeeds, remote coverdownload, offline foutmelding,
  lokale-kopie-verwijdering op hardware en catalogusfilters vereisen nog een
  afzonderlijke N437-acceptatie.

## B-014 — veilige Kobo-afbeeldingspagina's — PASS (Inkspell P0)

- **Fixes:** verwijder de Xteink-specifieke dubbele FAST-refresh voor EPUB-
  afbeeldingen op Kobo en render gecachete pixels via de begrensde
  `GfxRenderer` in plaats van de directe framebufferwriter.
- **Oorzaak:** de directe writer/FAST-volgorde liet op het echte Pearl-paneel
  slechts een smalle, deels geïnverteerde strook van Inkspell achter.
- **N437-resultaat:** de bekende Inkspell-titelpagina rendert nu volledig als
  één coherent omslagbeeld; de fysieke Photo Booth-controle bevat geen zwarte
  strook of half frame. De pagina duurt 380 ms inclusief de veilige HALF-
  refresh.
- **Release:** `dev-image-safe-1da0920e`, SHA
  `1da0920e053e59faba7e3d1b9cb7cbc47c716fbf20baa1639826e1b3b1022771`.
- **Vervolg:** voer de image-heavy corpusmatrix uit voordat algemene
  afbeelding/CSS-pariteit als volledig wordt gemarkeerd.

## B-015 — afbeeldingspagina naar tekstpagina — PASS (Inkspell P0)

- **Fix:** na een pagina met een afbeelding dwingt de eerstvolgende gewone
  tekstpagina één `FULL_REFRESH` af op de Kobo; daarna keert de ingestelde
  normale refreshcadans terug.
- **Oorzaak:** de framebuffer was correct, maar de Pearl-partial-refresh kon
  donker afbeeldingspigment niet volledig naar een witte tekstachtergrond
  terugdrijven. Daardoor bleef de omslag als verticale strook zichtbaar.
- **N437-resultaat:** Inkspell-afbeeldingspagina → `Next Page` → tekstpagina
  is fysiek met Photo Booth gecontroleerd: geen resterende coverstrook of
  inversie. Release `dev-image-transition-3c6c8862`.
- **Corpusvoorcontrole:** 27 van de 28 lokale EPUB's zijn ZIP-integriteit OK;
  `Mythos_ A Retelling of the Myths of Ancien - Stephen Fry.epub` heeft een
  ZIP-fout en wordt niet als rendererregressie aangemerkt.

## B-016 — idle pre-index van één volgend hoofdstuk — PASS

- **Fix:** tijdens een onafgebroken leespauze van 15 seconden maakt Kobo één
  SD-cache voor uitsluitend de volgende spine. Geen popup, geen wijziging van
  de actieve sectie en geen volledige boekcache in RAM.
- **Guardrails:** geen preload tijdens auto-page-turn, footnote-preview,
  renderlock of lage heap; een falende/afwijkende spine wordt maar éénmaal per
  positie geprobeerd.
- **N437-resultaat:** Inkheart bouwde na idle tijd spine 1 (57 pagina's) op
  de achtergrond als cache. De bestaande Inkspell image→text P0-regressie
  blijft opgelost. Release `dev-next-chapter-idle-60004476`.

## B-017 — zelfstandige Kobo/POSIX-buildgrens — PASS

- **Fixes:** verwijder `SIMULATOR` uit de N437-definities; gebruik
  `CROSSPOINT_POSIX` alleen voor basiscompatibiliteit en `KOBO_LINUX` voor
  hardware. Zet ESP-NOW, ESP Wi-Fi-events, USB-CDC en ESP power-save buiten
  het Kobo-pad.
- **Compilematrix:** na synchronisatie van de actuele worktree bouwden de
  Debian-LXC-simulator (2m23s), ESP32-C3 default (1m54s) en ARMv7/musl Kobo
  alle drie. De matrix ontdekte en herstelde shared-settings guards,
  simulator-persistence en onbedoelde Kobo-frontlight/DeviceInfo-referenties
  in niet-Kobo builds.
- **N437-resultaat:** atomisch gedeployed als
  `dev-platform-matrix-20260713T012946Z-e86fd35855ca`, SHA
  `e86fd35855cae56d55ea4e7c81e32753094400f7546e00f2641c50e8fc4d6ab1`.
  Home en Settings renderden op het fysieke scherm zonder crashbestand.
- **Bewijs:** [platform-matrix manifest](../artifacts/kobo/hardware/platform-matrix/20260713T0131Z/manifest.txt).

## B-018 — N437-capabilitymodel — PASS

- **Fixes:** vervang de Kobo-X4-impersonatie door `HalGPIO::Capabilities` en
  een gedeelde `DeviceCapabilities`-bridge. Migreer alle productiecallers in
  settings, header, thema's, readerdialogs, main en webstatus naar die bridge.
- **Oorzaak:** direct X3/X4-gedrag in gedeelde code kon op de Glo HD zijknoppen,
  RTC en apparaatnaam onjuist afleiden.
- **Buildmatrix:** Debian-LXC `pio run -e simulator` PASS; `pio run -e default`
  PASS; Buildroot ARM-rebuild PASS. De capabilitytest compileerde voor ARM en
  werd op de N437 zelf uitgevoerd met resultaat `capabilities-test=PASS`.
- **N437-resultaat:** release
  `dev-platform-capabilities-20260713T0208Z-dc406d2b92bf`, SHA
  `dc406d2b92bfcaaae4615e7ce8de0ba148c5466346682b57a875f6820467b4e7`.
  Bootlog meldt `Hardware detect: Kobo Glo HD N437`; Home is intern en via
  Photo Booth gecontroleerd, watchdogcounter 0.
- **Bewijs:** [capabilitymanifest](../artifacts/kobo/hardware/platform-capabilities/20260713T0212Z/manifest.txt).

## B-019 — echte Linux HalSystem — PASS

- **Fixes:** Kobo gebruikt nu `HalSystem` voor `/proc`/`sysinfo` memory,
  monotone tijd, gecontroleerde re-exec en crashdiagnostiek. De Kobo-
  ESP-compatibiliteit geeft dus geen vaste 1 MiB heap meer terug.
- **N437-resultaat:** geheugenlog toont 442–451 MiB beschikbare memory. Een
  gecontroleerde re-exec herstelde Inkspell via de normale readerroute. Eén
  root-only SIGABRT-proef schreef PC/LR-detail naar het crashreport, herstartte
  via de supervisor in CrashActivity en eindigde na de health-window met
  watchdog 0.
- **Release:** `dev-platform-system-crash-20260713T0246Z-d940c18936d2`, SHA
  `d940c18936d26b47f2556dab5a780062d772b25f8733e8f4abc9b07044773946`.
- **Bewijs:** [platform-system manifest](../artifacts/kobo/hardware/platform-system/20260713T0248Z/manifest.txt).

## B-020 — platformneutrale schedulergrens — START 2026-07-13T02:51Z

- **Scope:** PLAT-05. Vervang voor Kobo de directe FreeRTOS-renderworker-,
  lock-, wait- en cancellationsemantiek door een kleine POSIX-schedulerlaag,
  met behoud van de bestaande ESP32-route.
- **Eerste batch:** verwijder overbodige FreeRTOS-includes uit EpubReader- en
  StatusBarSettings-headers; KOReader gebruikt op Kobo de Linux-systeemklok
  in plaats van ESP-SNTP met `vTaskDelay`. Simulator (3m19s), ESP32 (7m13s),
  ARM en N437 Home-regressie PASS op
  `dev-plat05-compat-20260713T0321Z-728d25ee20c0`.
- **Resultaat:** DNF — zie de concrete `ActivityManager`-schedulerblokkade in
  de actieve roadmap; de volledige workerrefactor is niet als kleine veilige
  wijziging binnen deze tijdslimiet te leveren.

## B-021 — Kobo dependency audit — PASS

- **Scope:** PLAT-06. Controleer de werkelijke N437-CMake-objectselectie in
  plaats van alle ESP-broncode met een ruwe zoekopdracht te verwarren.
- **Fixes:** CMake exporteert `compile_commands.json`; de nieuwe audit
  preprocesses risicodragende objecten met hun echte Kobo-definities. De audit
  verwijderde twee verborgen ESP-OTA-activities en de simulator-firmwarestub
  uit de Kobo-target. De overblijvende native transfer-ABI-header is expliciet
  tijdelijk gedocumenteerd voor PLAT-01.
- **Resultaat:** ARM-build PASS; finale audit PASS voor 205 Kobo-objecten.
  Atomisch deployed als
  `dev-plat06-dependency-audit-20260713T0351Z-d4417ae9bf98`, SHA
  `d4417ae9bf98121c7b1beec05c518c92300ff01b0947b6804e9b2e098c7f2a0d`.
  Home was intern en fysiek stabiel, Wi-Fi/webtransfer startten en watchdog
  bleef 0. Bewijs: [dependency-audit manifest](../artifacts/kobo/hardware/platform-dependency-audit/20260713T035130Z/manifest.txt).

## B-022 — Home-card, frontlight en statuschrome — PASS

- **Scope:** UI-01. Registreer de volledig zichtbare Home-boekkaart als één
  directe Kobo-touchactie; de kaart moet de actuele opgeslagen readerroute
  openen zonder een footer- of menuactie te injecteren.

- **UI-02 batch:** de reader-frontlightslider reserveert nu eerst de vaste
  96px-footer via de screen-safe-area en laat daarboven extra tastbare ruimte;
  de elf getekende segmenten gebruiken exact dezelfde, verhoogde hitrects.

- **UI-03 batch:** Base, Lyra, Minimal en Rounded Raff halen batterij- en
  Wi-Fi-geometrie nu uit de actieve `ThemeMetrics`; de drieboogs-Wi-Fi-slot
  groeit met de batterij mee in plaats van vast op 32 px te blijven.

- **Buildmatrix:** Debian-LXC simulator en ESP32-default PASS; Buildroot ARM
  PASS. De finale dependency-audit preprocesste 205 Kobo-objecten en PASSte.
- **N437-resultaat:** release `dev-ui01-ui03-20260713T041943Z-03921b5374bf`,
  SHA `03921b5374bfe3fb50c7a0b7b53ccef772c11badff95e280d53d881e04fff8b5`.
  Directe Home-taps heropenden Inkspell en Season of Storms; Season herstelde
  spine 3. De 30%-slider raakte alleen target 30 (`937/3124`); de vrije strook
  boven de footer veranderde niets. Lyra 100/150/200/250% en alle zeven
  thema's op 200% renderden zonder crash, waarna Lyra 200% is hersteld.
  Interne captures en Photo Booth zijn opgeslagen; watchdog 0.
- **Bewijs:** [UI-batch manifest](../artifacts/kobo/hardware/ui01-ui03/20260713T041943Z/manifest.txt).

## B-023 — resterende vaste icoongeometrie — START 2026-07-13T06:37Z

- **Scope:** UI-04. Inventariseer alle zichtbare 16/20/24/32px-iconen buiten
  de al bewezen statuschrome en ontwerp één schaalroute die icoon, raster en
  hitbox bij elkaar houdt.

- **Uitkomst:** PASS voor de audit en de visueel bereikbare schaalroute.
  `KoboIconMetrics` is de enige runtimebron voor lijst-, menu-, tab- en
  coverplaceholdermaten. Lyra/Lyra3/Carousel/Minimal/RoundedRaff/Dashboard en
  de reader-tabbar zijn daarop overgezet. De simulator-, ESP32- en geforceerde
  ARM-appbuild zijn groen; release `dev-ui04-20260713T051825Z` met SHA
  `4d78f6076d384a935b01315aa8f58164aa420a0ef284801d2a1bb7f0ea5657b1`
  staat atomair op N437. Lyra 200% is hersteld, interne captures en Photo Booth
  zijn bewaard, watchdog 0. Zie `artifacts/kobo/hardware/ui04/20260713T051825Z/`.
  Een aansluitende Inkspell-proef bevestigde via Photo Booth dat de zwart
  ogende PBM geen fysieke readerstall was: directe kaarttap, reader-menu en
  Close Book → Home werken op dezelfde release.

## B-024 — touch-overlapmatrix — PASS 2026-07-13T08:02Z

- **Scope:** UI-05. Maak verborgen raakvlakconflicten reproduceerbaar en
  faalbaar in plaats van afhankelijk van stille last-drawn-wins-prioriteit.
- **Fixes:** registryaudit met opt-in-z-order-exceptie; Home-onderste rij
  begrensd bij footer; Library-header beperkt tot de headerband; modal Library
  wist onderliggende targets vóór registratie van panel en dismiss-scrim.
- **Build:** registry-hosttest PASS; simulator en ESP32 PASS in de UI-05-batch;
  finale geforceerde ARM-appbuild PASS.
- **N437-resultaat:** Home 8/0, Library 17/0, Library-modal 6/0, Settings
  15/0, reader-menu 22/0, keyboard 45/0 en option-dialog 3/0
  (`regions/forbidden_overlaps`). Buiten-tik sluit het modal en de zichtbare
  Home-Settingsfooter opent direct Settings. Photo Booth livepreview werd
  opnieuw bevestigd en toont de fysieke dialog correct; watchdog 0.
- **Release/bewijs:** `dev-ui05-20260713T055657Z`, SHA
  `845e7256d2786680b962c69567bfa743257e31b3c72a6399c0bb036b86172b8b`;
  [manifest](../artifacts/kobo/hardware/ui05/20260713T055657Z/manifest.txt).

## B-025 — OPDS achtergrond-jobqueue — START 2026-07-13T08:05Z

- **Scope:** OPDS-01. Isoleer catalogusrefresh, coverfetch, boekdownload en
  reconcile als observeerbare, annuleerbare, suspend-aware Kobo-jobs buiten
  activiteiten en zonder render- of touchwachttijd.
- **Uitkomst:** DNF uitsluitend op de fysieke suspend-race. `OpdsSyncService`
  bezit één Kobo-worker met catalogus-, cover-, boekdownload- en reconcilejobs;
  resultaten gaan terug naar de activity-thread, progress is observeerbaar en
  cancel/suspend zijn coöperatief. N437 haalde root en All Books op en downloadde
  `The Wise Man's Fear` (1.625.551 bytes) naar de OPDS-map, markeerde hem
  offline en opende hem direct in de reader; watchdog 0. De development-Kobo
  heeft auto-sleep bewust uit en een suspend midden in transfer vereist een
  fysieke wake-gate, dus dit is geen PASS.
- **Release:** `dev-opds01-suspend-20260713T061810Z`, SHA
  `d1c8668d4050a7ef0edde5cf85a0e24e911dd5670500413c03dd764648fe05e7`.

## B-026 — OPDS snapshotidentiteit en portalcontract — PASS 2026-07-13T08:35Z

- **Scope:** OPDS-03 en OPDS-04, plus het bestaande portalcontract dat deze
  serveridentiteit gebruikt.
- **Doel:** een volledige catalogussnapshot moet serververwijderingen
  deterministisch verwerken zonder lokale EPUB's/leesstatus los te koppelen;
  een wijziging van server-URL mag geen nieuwe bibliotheek veroorzaken.
- **Uitkomst:** persistente server-ID's migreren non-destructief vanuit de
  bestaande URL-hash; catalogusboeken krijgen snapshotgeneratie en
  remote-present-status. De portal gebruikt dezelfde ID-, primary- en
  sync-all-velden. Serie, index, updated, coverrel en acquisitiontype worden
  uit OPDS-feeds bewaard. De N437 draait de atomair gedeployde release
  `dev-opds03-04-migrate-20260713T072037Z`, SHA
  `a8196f09038afc2cbd2a236dcd7cce3c578e2a65a9ffd201a13d15434419baeb`;
  bewijs staat in `artifacts/kobo/hardware/opds03-04/20260713T072037Z/`.

## B-027 — reproduceerbare native vendorpatch — PASS 2026-07-13T07:20Z

- **Scope:** PLAT-01. Vervang de marker-only vendorpatchskip door een volledige
  controleerbare patchroute en filter macOS `._*`-metadata uit de Linux
  sourceglobs.
- **Resultaat:** een schone clone accepteerde de volledige patch en produceerde
  exact dezelfde vendorbron-delta (`7e5567573372d005c95aaf2f71ba24b667d1b843626291d1d3f9e41935d6bb6`).
  De LXC gebruikte dezelfde delta en de N437 draait de atomair gedeployde
  release `dev-opds03-04-migrate-20260713T072037Z`, SHA
  `a8196f09038afc2cbd2a236dcd7cce3c578e2a65a9ffd201a13d15434419baeb`, met
  watchdog 0.

## B-028 — fysieke schermopschoning bij activity-entry — PASS 2026-07-13T08:20Z

- **Scope:** zichtbare ghosting achter de Home-boektitel/kaart na grote
  schermwissels, zonder gewone reader-paginawissels naar GC16 te promoveren.
- **Implementatie:** de policy-only `KoboRefreshScheduler` heeft een
  one-shot clean-refreshverzoek. Dit wordt pas na een geslaagde full-panel
  refresh verbruikt en overleeft een mislukte submit. `ActivityManager`
  vraagt het declaratief aan voor Home, Settings en Library; reader-paginering
  blijft op de bestaande snelle/budgetgestuurde route.
- **Tests:** `crossink-kobo-refresh-scheduler` PASS op LXC, inclusief retry
  na failure. De lokale macOS PlatformIO-simulator faalt vóór de projectcode
  omdat de aanwezige Apple Command Line Tools geen `cstdint`/`atomic`
  systeemheaders aanbieden; dit is apart toolchainwerk en geen Kobo-regressie.
  De geforceerde ARM-appbuild PASS.
- **N437:** atomaire release `dev-disp-clean-entry-20260713T082041Z`, SHA
  `3b96575a42e85307a10e3fcbd80a59f99965278cdf3a2e971c47b4cc49149393`.
  Eén Home → Settings → Home en vijf vervolgcycli bleven op PID 3206 zonder
  watchdogrestart. Interne PBM's bewijzen het bedoelde Homeframe; na een
  noodzakelijke Photo Booth-herstart toont de fysieke capture geen residu van
  de vorige titel/kaart. Bewijs:
  `artifacts/kobo/hardware/display-clean-entry/20260713T081833Z/`.

## B-029 — OPDS covercache en Library-jobroute — START 2026-07-13T10:33Z

- **Scope:** OPDS-05, vervolgens compatibel OPDS-06/07 in dezelfde buildbatch.
  Eerst de bestaande coverstroom auditen: `OpdsSyncService` heeft al één
  background `CoverFetch`-job, maar `RecentBooksGridActivity` slaat remote
  catalogusboeken nog expliciet over en genereert lokale covers synchroon per
  pagina. De eerste implementatiestap is daarom een gedeelde, atomische
  covercache op basis van stabiele server-/book-ID's, met één actieve download,
  decoder-validatie en per-kaartinvalidatie.

## B-030 — EPUB-afbeeldingsrenderer als P0 — START 2026-07-13T11:15Z

- **Trigger:** de N437 Photo Booth-capture en de canonical framebuffercapture
  van *The Wise Man's Fear* tonen dezelfde fout: een kaart is naar rechts
  verschoven en afgeknipt, met een grotendeels leeg readercanvas. Dit is geen
  stale camera-preview en geen enkel ghostingartefact.
- **Vermoedelijk pad:** eerste JPEG/PNG-decode en cacheweergave gebruikten
  verschillende pixelvertalingen. De rendergate controleert nu expliciet dat
  cold-cache en warm-cache door hetzelfde orientation/bounds- én
  gray-plane-pad gaan.
- **Status:** open. Geen PASS totdat een gebundelde ARM-release en een
  fysieke corpusproef de kaart, image→text-overgang en terugnavigatie correct
  bewijzen.

- **Codebatch 2026-07-13T12:28Z:** de Kobo-first JPEG/PNG-route gebruikt nu
  dezelfde `GfxRenderer`-transformatie als cacheweergave. Pixelcaches hebben
  een expliciete `PXC1`-header, 2-bits-versiecontrole, exacte
  lengteinvariant en `.part`-naar-eindbestand-publicatie; een corrupte cache
  wordt alleen voor de betreffende afbeelding verwijderd. Mislukte/missende
  afbeeldingen wissen hun eigen contentrect en tonen een lokale placeholder
  in plaats van oude framebufferdata.
- **Correctie 2026-07-13T12:36Z:** ook de warm-cache-route gaat nu via
  `DirectPixelWriter` op Kobo. Die writer delegeert naar `GfxRenderer` en
  behoudt daarmee identiek de bestaande BW/GRAYSCALE_MSB/GRAYSCALE_LSB-logica;
  de eerdere verkorting `pixelValue < 3` kon de twee grijsplanes verliezen.
- **Hostbewijs:** Debian-LXC native Kobo-configuratie linkt de complete batch
  als `crossink-kobo`, SHA
  `c3245bb7c573f7126ee10442c0da2f767bcec954ea0088902954690736f15f45`.
  Dit is nadrukkelijk geen N437-PASS.
- **ARM-build en devicebewijs 2026-07-13T14:30Z:** de macOS-case-sensitive
  kernelcache is verplaatst naar een Docker-volume en de app-force-rebuildflag
  wordt nu expliciet naar de buildcontainer doorgegeven. Geïnstalleerde
  N437-release `dev-epub-render-p0-force-20260713T1430Z`, SHA-256
  `fa53702bb07be5c82873e77205dcd96a8c2d4f62d71a4ec5e9a5d1ce8294c329`.
  *The Wise Man's Fear* slaat de niet-renderbare SVG-cover veilig over,
  decodeert de eerste JPEG en de kaart op spine 7 volledig, en rendert die
  kaart na re-exec opnieuw uit de PXC1-cache. Inkheart's 823×1251 JPEG-cover
  is ook cold-cache fysiek volledig. Bewijs:
  `artifacts/kobo/hardware/epub-render-p0-force-20260713/`.
- **Hercontrole 2026-07-13T14:41–14:52Z:** een verse portrait-PBM van de
  herstelde Wise-Man-kaart is correct; de eerdere lezing als “volledig zwart”
  was een foutieve beoordeling van de rotatie/conversie, niet een export- of
  rendererdefect. Inkheart spine 4/page 18 bewijst een 115×186 8-bit PNG:
  cold decode schrijft 5.404-byte PXC1-cache; na re-exec is de contentrect
  bit-identiek aan de warme cache (alleen 142 klokpixels verschillen). Vijf
  text↔PNG-rondtrips renderen met AUTO/GC16 zonder crash. Een bewust
  leeggemaakte, uitsluitend testcache-afbeelding toont een lokale
  `Placeholder`; app-PID en watchdog blijven gezond. Portrait-foto via Photo
  Booth en interne captures staan in `epub-render-p0-force-20260713/retake/`.
- **Oriëntatiecontrole 2026-07-13T14:58–15:01Z:** dezelfde Wise-Man-kaart
  staat in portrait, landscape en inverted volledig binnen de paneelranden.
  Dit is zowel als canonical framebuffer vastgelegd als fysiek via Photo
  Booth; landscape lijkt in de camera zijwaarts omdat het apparaat zelf niet
  gedraaid is, niet door een rendererfout. De eindtoestand is portrait,
  oorspronkelijke readerpositie spine 7/page 0 en watchdog 0.

- **Cachefoutproef 2026-07-13T14:55Z:** uitsluitend de testkopie van Inkheart
  kreeg een drie-byte `img_4_0.pxc`. De N437 meldt `Cache header is truncated`,
  verwijdert alleen die cache, decodeert de originele 115×186 PNG opnieuw en
  publiceert weer exact 5.404 bytes PXC1. Een daarna herstelde Wise-Man-reader
  staat op spine 7/page 0; watchdog blijft 0. Bewijs:
  `retake/inkheart-png-truncated-cache-recovery.{log,png}` en
  `retake/original-wise-man-final-restore.log`.

- **EPUB-01 bewijs 2026-07-13T15:16Z — PASS:** de testkopie van *The Wise
  Man's Fear* (alleen `epub_10043541832670543240`) is op spine 7/page 0 met
  gewiste section- en `img_7_*.pxc`-cache geopend. De koude JPEG-decode schreef
  een 81.018-byte PXC1-cache; een normale dev-reexec op exact dezelfde positie
  gebruikte die warme cache. De vergelijker vond buiten de statusklok nul
  gewijzigde contentpixels. De live Photo Booth-capture toont dezelfde
  volledige kaart zonder clipping. Inkheart- en Heroes-PNG leveren dezelfde
  nul-diff; fysieke Wise-Man-captures bevestigen portrait, landscape en
  inverted. Release `dev-epub-render-p0-force-20260713T1430Z`, SHA
  `fa53702bb07be5c82873e77205dcd96a8c2d4f62d71a4ec5e9a5d1ce8294c329`,
  watchdog 0. Het reguliere OPDS-boek is daarna hersteld op spine 7/page 0.
  Bewijs: `artifacts/kobo/hardware/epub-render-p0-force-20260713/retake/wise-jpeg-{cold,warm}-rerun.*`,
  `photobooth-wise-jpeg-warm-rerun.png`, en de bestaande PNG/orientatiebestanden.

- **Groot-PNG-corpusbewijs 2026-07-13T15:03–15:08Z:** *Heroes* spine 7
  rendert een 1900×1442, 8-bit PNG-kaart naar 1056×801 op target `(8,57)`.
  Cold decode plus cachepublicatie duurde 1000 ms; na Home → heropenen laadt
  exact dezelfde PXC1-cache in 334 ms. Interne screenshots en Photo Booth
  tonen de gehele kaart, labels en randen zonder clipping of oude pixels.
  Daarna is *The Wise Man's Fear* opnieuw hersteld op spine 7/page 0,
  portrait, met watchdog 0. Overige corpusrollen en SVG blijven expliciet
  open; B-030 is dus nog geen PASS.

- **Blocker 2026-07-13T15:19Z — progressive JPEG:** de nieuw gekopieerde,
  SHA-256-gecontroleerde testkopie van *Maggie Blue and the Dark World* bevat
  42 progressive JPEG's. Op spine 1/page 1 werd een geldige 616×1029 JPEG
  als progressive gedetecteerd en op 1/8-scale gestart; direct daarna viel
  `crossink-kobo` met SIGSEGV 11 (`pc offset 0x298928`). Dit is een echte
  readercrash, geen corrupte EPUB of Photo Booth-probleem. Inspectie toonde
  dat de twee bestaande `JPEGDecodeMCU_P`-MCU_SKIP-patches alleen via de
  PlatformIO-hook werden toegepast. `scripts/kobo/apply-vendor-patches.sh`
  past ze nu idempotent toe vóór elke Buildroot-appconfiguratie. De fix is
  pas geldig na één verse ARM-build, cold/warm progressive-herhaling,
  fysieke capture en crashcounter 0; tot dan is B-030 expliciet FAIL.

- **EPUB-03 N437 bewijs 2026-07-13T15:41–15:51Z — PASS:** de actuele ARM
  release `dev-progressive-jpeg-fix-20260713T1543Z` (SHA
  `1e8dde0c2717b7189911c820b834a94c255486deaeff1b7a076a6d0699dbc56b`)
  decodeert Maggie's eerder crashende 616×1029 progressive JPEG veilig naar
  333×556 en schrijft 46.714 bytes PXC1. Een re-exec gebruikt die cache;
  koude en warme logical content-area zijn pixelidentiek (statusbar/footer
  uitgesloten) en watchdog blijft 0. Een drie-byte PXC op uitsluitend de
  testkopie is verworpen en opnieuw opgebouwd. Met uitsluitend het
  uitgepakte testbronbeeld tijdelijk weg toont de fysieke N437 een begrensde
  `Placeholder`; daarna is het bestand teruggezet zonder stale cache. Interne
  PBM's, logs en herstartte-live-Photo-Booth-captures staan in
  `artifacts/kobo/hardware/epub-render-p0-force-20260713/dev-epub-progressive-safe-20260713T1341Z/`.

- **EPUB-04 N437 bewijs 2026-07-13T15:52–15:57Z — PASS:** Maggie's
  TestCorpus-kopie doorliep vijf beeld→tekst→beeld-rondtrips en vijf
  beeld→beeld-rondtrips. Elke DRM-present slaagde (`result=1`); de
  beeld→tekst-cleanup was GC16 en de terugkeer naar beeld AUTO. Er was geen
  polariteitsomslag, leeg canvas, footer-corruptie, crash of
  watchdog-incident. Photo Booth is vóór de eindopname herstart; de fysieke
  Pearl-opname toont geen zichtbaar restbeeld. Bewijs:
  `artifacts/kobo/hardware/epub-render-p0-force-20260713/dev-epub-progressive-safe-20260713T1341Z/maggie-image-{text,image}-five-roundtrips.*`
  en `photobooth-maggie-image-text-five-roundtrips-final-live.png`.

- **EPUB-05 N437 bewijs 2026-07-13T16:01–16:04Z — PASS (image-only
  layout):** release `dev-epub-fullpage-image-20260713T1601Z`, SHA-256
  `541266696da8b77fd5c49e89815b9c59d8c94659ecb34ddf234844c72f1ff253`,
  herbouwt section caches (v45) en herkent een pagina met precies één
  afbeelding. Die afbeelding krijgt het hele reader-leesvlak, wordt
  gecentreerd en behoudt altijd de bronaspectratio; inline afbeeldingen en
  image+text-pagina's houden hun bestaande EPUB/CSS-layout. Een doelgerichte
  twee-spine EPUB bewijst op de echte N437 een brede bron `1807×736` naar
  `1056×430` en een staande bron `400×600` naar `834×1251`. De koude decode,
  warme PXC1-cacheheropening, canonical PBM en levende Photo Booth-capture
  zijn correct; er is geen crop, stretch, crash of watchdog-incident. Het
  originele *The Wise Man's Fear* staat daarna weer op zijn bewaarde positie
  (`spine=9`, `page=11`). Bewijs:
  `artifacts/kobo/hardware/epub-render-p0-force-20260713/retake/fullpage-image-{wide,portrait,portrait-warm}.*`
  en `photobooth-fullpage-image-portrait.png`.

- **EPUB-06 N437 bewijs 2026-07-13T16:08–16:12Z — PASS (gemengd/foutpad):**
  de gehashte TestCorpus-kopie `test_mixed_images.epub` doorliep een JPEG,
  een PNG en een spine met JPEG gevolgd door PNG; elke decode schreef een
  geldige PXC1-cache en alle presents eindigden met `result=1`. Een afzonderlijke
  64-byte-kopie `test_mixed_images-corrupt.epub` rapporteert `Failed to load
  epub` en keert lokaal terug naar Home; proces en watchdog bleven gezond.
  Dit valideert alleen het foutpad, niet een nieuwe build. Daarna is *The Wise
  Man's Fear* teruggezet op `spine=9`, `page=11`. Bewijs:
  `retake/mixed-media-six-forward.{log,pbm,png}`, `corrupt-epub-clean-attempt.log`
  en `original-wise-man-restored-after-corrupt-pass.log`.

- **EPUB-07 N437 bewijs 2026-07-13T16:18–16:23Z — PASS (oriëntaties):**
  de standalone staande 400×600-image-only pagina is via de echte portalsetting
  geopend in portrait, landscape CW en portrait 180°. De renderer rapporteert
  respectievelijk viewport/target `1056×1251 → 834×1251`,
  `1426×881 → 587×881` en opnieuw `1056×1251 → 834×1251`; de bronverhouding
  bleef exact behouden. Interne PBM's en een levende Photo Booth-preview tonen
  volledige, gecentreerde inhoud zonder clipping of polariteitsfout. Daarna
  herstelde de portal de reader naar portrait en *The Wise Man's Fear* naar
  `spine=9`, `page=11`; watchdog 0. De lokale corpusscan bevat geen SVG-EPUB,
  daarom is geen verzonnen SVG-case als eis opgevoerd. Bewijs:
  `retake/orientations/{landscape,inverted,restore-original-portrait}.*`.

## B-031 — PLAT-02 schone Kobo-portalreproductie

- **Start 2026-07-13T16:30Z:** reproduceer de gepinde native portalpatch vanuit
  een volledig schone vendorcheckout en lege Buildroot-output. Het doel is een
  verifieerbare bronboom en een route-smoke; er wordt geen bestaande Kobo- of
  gebruikerdata gewijzigd.
