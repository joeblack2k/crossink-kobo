# Uitvoeringsroadmap

## Algemene strategie

Werk in verticale, bewijsbare slices. Iedere slice bevat:

1. oorzaakbevestiging;
2. minimale architectuurwijziging;
3. unit-/integratietests;
4. build en static checks;
5. hardwaretest wanneer vereist;
6. documentatie- en logupdate;
7. één of meer atomische commits.

Start geen volgende risicovolle slice voordat de huidige basis groen is. Paralleliseer alleen onafhankelijke inspectie, testontwerp of afgebakende modules.

## Branch- en commitbeleid

- Werk niet rechtstreeks op `main`.
- Maak vanaf de vastgelegde HEAD een branch, bijvoorbeeld:

```text
hardening/kobo-beta4
```

- Geen force-push of history rewrite.
- Commit per coherent issue of kleine issuegroep.
- Gebruik herkenbare prefixes, bijvoorbeeld:

```text
touch: publish hit regions atomically
touch: add captured semantic gesture events
input: recover evdev after SYN_DROPPED
wifi: move scan and DHCP out of main loop
web: make websocket uploads atomic
power: reset touch state across suspend
ci: run Kobo host tests on pull requests
release: align Kobo beta metadata and docs
```

- Houd `K4_12_EXECUTION_LOG.md` bij na iedere commit.
- Push de branch en open een draft-PR wanneer GitHub-auth beschikbaar is.

## Fase 0 — baseline en beschermrails

Doelen:

- actuele HEAD en omgeving vastleggen;
- bestaande tests en snelle builds draaien;
- huidige failures onderscheiden van regressies;
- relevante callgraphs inventariseren;
- branch aanmaken;
- issue-register kopiëren naar het uitvoerlog met alle statussen op `OPEN`.

Geen productcode wijzigen voordat de baseline is vastgelegd.

## Fase 1 — atomische touchpublicatie

Issues: `TCH-01`, `TCH-02`.

Werk:

- staging/active-registry of gelijkwaardige immutable snapshot invoeren;
- volledige render als één framegeneration committen;
- activity-instance-ID of navigation-generation toevoegen;
- stale targetconsumptie weigeren;
- `consumeTouchTarget()` mag de actief getoonde registry niet globaal wissen;
- concurrentie-, stale-frame- en activity-transitiontests toevoegen.

Gate:

- input kan nooit een half opgebouwd frame zien;
- snelle tweede tap op een oud scherm kan niets activeren;
- bestaande directe touchroutes blijven groen.

## Fase 2 — semantische touch en gestures

Issues: `TCH-03`, `TCH-04`, `TCH-05`, `TCH-10`.

Werk:

- expliciete gesturetypen invoeren: `Tap`, `LongPress`, `Swipe`, `Cancelled`;
- target op touch-down capturen;
- swipe annuleert captured target;
- long-press blijft een targetevent en wordt niet gesimuleerd als nulduur-Confirm;
- begrensde semantische eventqueue toevoegen;
- legacy-knopinjectie alleen als adapter voor nog niet gemigreerde activities behouden;
- bibliotheekkaart, settingslijst, option dialog, keyboard en readerzone als eerste consumers testen.

Gate:

- één tik is één actie;
- long-press is één long-pressactie;
- swipe opent nooit het startitem;
- burstinput houdt volgorde of faalt zichtbaar bij expliciete queue-overflow.

## Fase 3 — robuuste evdev-laag

Issues: `TCH-07`, `TCH-08`, `TCH-09`, `CI-02`.

Werk:

- kernel-eventtimestamps gebruiken;
- `EVIOCSCLOCKID(CLOCK_MONOTONIC)` proberen;
- getypeerd readresultaat invoeren;
- `EINTR`, `EAGAIN`, EOF, korte reads en protocolfouten afhandelen;
- `SYN_DROPPED` discard/resync implementeren;
- reconnect met bounded back-off;
- type-B slotstate alleen implementeren wanneer capabilities dit vereisen, maar parser mag slots niet verkeerd mengen;
- volledige E2E-inputtest bouwen.

Gate:

- gesimuleerde eventachterstand verandert holdduur niet;
- deviceverlies laat geen permanent `down` achter;
- `SYN_DROPPED` veroorzaakt geen spookactie.

## Fase 4 — main loop, autosleep, sysfs en Wi-Fi

Issues: `TCH-06`, `SYS-01`, `SYS-02`, `NET-01`, `NET-02`, `NET-03`, `MAINT-01`.

Werk:

- één `hadUserActivityThisFrame()`-contract invoeren;
- refreshqualification per procesrun cachen;
- batterij- en USB-state gecacht/paced pollen;
- Wi-Fi-scan, DHCP en reconnect als non-blocking lifecycle uitvoeren;
- geen `system()`/`popen()` in hot loop;
- link-/association-/IP-status combineren;
- powersavebeleid aan actieve transfer koppelen;
- meetbare loop- en inputlatencytelemetry toevoegen zonder logspam.

Gate:

- touch-only gebruik voorkomt autosleep;
- geen multi-seconde inputstall door scan/DHCP;
- idle server laat WLAN-powersave terugkeren;
- headers blijven correct bij connect/disconnect.

## Fase 5 — webbeveiliging en atomische I/O

Issues: `WEB-01`, `WEB-02`, `WEB-03`.

Werk:

- werkelijke listen-addresses en exposure inventariseren;
- veilige transportpolicy kiezen: USB-only default of Wi-Fi-pairing/token;
- muterende routes beschermen;
- WebSocket- en fontuploads via tijdelijke bestanden en atomische replace;
- oorspronkelijke file behouden bij disconnect, overflow, schrijf-, sync- of validatiefout;
- path-, size-, CSRF- en tokennegativetests toevoegen;
- geen credentials of tokens loggen.

Gate:

- ongeauthenticeerde LAN-client kan geen muterende actie uitvoeren;
- afgebroken overwrite behoudt byte-identieke oude file;
- voltooide upload heeft verwachte hash en geen `.part`-restanten.

## Fase 6 — display, fallback en suspend

Issues: `DSP-01`, `DSP-02`, `DSP-03`, `PWR-01`, `PWR-02`, `CAL-01`.

Werk:

- één refreshpolicybron behouden;
- dirty region ook door FBInk uitvoeren;
- runtime DRM reopen/failover ontwerpen;
- wake-render tot één gecontroleerde full-present terugbrengen;
- touch/inputqueue/capture vóór suspend annuleren en na wake resyncen;
- calibratietool eerlijk maken of runtimekalibratie implementeren;
- eerst lokale/gesimuleerde tests, daarna beperkte hardwaretests en pas vervolgens soak.

Gate:

- geen stale touch na wake;
- geen dubbele zichtbare refresh;
- backendfout heeft gecontroleerde recovery en telemetry;
- hardware soak volgens validatiematrix.

## Fase 7 — CI, release en documentatie

Issues: `CI-01`, `REL-01`, `REL-02`.

Werk:

- Kobo host-CMake/CTest verplicht maken;
- ARM/Buildroot-build in passende CI-cadence toevoegen;
- README en issueformulier Kobo-specifiek maken;
- versievelden harmoniseren;
- één RC-manifest en testmatrix op dezelfde SHA produceren;
- secretscan en repository cleanliness uitvoeren.

Gate:

- PR-CI vangt een bewust geïnjecteerde Kobo-testfailure;
- geen Beta 4-label vóór alle P0-gates;
- releasebeslissing onderbouwd met één RC-SHA.

## Parallelisatievoorbeelden

Maximaal drie Luna Max-subagents kunnen nuttig parallel werken aan:

1. touchcallgraph en registry-invarianten;
2. evdev-testcorpus en Linux-inputcontract;
3. Wi-Fi/web threat model en atomic-I/O-tests.

De Luna Max-hoofdagent bepaalt de uiteindelijke architectuur, integreert patches, draait tests en accepteert geen subagentresultaat zonder eigen inspectie.
