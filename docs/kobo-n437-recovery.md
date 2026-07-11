# CrossInk-Kobo N437 recovery

Gebruik deze procedure uitsluitend voor de reserve-SD. De originele Kobo-SD
wordt niet gewijzigd en blijft de fysieke laatste terugvalroute.

## Applicatierecovery via USB-SSH

1. Verbind de ingeschakelde Kobo via USB. Wacht tot de Mac het ECM-interface
   heeft en probeer `ssh crossink-n437`.
2. Controleer de oorzaak met
   `cat /data/.crossink/recovery/active`, `logread` en
   `cat /data/.crossink/crash/last-signal.txt`.
3. Verwijder een geforceerde recovery alleen na diagnose met
   `rm -f /boot/flags/CROSSINK_FORCE_RECOVERY`.
4. Zet de watchdogteller terug met
   `printf '0\n' > /data/.crossink/watchdog/early-start-failures`.
5. Herstart met `reboot`. De supervisor valt opnieuw terug naar recovery als
   CrossInk binnen 60 seconden drie keer stopt.

Recovery houdt USB-networking en Dropbear actief. Het rootaccount heeft geen
wachtwoord; alleen de toegewijde publieke Ed25519-sleutel in de image is
toegestaan.

## Release terugrollen

Gebruik bij versioned applicatiereleases een bestaande release onder
`/opt/crossink/releases/`:

```sh
ln -sfn /opt/crossink/releases/<vorige-versie> /opt/crossink/current.new
mv -Tf /opt/crossink/current.new /opt/crossink/current
/etc/init.d/S60crossink restart
```

Controleer daarna minstens 60 seconden dat `/run/crossink-healthy` bestaat.

## Offline herstel van de reserve-SD

1. Schakel de Kobo volledig uit, verwijder uitsluitend de reserve-SD en plaats
   hem in de Mac.
2. Identificeer het device opnieuw met `diskutil list external physical` en
   `diskutil info /dev/diskN`. Vertrouw nooit op een eerder disknummer.
3. Controleer model, exacte bytegrootte en `Removable Media: Yes`.
4. Laat vóór iedere `dd` een nieuwe expliciete `USER GATE` bevestigen voor het
   volledige device `/dev/diskN`, nooit `diskNs1`.
5. Flash een eerder geverifieerde CrossInk-image of plaats de onaangeroerde
   originele Kobo-SD terug.

Na een offline flash: lees de volledige kaart terug, vergelijk de image-SHA of
de vastgelegde steekproefblokken en gebruik `diskutil eject /dev/diskN` voordat
de kaart wordt verwijderd.

## Niet doen

- Schrijf nooit kernel/rootfs-updates naar raw sectoren vanuit de Kobo-app.
- Maak de data-partitie nooit tegelijk schrijfbaar voor Kobo en USB-host.
- Kopieer geen private SSH-sleutel, wachtwoord of persoonlijke Wi-Fi-config in
  een release-image.
- Wis de NTX-hardwareconfig of de waveformsectoren niet.
