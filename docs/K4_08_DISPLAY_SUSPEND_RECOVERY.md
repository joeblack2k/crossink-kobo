# Display, suspend en recovery

## Eén refreshpolicy

De centrale refreshscheduler is de enige laag die bepaalt:

- skip bij ongewijzigd frame;
- dirty region;
- waveform;
- forced full;
- partialbudget;
- clean refresh;
- profieldegradatie en cooldown.

DRM en FBInk voeren dit besluit uit. Een backend mag niet aanvullend na een eigen hardcoded aantal partials een full refresh afdwingen.

## FBInk dirty region

De FBInk-route moet de door de scheduler berekende rectangle als echte refreshregion gebruiken wanneer de backend dit ondersteunt.

Acceptatie:

- backend ontvangt region;
- copy/refresh raakt alleen die region of documenteert aantoonbaar waarom full noodzakelijk is;
- unit-/mocktest bevestigt de exacte parameters;
- hardwaretest controleert artifacts en ghosting.

## DRM runtime recovery

Aanbevolen volgorde bij submitfout of deadline/quality failure:

1. scheduler degradeert profiel volgens bestaande policy;
2. recovery full op huidige backend;
3. bij I/O/backendfout: DRM één keer gecontroleerd heropenen;
4. opnieuw recovery full;
5. bij herhaalde fout: overschakelen naar FBInk;
6. huidige frame full presenteren;
7. telemetry en persistente diagnose vastleggen;
8. volgende cold boot probeert DRM opnieuw, tenzij expliciet gequarantined.

Geen eindeloze retryloop.

## Dirty-regioncoördinaten

Controleer dat iedere laag dezelfde fysieke/logische ruimte gebruikt:

- GfxRenderer logical orientation;
- packed native landscape buffer;
- scheduler panel width/height;
- DRM clip;
- FBInk rectangle;
- screenshot export.

Voeg tests toe voor randen, niet-byte-aligned logical rects en alle oriëntaties.

## Resume-render

Doel: één coherente eerste zichtbare wake-render.

Onderzoek de huidige route:

```text
select activity
→ requestUpdateAndWait()
→ mogelijk extra FULL displayBuffer()
```

Voorkeursoplossing:

```cpp
requestUpdateAndWait(RefreshIntent::FullAfterWake)
```

De renderthread tekent en presenteert één keer met full intent. Verwijder de tweede present alleen na hardwarebewijs.

## Touch rond suspend

### Vóór suspend

- geen nieuwe activityevents accepteren;
- semantische eventqueue leegmaken of veilig afronden;
- pointer capture annuleren;
- gesture resetten;
- evdevqueue drainen tot all-up met korte timeout;
- actieve touchsnapshot invalidation koppelen aan activitytransition;
- netwerkworkers pauzeren/cancelen;
- sleepframe volledig committen;
- storage sync volgens bestaande policy.

### Na resume

- inputdescriptor en capabilities controleren;
- bij twijfel evdev heropenen;
- parser, slots, `down` en timestamps resetten;
- frontlight herstellen;
- Wi-Fi lifecycle hervatten;
- activity herstellen;
- eerste frame zichtbaar presenteren;
- pas daarna touch weer toelaten.

## Suspendresultaat

Een return onder de ingestelde minimumduur is een onverwachte wake, maar mag niet automatisch een crashloop veroorzaken. Log:

```text
entered
wakeup_count_used
elapsed_ms
errno/detail
frontlight_before/after
wifi_state_before/after
touch_reopened
activity_restored
book_position_before/after
```

## Calibratie

Kies expliciet:

### Optie A — echte calibratie

- meerdere samples per target;
- median filtering;
- affine fit;
- residuele foutgrens;
- persisted calibration met model/driver/ranges;
- safe reset naar hardwaredefault;
- runtime gebruikt de matrix.

### Optie B — validationtool

- hernoem command, UI en docs van calibration naar validation;
- rapportage blijft raw/mapped/error;
- geen claim dat de tool runtimegeometrie aanpast.

Voer geen half geïmplementeerde calibratieclaim.

## Hardwaregates

### Ontwikkelgate

- 10 sleep/wakecycli;
- reader en Home afwisselen;
- positie, frontlight, touch, Wi-Fi en webserver controleren;
- geen stale tap of immediate wake door release.

### RC-gate

- 100 sleep/wakecycli;
- 1.000 gemengde page/displayupdates;
- temperatuur en fallbackcounter;
- drie cold boots na soak;
- recovery/USB-SSH beschikbaar;
- watchdogcounter 0.

Fast/Max-waveforms blijven buiten scope tenzij de bestaande kwalificatieprocedure volledig groen is.
