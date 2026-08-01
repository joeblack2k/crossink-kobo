# Projectstatus en auditbaseline

## Samenvatting

CrossInk-Kobo is geen vroege proof-of-concept meer. De port heeft een bruikbare productbasis met een eigen Buildroot-image, moderne N437-kernel, native Kobo-displaypad, touch, frontlight, batterijstatus, Wi-Fi, USB-netwerk, recovery, watchdog, EPUB-reader en een groot deel van de gedeelde CrossInk-interface.

De resterende risico's liggen vooral op grenzen tussen threads, inputframes, Linux-subsystemen en overgenomen ESP/Xteink-semantiek. De meeste fouten zijn daarom niet zichtbaar tijdens rustig normaal lezen, maar bij snelle gestures, een repaint tijdens touch, eventachterstand, netwerkactiviteiten, suspend/resume of een afgebroken upload.

## Aantoonbaar waardevolle bestaande delen

Deze onderdelen moeten als regressiegates worden behandeld en niet opnieuw worden geport:

- Buildroot- en kernelbasis voor N437;
- CrossInk-first boot;
- recovery/watchdog;
- USB-Ethernet en key-only SSH;
- native 1072×1448-logische interface;
- DRM-displaypad met FBInk-fallback;
- basis full/partial/DU-refresh;
- framebuffer- en screenshotlocking;
- zForce-touchmapping en oriëntatietransform;
- touch-hitboxregistry en directe touch voor meerdere activityfamilies;
- frontlight via sysfs;
- batterij- en USB-status via power_supply;
- bekende-Wi-Fi-autoconnect;
- persistente HTTP/webtransfer;
- EPUB-basispaginatie, cache, covers, afbeeldingen en per-boekpositie;
- OPDS-catalogusbasis;
- crashrapportage en gecontroleerde re-exec;
- target-aware dependency-audit.

## Belangrijkste architectuurgrenzen

### 1. Renderthread versus inputthread

De renderthread bouwt de visuele interface en publiceert touchgebieden. De main/inputthread leest die gebieden en injecteert acties. Mutexen beschermen individuele operaties, maar een volledig scherm moet ook als één atomische generatie worden gepubliceerd.

### 2. Touch versus legacy-knoppen

De gedeelde activities zijn historisch knopgestuurd. Kobo-touch wordt gedeeltelijk terugvertaald naar virtuele knopedges. Dat is nuttig als compatibiliteitslaag, maar onvoldoende voor swipe, long-press, pointer capture en snelle multi-eventreeksen.

### 3. Linux-state versus ESP-semantiek

Wi-Fi, webserver, sleep en restart zijn niet dezelfde operaties op Linux als op ESP32. Kobo moet een expliciete Linux-lifecycle hebben en mag geen ESP-aannames erven via compatheaders of gedeelde activitycode.

### 4. E-ink policy versus backend

De centrale scheduler hoort te bepalen welk gebied en welke waveform wordt gebruikt. DRM en FBInk moeten die beslissing uitvoeren, niet elk hun eigen extra policylaag onderhouden.

## Bekende release-inconsistenties

De functionele staat past bij Beta 3, maar delen van de bron noemen nog Beta 1. Controleer en harmoniseer minimaal:

- `CROSSINK_VERSION`;
- Buildroot package version;
- changelogheading;
- README-productomschrijving;
- issueformulier;
- release- en artifactnamen;
- testmatrixnaam;
- buildmanifest.

Versienummers mogen pas naar Beta 4 wanneer de Beta 4-gates werkelijk zijn gehaald.

## Verplichte baseline-inventarisatie door de agent

Leg vóór de eerste wijziging vast in `K4_12_EXECUTION_LOG.md`:

```text
repository=
branch=
head_sha=
submodule_status=
working_tree_clean=
build_host=
compiler=
cmake=
buildroot=
connected_n437=
n437_model=
n437_kernel=
active_binary_sha=
recovery_available=
usb_ssh_available=
```

Voer daarna de bestaande, niet-destructieve snelle checks uit. Noteer ieder bestaand rood resultaat als baseline; maskeer het niet met de nieuwe veranderingen.

## Broncode als uiteindelijke waarheid

Wanneer deze documenten en de actuele broncode van elkaar afwijken:

1. bevestig de afwijking in code en tests;
2. bepaal of de bron een nieuwere reparatie bevat;
3. werk het issue-register en uitvoerlog bij;
4. verwijder geen gate zonder bewijs dat het onderliggende risico niet meer bestaat.
