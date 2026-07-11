# Kobo Glo HD N437 Beta 1-testmatrix

Dit bestand is het bewijsregister voor de Beta 1-acceptatie. Markeer een rij
alleen `PASS` wanneer het genoemde artefact werkelijk bestaat en de test op de
fysieke N437 is uitgevoerd. Een compile, simulatorresultaat of verwante Tolino
is geen vervanging voor hardwarebewijs.

| ID | Acceptatie | Verplichte test | Bewijs | Status |
|---:|---|---|---|---|
| 1 | Hele reserve-SD | `diskutil info /dev/disk5`, expliciete bevestiging, whole-disk flash, byte-/SHA-verificatie en eject | `artifacts/kobo/hardware/disk5-flash.txt` | OPEN |
| 2 | CrossInk-first boot | Cold boot filmen/loggen; geen InkBox/Qt-processen of bestanden | `artifacts/kobo/hardware/cold-boot-1/` | OPEN |
| 3 | USB-Ethernet + sleutel-SSH | Nieuwe macOS USB-interface krijgt lease; `ssh crossink-n437 true` zonder prompt | `artifacts/kobo/hardware/usb-ssh.txt` | OPEN |
| 4 | Drie cold boots | Per boot SSH, uptime, kernelversie en bestandsoverdracht vastleggen | `artifacts/kobo/hardware/cold-boot-{1,2,3}/` | OPEN |
| 5 | Moderne kernel + N437-DT | `uname -a`, `/proc/device-tree/model`, `/proc/cmdline`, volledige `dmesg` | `artifacts/kobo/hardware/inventory/` | OPEN |
| 6 | 1072×1448 display | Border-/orientatietest, full/partial/fast, 100 pagina's en ghostingfoto's | `artifacts/kobo/hardware/display/` | OPEN |
| 7 | Volledige touch-UI | Zones, menu's, sliders, toetsenbord en 96px-tweeknopsframe testen | `artifacts/kobo/hardware/touch/` | OPEN |
| 8 | EPUB/TXT offline | Vast corpus openen, pagineren, herstarten en leespositie vergelijken | `artifacts/kobo/hardware/reader/` | OPEN |
| 9 | Frontlight 0–100% | 0/1/25/50/75/100%, suspend-uit en wake-restore | `artifacts/kobo/hardware/frontlight/` | OPEN |
| 10 | Batterij/laadstatus | Ontladen, USB-laden, vol en lage-batterijwaarschuwing vergelijken met sysfs | `artifacts/kobo/hardware/battery/` | OPEN |
| 11 | Tien sleep/wake-cycli | Positie, helderheid en volgende cold-boot-SSH na iedere cyclus | `artifacts/kobo/hardware/suspend/` | OPEN |
| 12 | Eigen recovery | Drie geforceerde vroege exits; recoveryflag, scherm en SSH bewijzen | `artifacts/kobo/hardware/recovery/` | OPEN |
| 13 | Geen secrets | Image mounten/scannen; alleen toegewijde publieke SSH-key toegestaan | `artifacts/kobo/release/secret-scan.txt` | OPEN |
| 14 | Reproduceerbare release | Schone build, manifest, SHA-256, bronpins, commit en herstelhandleiding | `artifacts/kobo/release/` | OPEN |
| 15 | Hoogste stabiele refresh | DU/A2/GC4/GC16 benchmark, thermiek, 1000 pagina's en veilige fallback | `artifacts/kobo/hardware/display/refresh-benchmark.csv` | OPEN |

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
