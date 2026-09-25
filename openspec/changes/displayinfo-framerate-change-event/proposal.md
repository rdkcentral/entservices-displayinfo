## Why

The `DisplayInfo` plugin's `Updated` JSON-RPC event only reports `PRE_RESOLUTION_CHANGE`,
`POST_RESOLUTION_CHANGE`, `HDMI_CHANGE`, and `HDCP_CHANGE`. Resolution changes on the
connected display frequently also change the active frame rate (e.g. a 4K60 → 4K24
mode switch), but clients have no event-driven way to learn this — they must poll
`DisplayInfo.framerate` after every `POST_RESOLUTION_CHANGE` and diff the value
themselves. This is wasteful and race-prone (the poll can race a further resolution
change).

## What Changes

- Add a new `Source` enum value `FRAMERATE_CHANGE` to the existing `Updated` JSON-RPC
  event (interface change in `entservices-apis` / `Exchange::IConnectionProperties::INotification::Source`).
- Add `Exchange::IConnectionProperties::InitializeFrameRate()` to cache the frame rate
  once, at plugin initialization time (before any resolution-change events can occur).
- On every `OnResolutionPostChange` callback, the DeviceSettings backend re-queries the
  frame rate via the existing `FrameRate()` getter, compares it against the cached
  value, and updates the cache.
- If the queried frame rate differs from the cached value, the backend emits
  `Updated(FRAMERATE_CHANGE)` to all registered `IConnectionProperties::INotification`
  observers **before** emitting `Updated(POST_RESOLUTION_CHANGE)`.
- `DisplayInfo::Initialize` calls `_connectionProperties->InitializeFrameRate()`
  immediately after acquiring the `IConnectionProperties` interface, so the cache is
  populated before any client can observe a stale/uninitialized value.
- Linux/DRM and BCM/RPI backends add stub overrides of `InitializeFrameRate()`
  (`ERROR_NOT_SUPPORTED` / `ERROR_UNAVAILABLE` respectively) — frame-rate change
  detection is DeviceSettings-only in this change.

## Capabilities

### New Capabilities

_None — this change extends an existing capability._

### Modified Capabilities

- `displayinfo`: The `Updated` event gains a new `FRAMERATE_CHANGE` source, and
  `IConnectionProperties` gains `InitializeFrameRate()`. The DeviceSettings backend
  caches frame rate at startup and compares it on every resolution-change callback.

## Impact

- **JSON-RPC surface:** The `updated` event's `Source` enum gains `FRAMERATE_CHANGE`.
  No breaking changes — additive enum value.
- **Interface:** `Exchange::IConnectionProperties` gains a new virtual method
  `InitializeFrameRate()` — interface change in `entservices-apis` (tracked via
  `feature/RDKEMW-24427_2`).
- **Code:**
  - `plugin/DisplayInfo.cpp` — calls `InitializeFrameRate()` during `Initialize()`.
  - `plugin/DeviceSettings/PlatformImplementation.cpp` — new `InitializeFrameRate()`
    and `IsFrameRateChanged()` methods; `OnResolutionPostChange()` now emits
    `FRAMERATE_CHANGE` before `POST_RESOLUTION_CHANGE` when the rate changed.
  - `plugin/Linux/PlatformImplementation.cpp`, `plugin/RPI/PlatformImplementation.cpp`
    — stub `InitializeFrameRate()` overrides.
- **Specs:** Updated `openspec/specs/displayinfo.spec.md` (`updated` event table,
  Covered Code, Conformance Testing).
- **Tests:** New L1 tests `InitializeFrameRate_Success`,
  `InitializeFrameRate_ExceptionHandling`, and an extended
  `ResolutionChange_NotificationTest` scenario asserting `FRAMERATE_CHANGE` fires only
  when the frame rate actually changes.
- **No breaking changes.** Existing clients that ignore unknown `Source` values are
  unaffected.
