# Evdev-hardening en inputtestplan

## Doel

De inputlaag moet correct blijven wanneer de main loop tijdelijk achterloopt, de kernel events verliest, een device reset, een read wordt onderbroken of meerdere gestures snel achter elkaar binnenkomen.

## Getypeerd readresultaat

Vervang een ambigu `bool readFrame(...)` door een resultaat met minimaal:

```cpp
enum class TouchReadResult {
  FrameReady,
  WouldBlock,
  Interrupted,
  SynDropped,
  DeviceLost,
  ProtocolError,
};
```

Gedrag:

- `WouldBlock`: normaal einde van de huidige drainronde;
- `Interrupted`: opnieuw proberen zonder stateverlies;
- `SynDropped`: actieve gesture annuleren en resyncprocedure starten;
- `DeviceLost`: descriptor sluiten, state resetten, reconnect plannen;
- `ProtocolError`: log met rate limit, state resetten, reconnect of fail-safe.

## Timestamps

### Vereist

- gebruik de timestamp uit het evdev-record;
- probeer bij openen `EVIOCSCLOCKID` op `CLOCK_MONOTONIC`;
- documenteer fallback voor kernels/drivers die dit niet accepteren;
- detecteer onmogelijke terugloop of een grote sprong;
- gebruik readtijd alleen als expliciete fallback.

### Test

Voer dezelfde down/move/uprecords direct en met kunstmatige readvertraging uit. De berekende holdduur en gestureclassificatie moeten identiek zijn.

## `SYN_DROPPED`

State machine:

```text
NORMAL
  └─ SYN_DROPPED → DISCARDING
DISCARDING
  ├─ events negeren
  └─ volgende SYN_REPORT → RESYNC
RESYNC
  ├─ actuele key/ABS/MT-state opvragen
  ├─ gesture en capture geannuleerd houden
  └─ nieuwe clean down vereist → NORMAL
```

Geen actie uitvoeren op basis van deels verloren data.

## Deviceverlies en reconnect

- Sluit descriptor bij EOF, `ENODEV`, blijvende `EIO` of ongeldige recordlengte.
- Reset `down`, raw coordinates, gesture, capture en queued synthetic hold.
- Herontdek het inputdevice met bounded back-off, bijvoorbeeld 100 ms → 500 ms → 2 s → 5 s.
- Controleer capabilities opnieuw.
- Gebruik een vaste bekende devicepath wanneer de N437-hardwareprobe die betrouwbaar vastlegt; discovery blijft fallback.

## Multitouch/protocolcapaciteiten

De N437 gebruikt naar verwachting één direct zForce-contact. Controleer toch:

- `INPUT_PROP_DIRECT`;
- `BTN_TOUCH`;
- `ABS_X/Y` versus `ABS_MT_POSITION_X/Y`;
- aanwezigheid van `ABS_MT_SLOT` en `ABS_MT_TRACKING_ID`;
- maximum aantal slots.

Bij type-B-slots mag nooit een X-coordinate van slot A met Y van slot B worden gecombineerd. Voor een single-touchproduct kan een tweede contact expliciet worden genegeerd of de huidige gesture annuleren, mits dit deterministisch is getest.

## Unit tests

### Transform

- portrait;
- inverted;
- beide landscapeoriëntaties;
- clamp buiten raw range;
- N437 swap/invert;
- custom calibration wanneer geïmplementeerd.

### Gesture

- gewone tap in iedere readerzone;
- bottom frame links/rechts;
- long-press exact onder/op/boven threshold;
- tapslopgrenzen 23/24/25;
- swipegrenzen 71/72/73;
- diagonale beweging;
- swipe startend binnen een target;
- frame-/activitywissel tussen down en up;
- down zonder up gevolgd door deviceverlies;
- `SYN_DROPPED` tijdens hold.

### Queue

- twee taps in één drainronde;
- tap + swipe + tap;
- long-press gevolgd door release;
- queue precies vol;
- één event boven capaciteit;
- activitytransition na eerste event stopt verdere oude-activityevents.

## E2E-test

Bouw een testpipeline met echte 16-byte N437-evdevrecords:

```text
raw records
→ KoboEvdevTouch/parser
→ gesture recognizer
→ committed TouchUiSnapshot
→ pointer capture
→ semantic event queue
→ MappedInputManager/adapter
→ TestActivity
```

Verplichte scenario's:

1. één tap opent exact target 2;
2. swipe begint op target 2 maar activeert geen target;
3. long-press opent action menu en geen normale openactie;
4. render commit tussen down/up annuleert stale target;
5. `SYN_DROPPED` produceert geen action;
6. twee snelle taps behouden volgorde;
7. deviceverlies laat geen held button of touch achter.

## Hardwaretest

Op de N437:

- 100 taps verspreid over schermranden en centrum;
- 50 swipes die op boekkaarten beginnen;
- 30 long-presses op kaarten en lijstitems;
- 20 snelle dubbele taps;
- orientationwissels;
- modal open/dismiss;
- touch vlak vóór en na suspend.

Leg release-ID, binaryhash, relevante logs en een kort resultaatmanifest vast.
