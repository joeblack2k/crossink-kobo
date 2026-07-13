# N437 EPUB-rendercorpus

Gebruik uitsluitend kopieën van deze bestanden onder `/Books/TestCorpus/` op
de Kobo. Raak geen regulier gebruikersboek, leespositie of bestaande cache aan.
De SHA-256 hieronder is die van de bron op de ontwikkel-Mac op 2026-07-13.

| Rol | Bestand | SHA-256 | Afbeeldingen | N437-proef |
| --- | --- | --- | --- | --- |
| Referentiekaart/JPEG | `2_The_Wise_Man_39_s_Fear_-_Rothfuss_Patrick.epub` | `7b02a0d80f25e8d4dbf17012d054849e03ac5b378d824a5113ac22668a911960` | 217 JPEG | cover plus beide kaarten; cold en warm cache; vijfmaal voor/terug |
| PNG-mix | `Inkheart - Cornelia Funke.epub` | `a059b3d4c664ecb900792b1555c3802d4d584fcd354d461dd5cc1f7cfb8bd22b` | 40 JPG, 3 PNG | alle PNG-pagina's en aangrenzende tekst |
| Grote gemengde media | `Heroes_ Volume II of Mythos - Stephen Fry.epub` | `5590a2084e3604142cec8f705dba3a0a6c894c43c62c550a1bdf80ed5e3e7634` | 38 JPG, 1 JPEG, 6 PNG | eerste, grootste en laatste afbeelding |
| Eenvoudige JPEG-controle | `5_-_The_Lady_of_the_Lake_-_Andrzej_Sapkowski.epub` | `253d8da9af8e24d636e11ae197f1a1978a63b5986575746b7595baed5215a08c` | 4 JPG | elke afbeelding, inclusief image→text en terug |
| Groot JPG-corpus | `I Am the Messenger - Markus Zusak.epub` | `e774c54eec8be1397f966cc9f612c731c53550f384342d5fdc5a64773e58fc51` | 67 JPG | drie verspreide illustraties |
| Beschadigde invoer | `Mythos_ A Retelling of the Myths of Ancien - Stephen Fry.epub` | `7b0449f8aea5c76f6f13f8d966a7e256029a26334d2f37c00d14338506b68237` | n.v.t. | `unzip -t` faalt op `images/00009.jpeg`; alleen lokale foutafhandeling, geen crash |

Er is in de huidige map `books` geen EPUB met SVG gevonden en geen bestand met
de naam *Inkspell*. Dat zijn geen ontbrekende implementatietaken: voeg pas een
SVG-test toe wanneer een concrete, legaal beschikbare bron aan deze corpusmap
is toegevoegd en hash die dan eerst.

## Acceptatieopname per testboek

1. Wis uitsluitend de `*.pxc`-cache behorend bij dit testboek.
2. Open de vastgelegde pagina en bewaar een interne framebuffercapture.
3. Heropen dezelfde pagina zonder cachewis en bewaar een tweede capture.
4. Vergelijk cold/warm, ga vijfmaal Next/Previous over de beeldovergang en
   noteer cache-hit, rendererduur, refreshprofiel, crashcounter en watchdog.
5. Herstart Photo Booth als de camera-preview stilstaat en bewaar een fysieke
   foto naast de interne capture.

## N437 afbeeldingsinvarianten

Elke `ImageBlock` gebruikt uitsluitend de logische readerviewport van
`GfxRenderer`, nooit de fysieke 1448×1072 DRM-coördinaten. De renderer dwingt
de volgende regels af:

1. De doelrect heeft strikt positieve breedte en hoogte; een rect die volledig
   buiten de logische viewport ligt wordt niet gedecodeerd.
2. Een volledig zichtbare rect mag alleen een PXC1-cache publiceren wanneer de
   cacheexacte doelmaat overeenkomt. Een gedeeltelijk zichtbare rect wordt
   door beide decoders gecontroleerd naar de logische viewport geclipt en
   publiceert geen volledige cache als dat onjuist zou zijn.
3. Decoder en cachehit loggen bronmaat, doelrect en `GfxRenderer`-orientation;
   hun pixels gaan op Kobo door dezelfde bounds-gecontroleerde
   `DirectPixelWriter`/`GfxRenderer`-route.
4. Een ontbrekende, lege of mislukte bron wist alleen de zichtbare doelrect en
   tekent daar een witte lokale placeholder; er blijft geen vorige beelddata
   achter.

De N437-proeven bevestigen onder meer JPEG `(292,117 487×664)`, PNG
`(478,213 115×186)` en een grote PNG `(8,57 1056×801)` binnen de portrait
viewport 1056×1251. De fysieke portrait-, landscape- en inverted-opnamen van
de Wise-Man-kaart tonen dezelfde logische layout zonder fysieke paneeloffset.
