# Non-goals en user gates

## Non-goals voor deze hardening-pass

Breid de scope niet uit naar grote nieuwe productfeatures tenzij een P0/P1-fix dit strikt vereist.

Niet als onderdeel van deze pass:

- nieuwe ebookformaten of volledige TXT/BMP/XTC-featurepariteit;
- nieuwe OPDS-features zoals uitgebreide zoek-/filterfunctionaliteit;
- KOReader Sync of Nearby Sync volledig opnieuw porten;
- WebDAV-featurepariteit toevoegen wanneer veilige webhardening zonder WebDAV kan worden afgerond;
- hoogwaardige grayscale-/coverpipeline opnieuw ontwerpen;
- nieuwe updater of complete final USB-mass-storageflow bouwen;
- overclock, MaxBeta-waveforms of ongeteste A2-profielen;
- volledige UI-redesign;
- andere Kobo-modellen ondersteunen;
- bestaande stabiele kernel/rootfs/recoverybasis vervangen;
- algemene CrossInk-upstreamrefactor die niet nodig is voor Kobo-hardening.

Deze punten mogen als afzonderlijke vervolgissues worden vastgelegd, maar mogen de Beta 4-hardening niet ontsporen.

## Veiligheidsregels

- Nooit de originele Kobo-SD overschrijven.
- Alleen de bewezen reserve-SD/developmentroute gebruiken.
- Nooit partitions, waveformsectoren of NTX-hardwareconfig wissen.
- Geen firmwareflash wanneer model/devicepath niet exact is bevestigd.
- Geen destructive test zonder herstelroute, USB-SSH en backup.
- Geen Fast/Max-profiel activeren zonder geldige device/kernel/binaryqualification.
- Geen auth uitschakelen om tests eenvoudiger te maken.
- Geen persoonlijke credentials in testfixtures of logs.
- Geen productiebranch direct pushen.

## User gates

Een user gate is alleen nodig voor een handeling die fysiek risico, langdurige device-onbeschikbaarheid of persoonlijke secrets vereist.

### GATE-HW-01 — eerste deployment na kerninputwijziging

Voorwaarden:

- hosttests groen;
- ARM-build groen;
- exact N437-model bevestigd;
- reserve-SD/recovery beschikbaar;
- huidige binary en statehash vastgelegd.

### GATE-PWR-01 — suspendsoak

Voorwaarden:

- korte 3–10 cycli handmatig groen;
- geen immediate wake of stale touch;
- logging en recovery werken;
- gebruiker accepteert dat het device tijdens de soak bezet is.

### GATE-DSP-01 — refresh-/waveformsoak

Voorwaarden:

- Safe blijft default;
- thermische en fallbacktelemetry actief;
- geen Max/A2 zonder expliciete kwalificatie;
- gebruiker kan het scherm fysiek op ghosting controleren.

### GATE-SEC-01 — pairingsecret of Wi-Fi-test

Voorwaarden:

- gebruik tijdelijke testcredentials;
- geen echte hoofdwachtwoorden;
- secret wordt na test gerevoked;
- logs worden gecontroleerd.

## Gedrag bij gate

De agent moet:

1. alle software-only werkzaamheden vóór de gate afronden;
2. exacte command, risico, verwachte duur van de fysieke handeling en rollback beschrijven;
3. niet alvast de risicovolle handeling uitvoeren;
4. de issue-status `BLOCKED_USER_GATE` zetten;
5. na toestemming verdergaan zonder een nieuwe brede analysefase.

Een ontbrekende user gate is geen reden om andere onafhankelijke issues niet af te ronden.
