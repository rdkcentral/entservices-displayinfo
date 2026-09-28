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
- The DeviceSettings backend caches the frame rate once, asynchronously, by spawning a
  detached thread (`CacheInitialFrameRateAsync()`) from the `DisplayInfoImplementation`
  constructor, retrying once after a fixed delay if the HAL isn't ready yet.
- On every `OnResolutionPostChange` callback, the DeviceSettings backend re-queries the
  frame rate via the existing `FrameRate()` getter, compares it against the cached
  value, and updates the cache.
- If the queried frame rate differs from the cached value, the backend emits
  `Updated(FRAMERATE_CHANGE)` to all registered `IConnectionProperties::INotification`
  observers **before** emitting `Updated(POST_RESOLUTION_CHANGE)`.
- No new interface method or Linux/RPI stub overrides are required — caching is an
  internal DeviceSettings-backend implementation detail, not part of
  `Exchange::IConnectionProperties`.

## Capabilities

### New Capabilities

_None — this change extends an existing capability._

### Modified Capabilities

- `displayinfo`: The `Updated` event gains a new `FRAMERATE_CHANGE` source. The
  DeviceSettings backend caches frame rate asynchronously at construction and compares
  it on every resolution-change callback.

## Impact

- **JSON-RPC surface:** The `updated` event's `Source` enum gains `FRAMERATE_CHANGE`.
  No breaking changes — additive enum value.
- **Interface:** No changes to `Exchange::IConnectionProperties` — frame-rate caching
  is an internal DeviceSettings-backend detail.
- **Code:**
  - `plugin/DeviceSettings/PlatformImplementation.cpp` — new `CacheInitialFrameRateAsync()`
    (spawned from the constructor) and `IsFrameRateChanged()` methods;
    `OnResolutionPostChange()` now emits `FRAMERATE_CHANGE` before `POST_RESOLUTION_CHANGE`
    when the rate changed.
- **Specs:** Updated `openspec/specs/displayinfo.spec.md` (`updated` event table,
  Covered Code, Conformance Testing).
- **Tests:** Extended `ResolutionChange_NotificationTest` scenario asserting
  `FRAMERATE_CHANGE` fires only when the frame rate actually changed, seeding the
  baseline via an `OnResolutionPostChange` callback.
- **No breaking changes.** Existing clients that ignore unknown `Source` values are
  unaffected.
