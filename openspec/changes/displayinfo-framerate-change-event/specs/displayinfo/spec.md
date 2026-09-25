# DisplayInfo — Frame Rate Change Event — Change Delta Spec

## Overview

This spec captures the requirements added or modified in the
`displayinfo-framerate-change-event` change: a new `FRAMERATE_CHANGE` source for the
`Updated` JSON-RPC event, a new `InitializeFrameRate()` interface method, and the
DeviceSettings backend logic that caches and diffs the active frame rate on every
resolution-change callback.

---

## Description

Prior to this change, `Exchange::IConnectionProperties::INotification::Source` only
defined `PRE_RESOLUTION_CHANGE`, `POST_RESOLUTION_CHANGE`, `HDMI_CHANGE`, and
`HDCP_CHANGE`. Clients had no event-driven way to learn that the active frame rate
changed as a side effect of a resolution change. This delta adds `FRAMERATE_CHANGE`,
a paired `InitializeFrameRate()` interface method used to seed a baseline cache at
plugin startup, and the DeviceSettings backend implementation that compares the
current frame rate against the cache on every `OnResolutionPostChange` callback.

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

### Requirement: Frame rate is cached during plugin initialization
`Exchange::IConnectionProperties` SHALL expose `InitializeFrameRate()`, which the
`DisplayInfo` plugin SHALL call once, immediately after acquiring the
`IConnectionProperties` interface in `Initialize()` and before registering
notification observers.

#### Scenario: Plugin initialization caches the current frame rate
- **WHEN** `DisplayInfo::Initialize` successfully acquires `_connectionProperties`
- **THEN** it SHALL call `_connectionProperties->InitializeFrameRate()` before
  `_connectionProperties->Register(&_notification)`

#### Scenario: InitializeFrameRate succeeds (DeviceSettings backend)
- **WHEN** the current resolution/frame rate is readable
- **THEN** `InitializeFrameRate()` SHALL cache the mapped `FrameRateType` value
- **THEN** `InitializeFrameRate()` SHALL return `ERROR_NONE`

#### Scenario: InitializeFrameRate fails (DeviceSettings backend)
- **WHEN** a `device::Exception`, `std::exception`, or unknown exception is thrown
  while reading the resolution/frame rate
- **THEN** `InitializeFrameRate()` SHALL return `ERROR_GENERAL`
- **THEN** the cached frame rate SHALL retain its last-known value (unmodified on
  exception)

#### Scenario: InitializeFrameRate on Linux/DRM backend
- **WHEN** `InitializeFrameRate()` is called on the Linux/DRM backend
- **THEN** it SHALL return `ERROR_NOT_SUPPORTED`

#### Scenario: InitializeFrameRate on BCM/RPI backend
- **WHEN** `InitializeFrameRate()` is called on the BCM/RPI backend
- **THEN** it SHALL return `ERROR_UNAVAILABLE`

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

### Interface method – `InitializeFrameRate` (new)

**Interface method:** `Exchange::IConnectionProperties::InitializeFrameRate()`

Not exposed as a JSON-RPC property — called internally by `DisplayInfo::Initialize`
to seed the frame-rate cache.

| Condition | Return code |
|-----------|-------------|
| Resolution/frame rate readable (DeviceSettings) | `ERROR_NONE` |
| `device::Exception` / `std::exception` / unknown exception (DeviceSettings) | `ERROR_GENERAL` |
| Linux/DRM backend | `ERROR_NOT_SUPPORTED` |
| BCM/RPI backend | `ERROR_UNAVAILABLE` |

---

## Performance

No additional polling introduced. Frame-rate change detection piggy-backs on the
existing `OnResolutionPostChange` IARM/DS callback — one extra `FrameRate()` query per
resolution-change event, bounded by the same latency characteristics as the existing
`DisplayInfo.framerate` property read.

---

## Security

_Not applicable — no new externally-reachable input surface; `InitializeFrameRate()`
is not exposed as a JSON-RPC method and the new `Source` enum value carries no
payload._

---

## Versioning & Compatibility

Additive change: a new `Source` enum value and a new interface method. Existing
clients that switch on `Source` and ignore unknown values are unaffected. No breaking
changes.

---

## Conformance Testing & Validation

| Test name | Coverage |
|-----------|----------|
| `InitializeFrameRate_Success` | `InitializeFrameRate()` caches the current frame rate and returns `ERROR_NONE` |
| `InitializeFrameRate_ExceptionHandling` | `InitializeFrameRate()` returns `ERROR_GENERAL` when the underlying resolution query throws |
| `ResolutionChange_NotificationTest` (extended) | `Updated(FRAMERATE_CHANGE)` fires only when the queried frame rate differs from the cached value; not fired when unchanged; fired alongside `Updated(POST_RESOLUTION_CHANGE)` |

---

## Covered Code

- `plugin/DisplayInfo.cpp`:
    - `DisplayInfo::Initialize` (added `_connectionProperties->InitializeFrameRate()` call)
- `plugin/DeviceSettings/PlatformImplementation.cpp`:
    - `DisplayInfoImplementation::InitializeFrameRate`
    - `DisplayInfoImplementation::IsFrameRateChanged`
    - `DisplayInfoImplementation::OnResolutionPostChange` (emits `FRAMERATE_CHANGE`)
- `plugin/Linux/PlatformImplementation.cpp`:
    - `DisplayInfoImplementation::InitializeFrameRate` (stub — `ERROR_NOT_SUPPORTED`)
- `plugin/RPI/PlatformImplementation.cpp`:
    - `DisplayInfoImplementation::InitializeFrameRate` (stub — `ERROR_UNAVAILABLE`)
- `Tests/L1Tests/tests/test_DisplayInfo.cpp`:
    - `DisplayInfoTestTest::InitializeFrameRate_Success`
    - `DisplayInfoTestTest::InitializeFrameRate_ExceptionHandling`
    - `DisplayInfoTestTest::ResolutionChange_NotificationTest` (extended with
      `FRAMERATE_CHANGE` scenario)

---

## Open Queries

- **OQ-01:** Should `InitializeFrameRate()`'s return value be surfaced through
  `DisplayInfo::Initialize`'s `message` output, or remain silently ignored? Currently
  ignored, consistent with other best-effort calls in `Initialize`.
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
