# Kobo Glo HD N437 hardwarebasis

Dit bestand legt de bekende hardwarefeiten en de bring-upgrenzen vast. Waarden
komen uit de bewezen N437 `ntx_hwcfg` op de referentie-image; aannames zijn
expliciet als zodanig gemarkeerd.

## Bevestigde NTX-configuratie

| Onderdeel | Waarde |
|---|---|
| PCB | `E60Q90`, revisie `0x10`, level A |
| SoC / RAM | i.MX6SL, 512 MB LPDDR2 |
| Interne opslag | microSD, ext4, NTX-partitietype 3 |
| Display | 6-inch bottom EPD, 1448×1072, 8-bit bus |
| EPD-controller/PMIC | i.MX6SL EPDC + TPS65185 |
| Touch | Neonode zForce v3, IR |
| Frontlight | tabel 8+, SY7201 LED-driver, MSP430 PWM |
| Systeem-PMIC / RTC | RC5T619 |
| Batterijprofiel | 1500 mAh |
| Wi-Fi | CyberTan WC121A2 |
| Hall-sensor | TLE4913 |
| Fysieke toetsen | geen toetsenmatrix; powerknop blijft een aparte wakebron |

## Moderne kernelbasis

Pin `akemnade/linux` commit `ccddaba42e0abafbce84cc2243cbe0c98d400944`
van branch `kobo/drm-merged-6.19`. Controle van InkBox' exacte oude
N437-boardbron bevestigt voor PCB 46 (`E60Q9X`) dezelfde relevante bedrading
als de Tolino Shine 2 HD: power `GPIO5_IO08`, zForce IRQ/reset
`GPIO5_IO06/GPIO5_IO09`, cover `GPIO5_IO12`, Wi-Fi power/reset
`GPIO4_IO29/GPIO5_IO00`, frontlight enable/boost `GPIO2_IO10/GPIO1_IO29` en
de TPS65185-signalen op GPIO2. De CrossInk-DTS erft deze bewezen nodes en
verwijdert uitsluitend de twee fysieke Tolino-front/homekeys die de Glo HD
niet heeft.

## Bestaande bootketen

De bewezen N437 U-Boot leest een legacy `uImage` vanaf sector `81920`
(40 MiB), detecteert de kernelgrootte uit de header en geeft ATAGs door. Bouw
daarom de moderne kernel met `CONFIG_ARM_APPENDED_DTB` en
`CONFIG_ARM_ATAG_DTB_COMPAT`, voeg de N437-DTB aan `zImage` toe en maak daarvan
een ARM/Linux `uImage` met load- en entry-adres `0x10008000`. Dit adres is
niet afgeleid uit de normale i.MX6SL-RAMbasis maar rechtstreeks bevestigd in
de legacy-uImage-header op sector 81920 van de referentiekaart (bytes
`10 00 80 00` voor zowel load als entry); gebruik daarom nooit een gegokt
`0x80008000`-adres.

Forceer in de moderne kernel `root=/dev/mmcblk0p3 rootfstype=ext4 rootwait rw`.
De bewezen kaart heeft p1=`boot`, p2=`recoveryfs`, p3=`rootfs` en p4=`user`;
dus p2 is niet de normale runtime-root. Dit voorkomt dat de door de oude
NTX-hardwareconfig toegevoegde p1-rootkeuze de nieuwe indeling overschrijft.
Behoud bij imagegeneratie de bewezen raw
bootloader, serienummer/MAC, `ntx_hwcfg` en waveform uit de gebruikersimage;
genereer of publiceer die blobs niet als nieuwe projectdata.

De NTX-waveformheader staat op sector `14335` en heeft magic
`ff f5 af ff`; de payload begint op sector `14336`. In de gecontroleerde
N437-referentie meldt de header 6.575.584 bytes. De payload heeft SHA-256
`a158cba8276dc5ed5a146f7465285db1741612a5066497d10269f526a597de67`,
14 temperatuurgebieden en een 5-bit LUT. `extract-n437-waveform.py` weigert
lege, structureel ongeldige of onverwacht gewijzigde data en injecteert hem
lokaal als `/lib/firmware/imx/epdc/epdc.fw`; de blob staat niet in Git.

De moderne EPDC-driver is DRM/KMS en biedt niet de historische
`MXCFB_SEND_UPDATE`-ioctl waarop FBInk-refreshes steunen. Gebruik op de moderne
kernel daarom de eigen libdrm dumb-buffer/dirtyfb-backend. Houd FBInk alleen
als gedetecteerde fallback voor een legacy framebuffer. Een kleine private
DRM-ioctl selecteert gecontroleerd DU, AUTO-partial of GC16-full.

## Referentie-indeling en imagebeleid

De referentie-image heeft een MBR met vier Linux-partities: p1=`boot` start op
sector 30720 (24 MiB), p2=`recoveryfs` op 104448 (500 MiB), p3=`rootfs` op
1128448 (128 MiB) en p4=`user` op 1390592 (ongeveer 3 GiB). De raw-kerneloffset
ligt vóór p2, op sector 81920. Gebruik die gemeten offsets alleen als
compatibiliteitsgrens voor de bootloader. Bouw de CrossInk-image zelf uit een
eigen boot-partitie, eigen recovery op p2, eigen hoofdrootfs op p3 en eigen
data-partitie; kopieer geen InkBox-rootfs of GUI.
Alleen de niet-publiceerbare, apparaatgebonden bootstrook en hardwareblobs
mogen in een lokale image-builder uit een expliciet aangewezen
referentiekaart worden behouden.

De referentie bevat ook een oude raw kernel op sector `2048`; de bewezen N437
`load_ntxkernel`-route gebruikt sector `81920`. De image-builder wist daarom
alleen het bekende oude slot `2048..14331`, vóór de waveformheader, zodat geen
vreemde kernel achterblijft en de apparaatdata intact blijft.

## Prestatiegrens

Breng Beta 1 eerst op de standaardklok stabiel. Optimaliseer daarna EPDC-
waveforms, updatevensters en refreshcoalescing. Voeg alleen een hogere
CPU-frequentie toe als een aparte opt-in profieltest thermiek, suspendverbruik,
1000 paginawissels en tien cold boots zonder regressie doorstaat. De final
image start altijd op de bewezen standaardklok.
