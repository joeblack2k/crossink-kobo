# Prepared change manifest

## High-confidence code already written

### 1. Atomic touch publication

The render worker writes hitboxes to a staging buffer and commits the complete
buffer under one lock. Input continues to see the previous committed frame
while rendering is in progress. Activity transitions invalidate the outgoing
frame immediately. A target is rejected when its generation is no longer
active.

### 2. Semantic gesture routing

Touch-down captures the committed visual target. A tap activates only that
captured target. A swipe clears capture and routes navigation, so swiping over a
book card cannot open the book. Long-press is preserved as a semantic target
instead of a zero-duration Confirm press/release. The 25–71 pixel deadband now
returns `Cancelled` explicitly.

### 3. Evdev discontinuity and reconnect handling

The N437 input path now requests monotonic kernel timestamps, distinguishes
`WouldBlock`, interruption, device loss and protocol error, discards through
`SYN_DROPPED`, resynchronizes current coordinates/contact state, reconnects
with bounded back-off and clears held input across orientation and suspend.
A deterministic parser test is registered in Kobo CTest. The main loop handles
only one completed gesture per frame so queued taps cannot overwrite each other.

### 4. Main-loop and power hotpaths

Injected touch edges are included in general user activity. Refresh
qualification is checked only when the requested profile changes and only Fast
requires the marker reads. Battery/USB sysfs is polled at 1 Hz rather than every
10–50 ms. Wi-Fi shell status checks are throttled and a stale IP alone no longer
means `WL_CONNECTED`.

### 5. Suspend/wake presentation

The pending Sleep→Reader/Home transition is committed before synchronous
rendering. The first destination render requests a clean panel update and the
synchronous path clears its already-deferred request, avoiding a stale sleep
render followed by a duplicate full submission.

### 6. Display backend recovery

FBInk now receives the scheduler's transformed dirty rectangle instead of a
full-panel refresh and no longer owns a second partial/full budget. A real DRM
I/O failure whose same-backend recovery also fails triggers a guarded live
FBInk open and full recovery frame; deadline or thermal policy alone never
switches backend.

### 7. Atomic WebSocket replacement

The WebSocket route now follows the existing safe HTTP-upload pattern: write a
same-directory `.part`, verify writes, sync, then rename. Disconnect and errors
remove only staging data. On Kobo, this WebSocket path accepts only EPUBs and
forces `/Books`. Custom-font multipart upload now uses the same staging model,
checks every write, validates the completed CPFONT magic from disk and activates
only after sync; an existing font survives every abort or invalid upload.

### 8. Kobo CI

A native `KOBO_BUILD_DISPLAY=OFF` CMake/CTest job is included in required CI.

## Deliberately not presented as finished code

The following need broader design or hardware evidence and remain Luna work:

- raw-evdev-to-activity E2E harness and physical disconnect/reconnect soak;
- a separate semantic FIFO only if the one-complete-gesture-per-frame strategy fails measured burst tests;
- non-blocking DHCP and scan worker/control socket;
- web pairing/token policy and complete mutating-route authorization;
- failure-injection proof for prepared font replacement on POSIX and an ESP regression build;
- physical validation of the prepared DRM→FBInk failover and FBInk dirty windows;
- physical 100-cycle suspend/wake proof for the prepared touch reset and one-present wake path;
- real calibration persistence versus validation-tool rename;
- final ARM/Buildroot build and physical N437 regression.
