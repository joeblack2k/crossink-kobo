# Beta 4 issue-register

## Statuswaarden

- `OPEN` — nog niet aangepakt.
- `IN_PROGRESS` — actieve code- of testwijziging.
- `FIXED_LOCAL` — code en lokale tests groen, nog geen vereiste hardwaretest.
- `PASS` — alle verplichte tests en evidence op dezelfde release-SHA geslaagd.
- `BLOCKED_USER_GATE` — uitsluitend een expliciete hardware-/veiligheidsgate ontbreekt.
- `WONT_FIX` — alleen toegestaan met technische onderbouwing en expliciete gebruikersacceptatie.

## P0 — releaseblokkerend

| ID | Onderwerp | Probleem | Minimale acceptatie |
|---|---|---|---|
| TCH-01 | Atomische touchregistry | `clear()` en incrementele registratie kunnen een leeg of half opgebouwd scherm publiceren. | Staging/commit of gelijkwaardige atomische framepublicatie; concurrentietest; geen half frame resolveerbaar. |
| TCH-02 | Frame- en activityvalidatie | Generation wordt opgeslagen maar niet hard afgedwongen; target kan na activity- of framewissel stale zijn. | Target bevat frame-ID en activity-ID; consumptie faalt veilig bij mismatch; gerichte tests. |
| TCH-03 | Gestureprecedentie | Een swipe die op een hitbox begint kan als tap op die hitbox eindigen. | Alleen Tap/LongPress mogen target activeren; swipe annuleert capture; E2E-test. |
| TCH-04 | Directe long-press | DirectListTouch injecteert press/release in één frame, waardoor echte holdsemantiek verloren gaat. | Semantische `TapTarget` en `LongPressTarget`; boekkaart-long-press opent actie zonder footer of tweede tap. |
| TCH-05 | Gesture-deadband | 25–71 px beweging is noch tap noch swipe en verdwijnt stil. | Expliciete cancelled gesture; grenswaardetests 23/24/25/48/71/72/73 px. |
| TCH-06 | Touch als user activity | Touch reset de algemene autosleep-timer niet betrouwbaar. | Iedere geldige touch-down, tap, swipe en long-press telt als activiteit; autosleeptest. |
| TCH-07 | Evdev-timestamps | Kernel-eventtijd wordt genegeerd; queueachterstand kan tap/hold verkeerd classificeren. | Monotone evdev-tijd gebruiken met veilige fallback; achterstandstest. |
| TCH-08 | `SYN_DROPPED` | Verloren evdev-events worden niet geresynchroniseerd. | Discard/resync-state machine; actieve gesture annuleren; test met geïnjecteerde `SYN_DROPPED`. |
| TCH-09 | Inputfouten en reconnect | `false` betekent zowel EAGAIN als deviceverlies; stale `down` kan blijven hangen. | Getypeerd readresultaat; EINTR; EOF/protocolfout; bounded reconnect; gesture-reset. |
| TCH-10 | Eventqueue | Meerdere taps in één main-loopframe kunnen worden samengevoegd of overschreven. | Begrensde semantische queue; volgorde behouden; overflow zichtbaar gelogd; bursttest. |
| NET-01 | Blokkerende Wi-Fi-hotpath | `popen/system`, scan en DHCP kunnen de main loop blokkeren. | Non-blocking worker/state machine; gemeten main-loopstall binnen afgesproken grens. |
| WEB-01 | Webbeheer zonder duidelijke auth | Persistente server op alle interfaces biedt wijzigende routes; pairing/auth ontbreekt of is onbewezen. | Expliciet threat model; token/pairing of aantoonbaar veilige interfacebinding; muterende routes beschermd; tests. |
| WEB-02 | Niet-atomische WebSocket-upload | Bestaand boek wordt verwijderd vóór volledige nieuwe upload. | Schrijven naar tijdelijk bestand, flush/fsync/validatie, atomische replace; oude file blijft bij iedere fout. |

## P1 — dagelijkse betrouwbaarheid en Kobo-optimalisatie

| ID | Onderwerp | Probleem | Minimale acceptatie |
|---|---|---|---|
| SYS-01 | Refreshqualification-hotloop | Model/kernel/manifest/marker worden vrijwel iedere loop opnieuw gelezen. | Cache per procesrun; alleen hercontrole bij relevante wijziging. |
| SYS-02 | Batterijpolling | power_supply-sysfs wordt veel vaker gelezen dan nodig. | Gecachte snapshot en redelijke pollintervallen; direct refresh na USB/resume. |
| NET-02 | Verbindingswaarheid | Achtergebleven IP-adres kan als verbonden gelden zonder echte WLAN-associatie. | Status combineert link/association/DHCP; disconnect wordt tijdig zichtbaar. |
| NET-03 | Wi-Fi-powersave | Persistente webserver zet powersave blijvend uit. | Idle powersave standaard aan; tijdelijk uit tijdens actieve transfer; restore na idle/stop/resume. |
| WEB-03 | Fontupload-writecontrole | Korte of mislukte writes zijn niet overal hard afgehandeld. | Iedere write gecontroleerd; `.part`-bestand; magic/size-validatie; atomische install. |
| DSP-01 | FBInk dirty region | Centrale dirty rectangle wordt niet volledig als backendvenster gebruikt; dubbele policy. | Backend voert exact schedulerbesluit uit; eigen partialbudget verwijderd. |
| DSP-02 | DRM runtimefailover | Runtime DRM-fout heeft geen gecontroleerde reopen/FBInk-failover. | Bounded retry, reopen, recovery-full en daarna FBInk-failover met telemetry. |
| DSP-03 | Dubbele wake-refresh | Resume kan render + extra full-present doen. | Eén zichtbare wake-render met gewenste full-refresh; hardwaremeting bevestigt geen regressie. |
| PWR-01 | Touchstate rond suspend | Gesture, capture en evdevstate kunnen over suspend blijven hangen. | Pre-suspend cancel/all-up; post-resume resync/reopen; eerste frame zichtbaar vóór input. |
| PWR-02 | Suspend/wake-soak | Dagelijkse powerflow is onvoldoende bewezen. | Tien snelle ontwikkelcycli en uiteindelijke 100-cyclusgate, positie/frontlight/Wi-Fi/input intact. |
| CAL-01 | Calibratieclaim | Tool registreert afwijking, maar runtime gebruikt geen afgeleide kalibratiematrix. | Echte persisted calibratie óf tool/UX eerlijk hernoemd naar validation; test en docs. |

## P2 — CI, releasekwaliteit en onderhoudbaarheid

| ID | Onderwerp | Probleem | Minimale acceptatie |
|---|---|---|---|
| CI-01 | Kobo-tests niet verplicht | Normale CI draait vooral PlatformIO/ESP32 en kan Kobo-regressies missen. | Host-Kobo CMake/CTest verplicht op PR; gezaghebbende ARM-build volgens afgesproken cadence. |
| CI-02 | Touch-E2E ontbreekt | Gesture-, registry- en activitytests zijn los en missen integratiefouten. | Raw evdev → parser → gesture → registry → inputmanager → testactivity E2E. |
| REL-01 | Versie- en documentdrift | Beta 1/Beta 3, X3/X4 en Kobo-instructies lopen door elkaar. | Producttarget, issueformulier, README, changelog, package/buildversie consistent. |
| REL-02 | Release-evidence | Oud bewijs komt van meerdere binaries en is niet één RC-regressie. | Eén RC-SHA, buildmanifest, testmatrix, hardware-evidence en secretscan. |
| MAINT-01 | Shellprocessen in compatlaag | Veel `system()`/`popen()` maakt fouten, timing en quoting lastig. | Hotpaths verwijderd; resterende calls afgebakend, gecontroleerd en gelogd; later netlink/wpa-control roadmap. |

## Afsluitdiscipline

Een issue mag alleen naar `PASS` wanneer:

1. de oorzaak expliciet is beschreven;
2. de reparatie klein genoeg is om te reviewen;
3. regressietests bestaan;
4. alle relevante automatische tests groen zijn;
5. hardwarebewijs bestaat wanneer de acceptatie hardwaregedrag vereist;
6. commit-SHA, binary-SHA en evidencepad in het uitvoerlog staan.
