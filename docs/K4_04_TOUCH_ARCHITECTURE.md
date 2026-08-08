# Doelarchitectuur voor Kobo-touch

## Probleemstelling

De huidige port combineert drie modellen:

- fysieke evdev-touchframes;
- renderer-gepubliceerde hitboxes;
- legacy virtuele knoppen voor gedeelde activities.

Dat werkt in veel normale gevallen, maar mist een expliciete pointer-capture- en framecommitsemantiek. De Beta 4-pass moet deze grens hard maken zonder alle activities te herschrijven.

## Verplichte invarianten

1. **Een getoond scherm heeft precies één actieve hitboxsnapshot.**
2. **Een render bouwt hitboxes buiten de actieve snapshot en commit ze atomisch.**
3. **Een touch-down capturet maximaal één target uit de actieve snapshot.**
4. **Een touch-up activeert alleen de op touch-down gecapteerde target.**
5. **De activity-instance en framegeneration moeten nog geldig zijn.**
6. **Een swipe of geannuleerde gesture activeert nooit een target.**
7. **Een long-press is een eigen semantisch event.**
8. **Een semantisch event wordt maximaal één keer geconsumeerd.**
9. **Een activitytransition annuleert alle captures van de oude activity.**
10. **Queue-overflow is zichtbaar, begrensd en mag geen willekeurige actie produceren.**

## Aanbevolen registrypatroon

Een mogelijke interface:

```cpp
using TouchFrameId = std::uint64_t;
using ActivityInstanceId = std::uint64_t;

struct TouchUiSnapshot {
  TouchFrameId frameId{};
  ActivityInstanceId activityId{};
  std::vector<TouchRegion> regions;
};

class TouchUiRegistry {
 public:
  Builder beginFrame(ActivityInstanceId activityId);
  void commitFrame(Builder&& builder);
  std::shared_ptr<const TouchUiSnapshot> activeSnapshot() const;
};
```

Een vaste array mag behouden blijven om allocaties te vermijden. Belangrijk is de atomische swap, niet het containerstype.

### Geen actieve `clear()` tijdens consumptie

Een geconsumeerde target moet als inputevent worden verwijderd, maar de visuele snapshot blijft actief totdat een nieuwe render is gecommitteerd. Hiermee blijft touchdown/touchup en screenshotbewijs coherent.

## Pointer capture

```cpp
struct TouchCapture {
  bool active = false;
  TouchFrameId frameId{};
  ActivityInstanceId activityId{};
  TouchRegionId regionId{};
  TouchPoint downPoint{};
  std::uint64_t downTimestampUs{};
};
```

### Captureflow

1. bij eerste `down`: resolve target uit actieve snapshot;
2. sla target, frame, activity en downpoint op;
3. bij beweging boven tapslop: markeer tap als cancelled;
4. bij swipe-threshold: emit swipe en verwijder targetcapture;
5. bij long-press-threshold: emit één `LongPressTarget` en markeer als fired;
6. bij `up` binnen tapslop en zonder long-press: emit `TapTarget`;
7. bij activity-/frame-invalidatie: emit niets en clear capture.

## Semantische eventtypen

```cpp
enum class TouchEventKind {
  TapTarget,
  LongPressTarget,
  SwipeLeft,
  SwipeRight,
  SwipeUp,
  SwipeDown,
  Cancelled,
};

struct TouchEvent {
  TouchEventKind kind{};
  ActivityInstanceId activityId{};
  TouchFrameId frameId{};
  TouchRegion target{};
  TouchPoint down{};
  TouchPoint up{};
  std::uint64_t durationUs{};
};
```

`Cancelled` hoeft meestal niet naar activities, maar telt wel als user activity en is nuttig voor diagnostiek.

## Begrensde eventqueue

Aanbevolen eigenschappen:

- vaste capaciteit, bijvoorbeeld 32 events;
- FIFO-volgorde;
- geen silent overwrite;
- bij overflow: nieuwe event weigeren, capture resetten, foutcounter/log;
- geen heapallocatie in de evdev-hotpath;
- één activity-loop mag meerdere veilige events consumeren, maar stopt na een activitytransition.

## Legacy-adapter

Niet alle activities hoeven direct semantische touch te begrijpen. Voeg een adapter toe:

```text
TapTarget op NavigationItem
→ zet selection direct
→ injecteer compatibele Confirm-edge

LongPressTarget op NavigationItem
→ roep activity-specifieke long-pressroute aan
→ niet reduceren tot nulduur-Confirm
```

Migreer in deze volgorde:

1. RecentBooksGrid/Library cards;
2. Settings en generieke lijsten;
3. OptionSelection en dialogs;
4. Keyboard;
5. reader-menu en sliders;
6. text selection surface;
7. resterende activityfamilies.

## Gesturegrenzen

Houd de huidige waarden alleen wanneer hardwaremetingen ze ondersteunen. Test minimaal:

```text
tap slop: 23, 24, 25 px
middengebied: 48 en 71 px
swipe: 72 en 73 px
```

Een middengebied is een expliciete cancel, geen stil vergeten event. Overweeg later physical-mm-afgeleide grenzen, maar verander tijdens deze pass niet onnodig het complete UX-profiel.

## Touch en e-ink-latency

De gebruikersinteractie hoort tegen het laatst gecommitte logische frame te lopen, niet tegen de verwachte toekomstige render. Bij een actie:

- inputtarget onmiddellijk invalideren voor een tweede activatie;
- activitytransition starten;
- oude snapshot bewaren als visuele waarheid totdat nieuw frame commit;
- tijdens transition geen target van het oude activity-ID uitvoeren.

## Telemetry

Log alleen bij debug of afwijking:

```text
frame_id
activity_id
gesture_kind
down/up coordinates
duration_us
target_kind/target_id
cancel_reason
queue_depth
dropped_events
```

Geen logspam per normaal touchframe in releasebuilds.
