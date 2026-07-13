# Kobo-bibliotheek — visuele taal

Bron: de door de gebruiker aangeleverde bibliotheekmock-up. Dit document is de
bindende visuele specificatie voor Kobo-specifieke schermen. Het vervangt geen
functionaliteit: ieder zichtbaar element moet een werkende, directe
touchhandeling hebben of niet worden getoond.

## Karakter

- **Tactiel minimalisme.** De UI voelt als een klein fysiek instrument: harde
  randen, dunne lijnen en subtiele 1-bit dithering geven knoppen en gleuven
  diepte zonder een Pearl-scherm met grijze vlakken of extra refreshwerk te
  belasten.
- **Rustig, boekgericht, monochroom.** Gebruik wit als ondergrond, zwart voor
  primaire informatie en slechts lichte dither voor secundaire selectie of
  voortgang. Geen grijze vlakken achter volledige kaarten en geen decoratieve
  kleurafhankelijkheid.
- **Sterke, compacte hiërarchie.** Statusbalk → appkop → tabnavigatie →
  covers → metadata → paginering. Iedere laag krijgt een vaste ruimte; tekst
  mag nooit over de volgende laag heen tekenen.
- **Omslagen zijn de primaire scan-eenheid.** Titel, auteur en leesstatus
  horen direct onder de omslag. Niet in een afzonderlijk detailscherm als de
  gebruiker slechts wil kiezen.
- **Directe vingerbediening.** Een kaart bevat zowel omslag als metadata in
  dezelfde hitbox. Tabs en vorige/volgende zijn afzonderlijke, zichtbare
  hitboxes; pijltjes of X4-buttonhints zijn nooit vereist.

## Bibliotheekskelet

1. **Statusbalk** — bestaand Kobo-statusgebied met Wi-Fi, gecentreerde klok en
   batterij rechts. Hoogte komt uitsluitend uit `ThemeMetrics`.
2. **Appkop** — titel `LIBRARY`/gelokaliseerde bibliotheeknaam, met links een
   menuactie en rechts alleen acties waarvoor een backend bestaat (zoeken,
   verversen). Geen lege icoonplaatsaanduidingen.
3. **Tabs** — `RECENT`, `BOOKS` en `OPDS` in hoofdletters. De actieve tab
   heeft één zwarte underline; de overige tabs zijn ongevuld. Series en
   collecties verschijnen pas wanneer ze een werkende datalaag hebben.
4. **Responsief omslagraster** — drie kolommen in portrait zolang de kaarten
   minstens circa 9 mm breed zijn. Op alle Kobo-schalen moeten omslag,
   titel, auteur en status binnen één kaart blijven. Een pagina toont alleen
   volledige rijen; op N437 is dat standaard 3 × 2 kaarten.
5. **Kaartmetadata** — één vette, afgekorte titelregel, auteur regulier,
   daarna `Unread`, `Read` of een compacte voortgangsbalk zonder los
   percentage. Drie vaste baselines voorkomen overlap met buurkaart of
   paginering.
6. **Paginering** — onderaan een ruime `Previous`-knop, midden
   `Page n of m`, en `Next`. Knoppen zijn disabled aan de rand of wrappen
   alleen wanneer dat door de actuele activity expliciet is toegestaan.

## Interactiecontract

- **Hamburger** — opent een modal, links uitgeschoven navigatiepaneel met
  Home, lokale bibliotheek, netwerkbibliotheek, collecties, instellingen en
  account alléén wanneer die bestemming bestaat. De overlay gebruikt een
  beperkte region-refresh; sluiten via Back of tik buiten het paneel.
- **Zoeken** — verandert de appkop in een lokaal tekstveld met het bestaande
  e-inktoetsenbord. Zoek gedebounced in titel, auteur en serie van de lokale
  metadata. Netwerkzoekresultaten verschijnen alleen als er een concrete,
  geconfigureerde catalogus-API is; lokale zoekactie blijft altijd bruikbaar.
- **Sync-status** — toon pas een syncicoon als de gekozen syncprovider een
  werkende status en handmatige actie heeft. Online, offline en bezig hebben
  ieder een duidelijke monochrome staat; een tik start uitsluitend een echte
  synchronisatie, nooit een cosmetische animatie.
- **Sorteren/filteren** — een tactiele balk onder de tabs biedt Laatst gelezen,
  Toegevoegd en A–Z plus Ongelezen/Voltooid. Keuzelijsten zijn volledige,
  direct-tikbare overlays; de gekozen instelling is persistent.

## Maten en e-inkregels

- Bereken alle geometrie uit `renderer.getScreenWidth/Height()` en de geschaalde
  `ThemeMetrics`; geen vaste X4-coördinaten.
- Gebruik 3:5-omslagverhouding, gecentreerd binnen de kolom, met metadata op
  de volledige kolombreedte en consistente gutters.
- Registreer paintrect en hitrect vanuit dezelfde `LibraryGridLayout`.
- Een volledige activiteit gebruikt één duidelijke full refresh bij de
  overgang; selectie, tab-underline en voortgang gebruiken beperkte updates
  waar de bestaande renderer dat veilig ondersteunt.
- Knoppen gebruiken een zwarte buitenrand, witte kern en hoogstens één
  dither-inset voor een ingedrukte/uitgeschakelde staat. Vermijd zachte
  schaduwen, animaties en grote grijze achtergronden.
- De 1-bit Kobo-fallback behoudt contrast: dither is uitsluitend secundair en
  tekst is altijd effen zwart of wit.

## Implementatievolgorde

1. Herbruikbare `LibraryGridLayout`: header/tab/content/footer-geometrie,
   kaartrechthoeken, paginering en touchregistratie.
2. `RecentBooksGridActivity`: de eerste echte tab, met omslagen, metadata,
   voortgang, directe kaarttouch en pagecontrols.
3. `FileBrowserActivity`: `BOOKS` gebruikt dezelfde chrome maar behoudt de
   bestaande, echte filesystemacties.
4. `OpdsBookBrowserActivity`: `OPDS` gebruikt dezelfde chrome en behoudt
   zoeken, catalogusnavigatie en download. De eerste directe lijstport is al
   aanwezig; daarna volgt de gridvariant alleen wanneer covermetadata uit de
   feed beschikbaar is.
5. Voeg menu/search/verversiconen pas toe met echte handlers en N437-bewijs.
6. Voeg na de bestaande library-baseline toe: navigatie-overlay, lokale zoek,
   sort/filter en vervolgens providergebonden syncstatus. Gebruik geen CSS of
   browsercanvas: dit is een native renderer met region-refreshes.

## Acceptatie

- Geen overlap, geclipte metadata, piepkleine kaarten of X4-buttonhints.
- Eén tap opent een boek, een tab of een pagina-actie; Back sluit veilig.
- Het raster is bruikbaar op 100/150/200/250% en in portrait; landscape volgt
  na die portrait-baseline.
- N437-screenshot én camerafoto tonen dezelfde leesbare structuur.

## Geïmplementeerde eerste slice

`RecentBooksGridActivity` gebruikt op de echte N437 een 3 × 2 raster met
echte EPUB-omslagen, directe kaarttouch, functionele `Browse Files`- en
`OPDS Browser`-tabs en veilige vorige/volgende-paginering. Bewijs:
[`visual/20260712T1030Z`](../artifacts/kobo/hardware/visual/20260712T1030Z/manifest.txt),
release `dev-library-covers-ca7982e6` (SHA
`ca7982e67b7925309e6f1096b7b842288e2c7accff12a1f78622254554d3b8b7`).
