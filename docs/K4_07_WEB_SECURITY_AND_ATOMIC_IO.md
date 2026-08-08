# Webbeveiliging en atomische bestandsschrijfacties

## Threat model

De Kobo kan bereikbaar zijn via:

- USB-gadgetnetwerk;
- thuis-/kantoor-Wi-Fi;
- eventueel een door de Kobo gemaakte accesspointmodus.

De webserver biedt mogelijk muterende acties zoals upload, overwrite, delete, rename, move, instellingen, fonts, OPDS-servers en Wi-Fi-credentials. Een LAN mag daarom niet als automatisch vertrouwd worden behandeld.

## Vereiste exposure-inventarisatie

Leg vast:

```text
HTTP listen addresses=
WebSocket listen addresses=
UDP discovery interfaces=
USB interface/address=
Wi-Fi interface/address=
AP interface/address=
default enabled=
authentication=
CSRF protection=
```

Controleer dit op de werkelijk gebouwde Kobo-native server, niet alleen in gedeelde headers.

## Veilige productpolicy

Een van deze modellen is acceptabel, mits expliciet getest:

### Model A — USB-only default

- muterende webserver standaard uitsluitend op USB-adres;
- Wi-Fi-webbeheer moet de gebruiker tijdelijk op het apparaat inschakelen;
- duidelijke timeout en zichtbare status.

### Model B — pairing/token

- eerste lokale activatie toont een eenmalige code;
- succesvolle pairing levert een cryptografisch willekeurig token;
- token wordt root-only/persoonlijk opgeslagen;
- iedere muterende HTTP- en WebSocketactie vereist token;
- CSRF-bescherming voor browserflows;
- revoke/reset in Kobo-settings;
- geen token in URL, logs of screenshots.

Alleen vertrouwen op een obscure poort, device-IP of thuisnetwerk is onvoldoende.

## Routeclassificatie

### Read-only

- status;
- veilige file listing;
- downloads, mits protected paths blijven gelden;
- publieke statische assets.

### Muterend en altijd beschermd

- upload/overwrite;
- mkdir;
- rename/move/delete;
- settings POST;
- font upload/delete;
- OPDS add/update/delete/primary;
- Wi-Fi add/update/delete;
- WebSocket upload START/BIN;
- eventuele WebDAV mutaties.

## Atomische uploadtransactie

Voor iedere overwrite:

```text
1. normaliseer en valideer doelpad
2. open uniek tijdelijk bestand in dezelfde directory
3. stream data met volledige write-controle
4. flush en fsync file
5. valideer verwachte size/hash en indien relevant container/magic
6. fsync directory waar ondersteund
7. atomische rename/replace
8. pas daarna cache/user-state-migratie uitvoeren
9. bij iedere fout: temp verwijderen, origineel ongemoeid laten
```

Gebruik nooit eerst `remove(target)` gevolgd door direct schrijven naar `target`.

## WebSocket-specifiek

- START bevat een streng geparseerde size;
- maximum uploadgrootte en vrije-ruimtecheck;
- één ownerclient per upload;
- tweede START krijgt duidelijke fout;
- overflow, disconnect en timeout verwijderen alleen temp;
- DONE pas na succesvolle sync/replace;
- cache invalidation pas na commit;
- serverstop/suspend abort veilig;
- geen globale stale uploadstate na reconnect.

## Fontuploads

- valide family en filename;
- `.part` in dezelfde targetdirectory;
- magic vroeg controleren;
- iedere buffered write returnwaarde controleren;
- volledige bestandsgrootte en structurele header controleren;
- atomische install;
- registry pas na commit dirty markeren;
- bestaand geldig font behouden bij fout.

## CSRF en browsergedrag

Bij cookie-/browsergebaseerde auth:

- `SameSite=Strict` of gelijkwaardige lokale policy;
- muterende requests vereisen CSRF-token;
- controleer `Origin`/`Host` waar passend;
- geen state mutation via GET;
- duidelijke CORS-policy;
- geen wildcard origins voor muterende API.

## Negatieve tests

1. muterende request zonder token;
2. fout token;
3. verlopen/revoked token;
4. cross-origin POST;
5. path traversal;
6. protected hidden segment;
7. symlink/race wanneer POSIX storage dit toelaat;
8. nul-byte upload;
9. declared size kleiner/groter dan stream;
10. disconnect op 0%, 1%, 50%, 99% en na laatste byte vóór commit;
11. disk full/korte write;
12. restart of suspend tijdens upload;
13. overwrite van bestaand EPUB en font;
14. gelijktijdige tweede client.

## Acceptatiebewijs

Voor overwrite-tests:

```text
old_sha256=
new_expected_sha256=
result_sha256=
temp_files_after_test=
http_or_ws_result=
cache_state=
```

Bij iedere fout moet `result_sha256 == old_sha256` zijn.
