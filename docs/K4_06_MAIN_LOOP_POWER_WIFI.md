# Main loop, power, sysfs en Wi-Fi

## Hoofddoel

De Kobo-main loop moet input en UI blijven servicen zonder periodieke multi-seconde blokkades of onnodige sysfs-/process-spam. E-reader-batterijduur is een productvereiste, geen latere optimalisatie.

## Eén user-activitycontract

Introduceer één bron van waarheid:

```cpp
bool MappedInputManager::hadUserActivityThisFrame() const;
```

Dit telt minimaal:

- fysieke powerknop;
- touch-down;
- tap;
- swipe;
- long-press;
- geannuleerde gesture na betekenisvolle beweging;
- eventuele hardware-pagebuttons op andere targets.

De autosleep-timer gebruikt dit contract in plaats van alleen ruwe GPIO.

### Acceptatie

Een gebruiker die langer dan de ingestelde timeout uitsluitend via touch door menu's en boeken navigeert, mag niet tijdens actieve bediening in suspend gaan.

## Refreshqualification cachen

De N437-model-, kernel-, manifest- en qualificationmarker zijn stabiel tijdens één procesrun.

- lees en valideer qualification bij startup;
- sla resultaat en reden op;
- hercontroleer alleen bij profielwijziging of expliciete debug/reload;
- schrijf settings alleen wanneer de effectieve waarde werkelijk verandert;
- geen filesystemreads per main-loopiteration.

## Batterij- en USB-polling

Aanbevolen uitgangspunten:

- USB online: iedere 250–1000 ms, of via uevent wanneer eenvoudig en betrouwbaar;
- batterijpercentage/status: iedere 15–30 s;
- direct pollen na startup, resume en USB-statechange;
- gecachte `BatterySnapshot` beschikbaar voor header en device info;
- foutcounter en laatste succesvolle timestamp bewaren.

Geen open/read/close van meerdere sysfsfiles om de 10 ms.

## Wi-Fi state machine

### Verboden hotpathgedrag

- blokkerend `iw scan` in de main thread;
- synchroon `udhcpc` met secondenlange timeout in `status()`;
- `popen("wpa_cli ...")` bij iedere loop;
- herhaald `system()` zonder gecachte state of timeout;
- een oud IP-adres als enige bewijs van verbinding.

### Gewenste states

```text
Disabled
LinkDown
Scanning
Associating
AssociatedNoIp
DhcpRunning
Connected
Backoff
AccessPoint
Suspending
```

### Verbindingswaarheid

`Connected` vereist minimaal:

- interface up;
- echte association/completed state;
- geldig IPv4-adres;
- geen expliciete disconnect;
- recente state-updatetijd.

Een achtergebleven IP zonder association is niet connected.

### Uitvoering

Voorkeur:

1. wpa_supplicant control socket voor status/events;
2. netlink/route state voor link en adressen;
3. apart childproces/worker voor DHCP met timeout en completionpolling;
4. shellcalls alleen als afgebakende fallback buiten de hot loop.

Een pragmatische workerthread is acceptabel voor Beta 4 wanneer ownership, cancel, suspend en logging helder zijn.

## Scan en DHCP

- Scan start asynchroon.
- UI toont scan-in-progress en blijft touchresponsief.
- Cancel/Back beëindigt of negeert de completion veilig.
- DHCP mag de activity/main loop niet blokkeren.
- Een tweede connectrequest supersedeert de eerste deterministisch.
- Suspend annuleert of pauzeert netwerkwerk zonder data race.

## Wi-Fi-powersavebeleid

Doel:

- idle: powersave aan;
- actieve scan/association/download/upload: tijdelijk uit;
- na laatste netwerkactiviteit: powersave na korte graceperiode terug aan;
- alleen USB-webverkeer verandert wlan0 niet;
- stop/suspend/resume herstelt de bedoelde state.

Leg de vorige state vast; gebruik geen blind `off` bij iedere serverstart.

## Main-looptelemetry

Meet zonder release-logspam:

- max loopduur;
- p50/p95/p99 loopduur in debugmeting;
- tijd sinds laatste inputpoll;
- lengte inputqueue;
- Wi-Fi-workerstatus;
- laatste sysfspoll;
- actieve transfer;
- aantal stalls boven 50/100/250/1000 ms.

### Beta 4-doel

Stel na baseline een realistische harde grens vast. Richtwaarde:

- geen ongecontroleerde stall boven 100 ms in normale navigatie;
- geen scan/DHCP-stall van seconden in main/inputthread;
- touch-events worden tijdens netwerkwerk niet verkeerd geclassificeerd of samengevoegd.

## Batterij-A/B-test

Zelfde device, helderheid, boek/scherm, Wi-Fi en omgeving:

1. persistente server aan, idle powersave correct;
2. persistente server aan, actieve transferperioden;
3. server uit;
4. minimaal meerdere uren per run;
5. start/eindpercentage, uptime, wakeups, Wi-Fi-state en temperatuur loggen.

Trek geen harde batterijclaim uit één korte meting.
