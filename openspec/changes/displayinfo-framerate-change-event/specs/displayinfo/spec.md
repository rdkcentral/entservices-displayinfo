# DisplayInfo — Frame Rate Change Event — Change Delta Spec

## Overview

This spec captures the requirements added or modified in the
`displayinfo-framerate-change-event` change: a new `FRAMERATE_CHANGE` source for the
`Updated` JSON-RPC event, and the DeviceSettings backend logic that caches (asynchronously,
at construction time) and diffs the active frame rate on every resolution-change callback.

---

## Description

Prior to this change, `Exchange::IConnectionProperties::INotification::Source` only
defined `PRE_RESOLUTION_CHANGE`, `POST_RESOLUTION_CHANGE`, `HDMI_CHANGE`, and
`HDCP_CHANGE`. Clients had no event-driven way to learn that the active frame rate
changed as a side effect of a resolution change. This delta adds `FRAMERATE_CHANGE`,
an asynchronous baseline-cache seed (`CacheInitialFrameRateAsync()`, spawned from the
`DisplayInfoImplementation` constructor) at plugin startup, and the DeviceSettings
backend implementation that compares the current frame rate against the cache on
every `OnResolutionPostChange` callback.

---

## Requirements

### Requirement: Updated event supports a FRAMERATE_CHANGE source
The `Updated` JSON-RPC event's `Source` enum SHALL include `FRAMERATE_CHANGE`, fired
whenever the active frame rate of the primary video output changes.

#### Scenario: Frame rate changes on a resolution-change callback
- **WHEN** `OnResolutionPostChange` is invoked and the queried frame rate differs from
  the previously cached frame rate
- **THEN** the plugin SHALL emit `Updated(FRAMERATE_CHANGE)` to all registered
  `IConnectionProperties::INotification` observers before `Updated(POST_RESOLUTION_CHANGE)`

#### Scenario: Frame rate unchanged on a resolution-change callback
- **WHEN** `OnResolutionPostChange` is invoked and the queried frame rate matches the
  previously cached frame rate
- **THEN** the plugin SHALL NOT emit `Updated(FRAMERATE_CHANGE)`
- **THEN** the plugin SHALL still emit `Updated(POST_RESOLUTION_CHANGE)` as before

### Requirement: Frame rate is cached asynchronously at construction time
The DeviceSettings backend SHALL seed the frame-rate cache by spawning a detached
background thread (`CacheInitialFrameRateAsync()`) from the `DisplayInfoImplementation`
constructor, so the first `OnResolutionPostChange` callback has a best-effort baseline
to diff against without blocking plugin `Initialize()`.

#### Scenario: Constructor spawns the frame-rate cache thread
- **WHEN** `DisplayInfoImplementation` is constructed
- **THEN** it SHALL spawn a detached thread running `CacheInitialFrameRateAsync()`

#### Scenario: CacheInitialFrameRateAsync succeeds (DeviceSettings backend)
- **WHEN** the current resolution/frame rate is readable
- **THEN** `CacheInitialFrameRateAsync()` SHALL cache the mapped `FrameRateType` value
  and return without retrying

#### Scenario: CacheInitialFrameRateAsync retries on failure (DeviceSettings backend)
- **WHEN** a `device::Exception`, `std::exception`, or unknown exception is thrown
  while reading the resolution/frame rate on the first attempt
- **THEN** it SHALL wait a fixed delay and retry once more before giving up
- **THEN** the cached frame rate SHALL retain its last-known value if all attempts fail

### Requirement: Frame rate change detection reuses the existing FrameRate getter
The DeviceSettings backend SHALL detect frame-rate changes by invoking the same
private `FrameRate(FrameRateType&)` implementation used by the `DisplayInfo.framerate`
JSON-RPC property, guarded by a dedicated lock separate from the observer-list lock.

#### Scenario: Frame rate query failure during change detection
- **WHEN** `FrameRate()` throws or fails while `IsFrameRateChanged()` is querying the
  current rate
- **THEN** the resulting rate SHALL be treated as `FRAMERATE_UNKNOWN` for comparison
  purposes
- **THEN** the cache SHALL be updated to `FRAMERATE_UNKNOWN`

---

## Architecture / Design

_Not applicable — this delta reuses the existing `OnResolutionPostChange` callback
path and `FrameRate()` getter; no new architecture introduced. See `design.md` in this
change for the caching/locking decisions._

---

## External Interfaces

### JSON-RPC Event – `updated` (modified)

**Event name:** `DisplayInfo.1.updated`

#### Event payload (`Source` enum) — added value

| Value | Trigger |
|-------|---------|
| `FRAMERATE_CHANGE` | The active frame rate on the primary video output changed, detected during a resolution-change callback |

### Interface method – `CacheInitialFrameRateAsync` (new, internal)

**Method:** `DisplayInfoImplementation::CacheInitialFrameRateAsync()` (DeviceSettings backend only)

Not exposed as a JSON-RPC property or an `Exchange::IConnectionProperties` interface
method — spawned as a detached thread from the `DisplayInfoImplementation` constructor
to seed the frame-rate cache. Failures are logged and retried once after a fixed
delay; there is no return value surfaced to callers.

---

## Performance

No additional polling introduced. Frame-rate change detection piggy-backs on the
existing `OnResolutionPostChange` IARM/DS callback — one extra `FrameRate()` query per
resolution-change event, bounded by the same latency characteristics as the existing
`DisplayInfo.framerate` property read.

---

## Security

_Not applicable — no new externally-reachable input surface; `CacheInitialFrameRateAsync()`
is an internal backend detail, not exposed as a JSON-RPC method or interface method,
and the new `Source` enum value carries no payload._

---

## Versioning & Compatibility

Additive change: a new `Source` enum value only. Existing clients that switch on
`Source` and ignore unknown values are unaffected. No breaking changes.

---

## Conformance Testing & Validation

| Test name | Coverage |
|-----------|----------|
| `ResolutionChange_NotificationTest` (extended) | `Updated(FRAMERATE_CHANGE)` fires only when the queried frame rate differs from the cached value; not fired when unchanged; fired alongside `Updated(POST_RESOLUTION_CHANGE)`; baseline seeded via an `OnResolutionPostChange` callback since there is no interface method to call directly |

---

## Covered Code

- `plugin/DeviceSettings/PlatformImplementation.cpp`:
    - `DisplayInfoImplementation::DisplayInfoImplementation` (spawns the detached cache thread)
    - `DisplayInfoImplementation::CacheInitialFrameRateAsync`
    - `DisplayInfoImplementation::IsFrameRateChanged`
    - `DisplayInfoImplementation::OnResolutionPostChange` (emits `FRAMERATE_CHANGE`)
- `Tests/L1Tests/tests/test_DisplayInfo.cpp`:
    - `DisplayInfoTestTest::ResolutionChange_NotificationTest` (extended with
      `FRAMERATE_CHANGE` scenario)

---

## Open Queries

- **OQ-01:** Should `CacheInitialFrameRateAsync()`'s failure be surfaced anywhere
  beyond a log line, or remain silently best-effort as it is today? Currently
  silent/best-effort.
- **OQ-02:** Should Linux/DRM and BCM/RPI backends eventually implement real
  frame-rate change detection? Tracked as a future change; out of scope here.

---

## References

- `openspec/specs/displayinfo.spec.md` — canonical DisplayInfo spec (merge target)
- `interfaces/IDisplayInfo.h` — `Exchange::IConnectionProperties`

---

## Change History

- 2026-09-25 — displayinfo-framerate-change-event change — Delta requirements defined:
  `FRAMERATE_CHANGE` source, `InitializeFrameRate()` interface method, DeviceSettings
  backend caching/diff logic.
- 2026-09-28 — displayinfo-framerate-change-event change (revised) — Removed the
  `InitializeFrameRate()` interface method; the DeviceSettings backend now seeds the
  frame-rate cache asynchronously via `CacheInitialFrameRateAsync()`, spawned as a
  detached thread from the `DisplayInfoImplementation` constructor with one retry.
  Removed the Linux/RPI stub scenarios and Covered Code entries (no longer
  applicable); `ResolutionChange_NotificationTest` seeds its baseline via
  `OnResolutionPostChange` instead of the removed interface method.
