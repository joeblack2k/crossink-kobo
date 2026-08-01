# CI, release en repositoryhygiëne

## PR-CI

Iedere pull request moet minimaal uitvoeren:

1. formatter/check;
2. static analysis;
3. bestaande gedeelde tests;
4. Kobo-host CMake-build met `BUILD_TESTING=ON` en waar nodig `KOBO_BUILD_DISPLAY=OFF`;
5. alle Kobo-CTest-tests;
6. touch-E2E-test;
7. target-aware dependency-audit voor relevante wijzigingen.

Gebruik officiële actionversies die binnen het repositorybeleid passen. Pin externe dependencies waar praktisch.

## ARM/Buildroot-CI

Een volledige Buildroot-build kan duur zijn. Vereist patroon:

- op `main` en releasebranches: gezaghebbende ARM/Buildroot-build;
- op PR: volledige build wanneer Kobo-platform-, Buildroot-, vendor- of gedeelde readercode verandert, of anders een betrouwbare gecachte/crossbuildcheck;
- op release: schone build zonder hergebruikte lokale output;
- compile commands en manifest bewaren.

## Testgevoeligheid controleren

Voeg niet alleen een groen job toe. Bevestig éénmalig dat de CI werkelijk faalt wanneer:

- een Kobo-unit test bewust wordt gebroken;
- een Kobo-source niet compileert;
- een verboden ESP-dependency actief wordt;
- een formatteringsfout wordt geïntroduceerd.

Herstel de tijdelijke sabotage vóór commit; noteer de verificatie in het uitvoerlog.

## Productdocumentatie opschonen

Controleer minimaal:

- README opent als Kobo-product, niet als X3/X4-firmware;
- installatie-instructies zijn veilig en Kobo-specifiek;
- ESP32/PlatformIO-instructies zijn duidelijk upstream/legacy of apart geplaatst;
- issueformulier bevat Kobo Glo HD N437, kernel, build-SHA, orientation, displaybackend, touchlog en suspendstatus;
- links wijzen naar deze repository waar passend;
- bestaande CrossInk-upstreamcredits en licenties blijven correct;
- unsupported functies worden niet als werkend gepresenteerd.

## Versiebron

Definieer één primaire versiebron of een gegenereerde synchronisatiecheck voor:

- C/C++ builddefine;
- Buildroot package version;
- changelog;
- artifact-/releasefilename;
- buildmanifest;
- UI Device Info.

CI moet falen bij versieafwijking.

## Beta 4-label

Gebruik pas `Beta 4` wanneer:

- alle P0-issues `PASS` zijn;
- verplichte P1-gates voor dagelijks gebruik groen zijn;
- PR-CI en ARM-build groen zijn;
- één RC-SHA de finale regressie heeft doorlopen;
- geen kritieke web-, touch-, suspend- of dataverliesbug openstaat.

Tot die tijd:

```text
1.4.0-kobo-beta3
```

Daarna eerst:

```text
1.4.0-kobo-beta4-rc1
```

en pas na RC-evidence:

```text
1.4.0-kobo-beta4
```

## Secrets en artifacts

Releasecheck moet minimaal zoeken naar:

- private keys;
- Wi-Fi SSID/password;
- auth tokens/pairing secrets;
- persoonlijke OPDS-credentials;
- lokale IP-/hostnamen waar privacy relevant is;
- volledige diskimages;
- cores/dumps met persoonlijke inhoud;
- tijdelijke uploadbestanden.

Een dedicated publieke SSH-key mag alleen volgens het bestaande recoverymodel aanwezig zijn; nooit de private key.

## Draft-PR

De draft-PR bevat:

- baseline-SHA;
- issue-ID's;
- architectuurwijzigingen;
- tests per bewijsniveau;
- hardware-evidence;
- bekende resterende risico's;
- expliciete releasebeslissing;
- geen opgeblazen claim dat alle CrossInk-featurepariteit is bereikt.
