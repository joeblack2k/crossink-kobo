# Kobo Glo HD N437 Beta 1-testmatrix

Dit bestand is het bewijsregister voor de Beta 1-acceptatie. Markeer een rij
alleen `PASS` wanneer het genoemde artefact werkelijk bestaat en de test op de
fysieke N437 is uitgevoerd. Een compile, simulatorresultaat of verwante Tolino
is geen vervanging voor hardwarebewijs.

| ID | Acceptatie | Verplichte test | Bewijs | Status |
|---:|---|---|---|---|
| 1 | Hele reserve-SD | `diskutil info /dev/disk5`, expliciete bevestiging, whole-disk flash, byte-/SHA-verificatie en eject | `artifacts/kobo/hardware/disk5-flash.txt` | PASS |
| 2 | CrossInk-first boot | Cold boot filmen/loggen; geen InkBox/Qt-processen of bestanden | `artifacts/kobo/hardware/cold-boot-1/` | OPEN |
| 3 | USB-Ethernet + sleutel-SSH | Nieuwe macOS USB-interface krijgt lease; `ssh crossink-n437 true` zonder prompt | `artifacts/kobo/hardware/usb-ssh.txt` | PASS |
| 4 | Drie cold boots | Per boot SSH, uptime, kernelversie en bestandsoverdracht vastleggen | `artifacts/kobo/hardware/cold-boot-{1,2,3}/` | OPEN |
| 5 | Moderne kernel + N437-DT | `uname -a`, `/proc/device-tree/model`, `/proc/cmdline`, volledige `dmesg` | `artifacts/kobo/hardware/inventory/` | PASS |
| 6 | 1072×1448 display | Border-/orientatietest, full/partial/fast, 100 pagina's en ghostingcontrole | `artifacts/kobo/hardware/display/` | PASS |
| 7 | Volledige touch-UI | Zones, menu's, sliders, toetsenbord en 96px-tweeknopsframe testen | `artifacts/kobo/hardware/clip-direct-touch/20260712T1320Z/manifest.txt` + volledige matrix | PARTIAL op `bcc9d5014f5c` — reader, menus, stale transition, Time-to-Sleep, Go-to-Percent, OPDS-lijst→detail, keyboard, bookmarks, footnotes, clippings en woordbereikselectie op N437 bewezen; modals, disabled controls en overige activityfamilies OPEN |
| 8 | EPUB/TXT offline | Vast corpus openen, pagineren, herstarten en leespositie vergelijken | `artifacts/kobo/hardware/reader/n437-reader-corpus.tsv` | EPUB 12/12 PASS op `b833f355706e`; TXT en bredere formatmatrix OPEN |
| 9 | Frontlight 0–100% | 0/1/25/50/75/100%, suspend-uit en wake-restore | `artifacts/kobo/hardware/power/` | OPEN — grenzen PASS, suspend/wake open |
| 10 | Batterij/laadstatus | Ontladen, USB-laden, vol en lage-batterijwaarschuwing vergelijken met sysfs | `artifacts/kobo/hardware/power/` | OPEN — 99%/vol/adapter PASS; ontladen/lage grens open |
| 11 | Tien sleep/wake-cycli | Positie, helderheid en volgende cold-boot-SSH na iedere cyclus | `artifacts/kobo/hardware/suspend/` | OPEN |
| 12 | Eigen recovery | Drie geforceerde vroege exits; recoveryflag, scherm en SSH bewijzen | `artifacts/kobo/hardware/recovery/` | PASS |
| 13 | Geen secrets | Image mounten/scannen; alleen toegewijde publieke SSH-key toegestaan | Lokale release-evidence (niet gepubliceerd) | OPEN |
| 14 | Reproduceerbare release | Schone build, manifest, SHA-256, bronpins, commit en herstelhandleiding | Lokale release-evidence (niet gepubliceerd) | OPEN |
| 15 | Hoogste stabiele refresh | DU/A2/GC4/GC16 benchmark, thermiek, 1000 pagina's en veilige fallback | `artifacts/kobo/hardware/display/refresh-benchmark.csv` | OPEN |
| 16 | Corrupt recent-books herstel | Afgebroken `recent.json` injecteren, reexec, quarantine, nieuwe geldige persist en oorspronkelijke state herstellen | `artifacts/kobo/hardware/persistence/20260711T185848Z/negative-recent-result.txt` | PASS op `c431546cfa12`; watchdog 0 |
| 17 | Bekende Wi-Fi + webupload | Saved-network bootconnect, station-IP, Wi-Fi HTTP-upload, SHA-vergelijking, geen `.part` en EPUB openen | `artifacts/kobo/hardware/wifi/20260711T1912Z/wifi-autoconnect-upload.txt` | PASS op `c5a63fd01b9f`; watchdog 0 |
| 18 | Per-boek leespositie | EPUB A vooruit, EPUB B vooruit, terugwisselen en beide afzonderlijke posities controleren | `artifacts/kobo/hardware/reader/per-book-position-20260711T1916Z.txt` | PASS op `c5a63fd01b9f`; A spine 5, B spine 2, watchdog 0 |
| 19 | Schaalbasis Home + Settings | Op echte N437 Home en Settings op 100/150/200/250 starten; native screenshot per scherm; 200% herstellen | `artifacts/kobo/hardware/scale/20260711T1935Z/manifest.txt` | PASS op `16768646944e`; 8 captures, 1072×1448, tabs/waarden/frame zonder overlap |
| 20 | Kobo Device Info | `Systeem → Apparaat → Apparaat` openen via touch; Wi-Fi-status zichtbaar; Back via touch | Lokale hardware-evidence (niet gepubliceerd) | PASS op `16768646944e`; SSID, IPv4, RSSI, model, kernel, opslag, batterij en frontlight zichtbaar |
| 21 | Direct Close Book | Reader-menu openen en de zichtbare rij `Close Book` direct aanraken; geen tussenstappen of terugkeer naar reader | `artifacts/kobo/hardware/touch/20260711T2215Z/` | PASS op `015acdf001cd`; direct Home zichtbaar |
| 22 | Reader-frontlight direct touch | In reader-menu 30%-segment aanraken; UI en opgeslagen waarde controleren | `artifacts/kobo/hardware/touch/20260711T2215Z/` | PASS op `015acdf001cd` voor 30%; volledige 0–100%-reeks en normalisatie OPEN |
| 23 | Kobo-platformunit-tests | ARM/LXC tests voor transform, gesture, registry, evdev-key, sysfs en packed-mono | buildlog van releasecandidate | 6/6 PASS tijdens audit; releasecandidate-log nog vastleggen |
| 24 | Geen legacy touchqueue | Iedere `GUI.drawList` direct aanraken; bevestig dat `queueLegacyNavigationFallback` niet meer bestaat | `artifacts/kobo/hardware/clip-direct-touch/20260712T1320Z/manifest.txt` + volledige touchmatrix | PARTIAL op `bcc9d5014f5c` — queue verwijderd, single-use targetinvalidate en directe lijstactivatie voor System, Wi‑Fi, OPDS, bookmark, footnote en clipping bewezen. ClipSelection gebruikt nu echte woordtaps; overige activityfamilies OPEN |
| 25 | Netwerklifecycle | Voer clock, fonts, OPDS, Calibre, KOReader en sync uit terwijl bekende Wi-Fi en webserver actief blijven | lokale hardware-evidence (niet gepubliceerd) + netwerk-failurematrix | PARTIAL op `53f2cfd7d48d` — N437 Wi‑Fi-scan, SSH, `wpa_supplicant` en HTTP-upload bleven tijdens én na picker-Back op het test-netwerk beschikbaar; overige flows nog niet E2E |
| 26 | Updatemenu correct | Op Kobo zijn ESP OTA/SD-firmwareacties onzichtbaar; later Kobo-updater success/failure testen | `artifacts/kobo/hardware/touch/20260712T0748Z/manifest.txt` | PARTIAL op `a45ab1335cc8` — System-screen zonder ESP OTA/SD-acties bewezen; Kobo-updater ontbreekt |
| 27 | OPDS-configuratie en catalogus | Server toevoegen, herstarten, geen wachtwoord in API/logs, catalogus openen en een boek downloaden; test ook offline/TLS-fout | `artifacts/kobo/hardware/network/20260712T1242Z/manifest.txt` | PARTIAL op `69467e8f6e81` — configuratie, rootfeed, catalogus, titel en HTTP Basic-auth EPUB-download (5.510.163 bytes) zijn op N437 bewezen; zoeken, vervolgpagina, TLS en offline/cancel blijven OPEN |
| 28 | Beschadigde EPUB-omslagcache | Maak een bestaande 300×450-cache 64 bytes, open Recent library en controleer herbouw, cover en watchdog | `artifacts/kobo/hardware/cover-cache/20260712T1023Z/manifest.txt` | PASS op `b5f6b1e4` — 64/18062-byte BMP verwijderd en automatisch herbouwd; fysieke Photo Booth-preview en framebuffer tonen herstelde cover |
| 29 | Native bibliotheekzoeker | Tik headervergrootglas, voer bestaande en niet-bestaande lokale termen in en controleer resultaatgrid, no-result en cardtouch | `artifacts/kobo/hardware/library-search/20260712T1100Z/manifest.txt` | PASS op `6ef1e9c4` — headerzone/keyboard/no-result/2-van-16 lokale matches fysiek op N437 bewezen |
| 30 | Library-hamburgeroverlay | Open hamburger, toets modal-z-order, sluit buiten paneel en open ten minste één echte bestemming | `artifacts/kobo/hardware/library-menu/20260712T1120Z/manifest.txt` | PASS op `185d2c79` — modal paneel/outside-dismiss/Instellingenroute direct op N437 bewezen |
| 31 | Library sort/filter + coherente screenshot | Tik controls, doorloop Alles/Ongelezen/Voltooid, re-exec en verifieer persistente standaard; screenshot mag nooit twee renderframes mengen | `artifacts/kobo/hardware/library-controls/20260712T1205Z/manifest.txt` | PASS op `6d3491ce39a6` — directe controls, semantisch Ongelezen, Voltooid-lege staat, re-exec-persistentie en renderlocked capture bewezen |
| 32 | Omlagcache-herbouw zonder framebuffervervuiling | Verwijder één gegenereerde 300×450-cache, open Recent library, verifieer herbouw en volledige frame | `artifacts/kobo/hardware/library-cover-frame/20260712T1211Z/manifest.txt` | PASS op `3aa0635e817e` — cache herbouwd tot 18062 bytes; complete libraryframe bleef intact |
| 33 | EPUB-seriesmetadata en cachemigratie | Migreer legacy cache, open boek met Calibre-serie en verifieer persistente recente metadata zonder crash | `artifacts/kobo/hardware/library-series-metadata/20260712T1219Z/manifest.txt` | PASS op `15153322a57b` — Tower of Swallows persistent als `Witcher` vastgelegd; EPUB3-collectietest nog open |
| 34 | Vier functionele librarytabs | Tik Recent, Series en Collecties; verifieer cache-indexering, covergrid, directe routes en persistente metadata | `artifacts/kobo/hardware/library-tabs/20260712T1237Z/manifest.txt` | PASS op `69467e8f6e81` — vier-tabchrome, vier Series-titels en Acceptance-Local-collectie op N437 bewezen |
| 35 | Browse Files en veilige librarypaginering | Tik Browse Files vanaf Home; tik daarna library `Next >` met een progressieve-coverbron en verifieer actieve app/geen nieuw SIGSEGV | `artifacts/kobo/hardware/p0-browse-next/20260712T151143Z/manifest.txt` | PASS op `44283117aff7` — Browse opent FileBrowser; Next blijft draaien en slaat de onveilige progressieve cover veilig over |
| 36 | OPDS-first Library en offline cache | Home → Library, bootstrap All Books, cataloguscache, lichte remote kaart, download/open, legacy-bestand en handmatige sync | `artifacts/kobo/hardware/opds-library/20260712T154000Z/manifest.txt` | PARTIAL op `61bcb10a378` — 39 catalogustitels, remote/light kaart, download/open, cache en sync N437 bewezen; meerpagina/offline/error/removal open |
| 37 | Platform compilematrix zonder simulatorruntime | Bouw desktop simulator, ESP32-default en ARM Kobo apart; controleer Kobo-definities, atomaire release, fysieke Home/Settings en crashpad | `artifacts/kobo/hardware/platform-matrix/20260713T0131Z/manifest.txt` | PASS op `e86fd35855ca`; simulator 2m23s, ESP32 1m54s, ARM en N437-runtime PASS; geen crashbestand |
| 38 | N437 capabilitymodel | Compileer de gedeelde capabilitybridge voor simulator, ESP32 en ARM; voer de ARM-test op de N437 uit en controleer de bootlog op N437 in plaats van X4 | `artifacts/kobo/hardware/platform-capabilities/20260713T0212Z/manifest.txt` | PASS op `dc406d2b92bf`; simulator/ESP/ARM bouwen, capabilitytest draait op N437, bootlog noemt `Kobo Glo HD N437`, watchdog 0 |
| 39 | Linux HalSystem en gecontroleerd crashherstel | Verifieer echte available memory, monotone tijd, gecontroleerde `exec` en één afgeschermde fatal-signal-herstelroute; laat de health-window de watchdog weer wissen | `artifacts/kobo/hardware/platform-system/20260713T0248Z/manifest.txt` | PASS op `d940c18936d2`; 442–451 MiB echte beschikbare memory, re-exec herstelt Inkspell, SIGABRT bewaart PC/LR-report en keert via CrashActivity terug; watchdog 0 na healthy window |
| 40 | Kobo target dependency audit | Preprocess de finale ARM `compile_commands.json`; controleer dat ESP-NOW, OTA-partities, Preferences, deep-sleep, ESP.restart en ESP-WebServer niet actief zijn; meld iedere tijdelijke compatheader | `artifacts/kobo/hardware/platform-dependency-audit/20260713T035130Z/manifest.txt` | PASS op `d4417ae9bf98`; 205 Kobo-objecten, 8 actieve risicorepresentanten, geen verboden actieve dependency; één expliciet gelogde tijdelijke POSIX-webserver-ABI onder open PLAT-01; N437 Home/Photo Booth/watchdog 0 |
| 41 | Home-current-book directe touch | Tik de zichtbare huidige kaart voor twee verschillende EPUB's; verifieer EpubReader-route en bestaande positie zonder footeractie | `artifacts/kobo/hardware/ui01-ui03/20260713T041943Z/manifest.txt` | PASS op `03921b5374bfe`; Inkspell en Season of Storms direct geopend; Season herstelde spine 3, watchdog 0 |
| 42 | Reader-frontlight zonder footeroverlap | Open reader-menu, controleer 11 sliderrects boven footer, tik 30% en tik de tussenruimte | `artifacts/kobo/hardware/ui01-ui03/20260713T041943Z/manifest.txt` | PASS op `03921b5374bfe`; target 30 zet `937/3124`, gap-tap heeft geen target/framewijziging |
| 43 | Schaalbare headerstatus | Capture Lyra op 100/150/200/250% en alle actieve thema's op 200%; herstel Lyra 200% | `artifacts/kobo/hardware/ui01-ui03/20260713T041943Z/manifest.txt` | PASS op `03921b5374bfe`; zeven thema's werkelijk herstart, interne captures plus Lyra-Photo-Booth, watchdog 0 |
| 44 | Resterende zichtbare icoongeometrie | Audit vaste 16/20/24/32px-bronnen; bouw simulator/ESP/ARM, capture Lyra/Minimal op 200% en verifieer fysieke N437; herstel Lyra 200% | `artifacts/kobo/hardware/ui04/20260713T051825Z/manifest.txt` | PASS op `4d78f6076d38`; gecentraliseerde lijst/menu/tab/covermetrics, interne captures en Photo Booth zonder clipping, watchdog 0. |
| 45 | Directe reader-open sanity | Tik de Inkspell-kaart, controleer fysieke coverpagina, open menu met centrale tap en kies `Close Book` | `artifacts/kobo/hardware/ui04/20260713T051825Z/manifest.txt` | PASS op `4d78f6076d38`; Photo Booth toont de fysieke cover en het menu, registry 22 menu- en daarna 8 Home-regio's, watchdog 0. Zwarte PBM was geen Pearl-fout. |
| 46 | Touch-overlapmatrix | Audit Home, Library, modal, Settings, reader-menu, toetsenbord, dialog en vaste footer via registry; verifieer een echte modal-dismiss en Settings-footer-tap | `artifacts/kobo/hardware/ui05/20260713T055657Z/manifest.txt` | PASS op `845e7256d278`; 8/17/6/15/22/45/3 actieve regio's met telkens 0 verboden overlap; fysieke Photo Booth-dialog en watchdog 0. |
| 47 | EPUB-afbeeldingsrendering als P0-gate | Wis alleen de cache van elk corpusboek; vergelijk cold/warm cache voor JPEG, PNG, SVG en image↔text terug/vooruit via interne capture én Photo Booth | `artifacts/kobo/hardware/epub-render-p0-force-20260713/retake/` | PASS op `541266696da8` — baseline JPEG (*Wise Man's Fear*), kleine/grote PNG (Inkheart/*Heroes*), progressive JPEG (Maggie), gemengde JPEG/PNG, image↔text-rondtrips, corrupte-EPUB-foutafhandeling en brede/staande image-only EPUB-pagina's zijn op N437 bewezen. De image-only proef is herhaald in portrait, landscape en inverted; cold/warm PXC1-cache, interne captures, live Photo Booth en watchdog 0 zijn vastgelegd. De lokale gehashte corpus bevat geen SVG-bron; er is geen fictieve SVG-test toegevoegd. |

## Aanvullende regressiematrix

Leg voor ieder relevant onderdeel online, offline en foutgedrag vast:

- portrait, landscape en inverted;
- tap, drag, long press, randzones en gedeactiveerde knoppen;
- simpele, complexe, image-heavy, Unicode en beschadigde EPUB; TXT en BMP;
- frontlightgrenzen, laden, kritieke batterij en veilige shutdown;
- suspend, wake, reboot, procescrash en onverwachte power loss;
- USB-netwerk versus host-storage zonder gelijktijdige schrijfmount;
- Wi-Fi STA/AP, TLS-fout, OPDS, Calibre, KOReader Sync, WebDAV en upload;
- updater: succes, verkeerde signature, volle SD, stroomonderbreking en rollback;
- volle data-partitie, corrupte cache, ontbrekend boek en beschadigde config.

## Bewijsregels

- Sla tekstlogs als UTF-8 op met UTC-tijd, gitcommit en image-SHA.
- Bewaar foto's onder dezelfde testcase met een korte beschrijving van wat
  zichtbaar moet zijn.
- Noteer een afwijking als `FAIL`; verander die pas na een herhaalde test.
- Laat `OPEN` staan zolang echt hardwarebewijs ontbreekt.
