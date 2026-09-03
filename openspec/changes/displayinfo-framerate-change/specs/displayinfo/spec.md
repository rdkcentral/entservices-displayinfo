# DisplayInfo — Frame Rate Change Notification Delta Spec

## Overview

This spec captures the requirements added to the `displayinfo` canonical spec as part of the `displayinfo-framerate-change` change: a new `FRAMERATE_CHANGE` value for the `updated` event's `Source` enum, and frame-rate caching/change-detection behaviour in the DeviceSettings backend triggered by `OnResolutionPostChange`.

---

## Description

The `displayinfo-framerate-change` change adds frame-rate change detection to the DeviceSettings backend of the `DisplayInfo` plugin. On plugin initialisation, the current frame rate is cached. Whenever the DeviceSettings backend receives `OnResolutionPostChange` (indicating a display resolution change has completed), the frame rate is re-queried and compared against the cached value. If the frame rate differs, the cache is updated and an `Updated(FRAMERATE_CHANGE)` notification is emitted to all registered `IConnectionProperties::INotification` observers, in addition to the existing `Updated(POST_RESOLUTION_CHANGE)` notification.

`FRAMERATE_CHANGE` is a new value on `Exchange::IConnectionProperties::INotification::Source`, defined in the external `entservices-apis` interfaces repository. This delta assumes that value exists; the corresponding interfaces-repo change is tracked separately.

---

## Requirements

### Requirement: Updated event supports FRAMERATE_CHANGE source
The `updated` event's `Source` enum SHALL include a `FRAMERATE_CHANGE` value, in addition to the existing `PRE_RESOLUTION_CHANGE`, `POST_RESOLUTION_CHANGE`, `HDMI_CHANGE`, and `HDCP_CHANGE` values.

#### Scenario: Documentation lists FRAMERATE_CHANGE as a valid Source value
- **WHEN** a reviewer audits the `updated` event payload documentation
- **THEN** `FRAMERATE_CHANGE` SHALL be listed alongside `PRE_RESOLUTION_CHANGE`, `POST_RESOLUTION_CHANGE`, `HDMI_CHANGE`, and `HDCP_CHANGE`

### Requirement: Frame rate is cached on plugin initialisation (DeviceSettings backend)
On construction, the DeviceSettings backend `DisplayInfoImplementation` SHALL query `FrameRate()` once and store the result in a cache member variable.

#### Scenario: Cache populated at construction
- **WHEN** `DisplayInfoImplementation` is constructed on the DeviceSettings backend
- **THEN** the implementation SHALL call `FrameRate()` and store the resulting `FrameRateType` value in a cache member variable

### Requirement: Frame rate is re-queried and compared on OnResolutionPostChange (DeviceSettings backend)
Whenever the DeviceSettings backend receives `OnResolutionPostChange`, the implementation SHALL query `FrameRate()` again and compare the result against the cached value.

#### Scenario: Frame rate queried after resolution change completes
- **WHEN** `OnResolutionPostChange` is invoked on the DeviceSettings backend
- **THEN** the implementation SHALL call `FrameRate()` and compare the returned value against the cached frame rate

### Requirement: Frame rate cache is updated on every OnResolutionPostChange (DeviceSettings backend)
Regardless of whether the queried frame rate differs from the cached value, the cache SHALL be updated to the newly queried value after every `OnResolutionPostChange`.

#### Scenario: Cache updated with newly queried value
- **WHEN** `OnResolutionPostChange` queries a frame rate value (changed or unchanged)
- **THEN** the cache member variable SHALL be set to the newly queried value

### Requirement: FRAMERATE_CHANGE is emitted only when the frame rate changes (DeviceSettings backend)
The DeviceSettings backend SHALL emit `Updated(FRAMERATE_CHANGE)` to all registered `IConnectionProperties::INotification` observers if and only if the frame rate queried during `OnResolutionPostChange` differs from the previously cached value.

#### Scenario: Frame rate changed — notification emitted
- **WHEN** `OnResolutionPostChange` queries a frame rate that differs from the cached value
- **THEN** the implementation SHALL call `Updated(FRAMERATE_CHANGE)` on every registered observer

#### Scenario: Frame rate unchanged — no notification emitted
- **WHEN** `OnResolutionPostChange` queries a frame rate equal to the cached value
- **THEN** the implementation SHALL NOT call `Updated(FRAMERATE_CHANGE)` on any observer

#### Scenario: POST_RESOLUTION_CHANGE is still emitted independently
- **WHEN** `OnResolutionPostChange` is invoked, regardless of whether the frame rate changed
- **THEN** the implementation SHALL still call `Updated(POST_RESOLUTION_CHANGE)` on every registered observer, unchanged from existing behaviour

---

## Architecture / Design

See `design.md` in this change for the full design rationale (cache member placement, locking strategy, and the reuse of `ResolutionChangeImpl` for both `POST_RESOLUTION_CHANGE` and `FRAMERATE_CHANGE` notifications).

---

## External Interfaces

### JSON-RPC Event – `updated` (extended)

**Event name:** `DisplayInfo.1.updated`

#### Event payload (`Source` enum) — new value

| Value | Trigger |
|-------|---------|
| `FRAMERATE_CHANGE` | The frame rate of the connected display changed as a result of a resolution change |

This value is additive to the existing `Source` enum documented in `openspec/specs/displayinfo.spec.md`.

---

## Performance

Re-querying `FrameRate()` on every `OnResolutionPostChange` adds one DeviceSettings library call per resolution-change event. Resolution changes are low-frequency (user- or source-triggered), so this has no measurable impact on steady-state performance.

---

## Security

_Not applicable — this delta only adds an internal comparison and an additional enum value to an existing notification path; no new external attack surface is introduced._

---

## Versioning & Compatibility

Adding `FRAMERATE_CHANGE` to the `Source` enum is additive and backwards-compatible for existing clients that switch on known `Source` values (unrecognised values are typically ignored by well-behaved clients). This delta depends on a corresponding additive change to the `Exchange::IConnectionProperties::INotification::Source` enum in the external `entservices-apis` repository; until that dependency lands, this repository's code will not compile against the new enum value.

---

## Conformance Testing & Validation

| Test name | Coverage |
|-----------|----------|
| `FrameRate_CachedOnConstruction` | Constructor calls `FrameRate()` and populates the cache |
| `FrameRate_Unchanged_NoNotification` | `OnResolutionPostChange` with an unchanged frame rate does not emit `Updated(FRAMERATE_CHANGE)` |
| `FrameRate_Changed_EmitsNotification` | `OnResolutionPostChange` with a changed frame rate emits `Updated(FRAMERATE_CHANGE)` and updates the cache |
| `FrameRate_Changed_PostResolutionChangeStillEmitted` | `Updated(POST_RESOLUTION_CHANGE)` is still emitted independently of the frame-rate outcome |

---

## Covered Code

- `plugin/DeviceSettings/PlatformImplementation.cpp`:
    - `DisplayInfoImplementation::DisplayInfoImplementation` (constructor — cache initialisation)
    - `DisplayInfoImplementation::OnResolutionPostChange`
    - `DisplayInfoImplementation::FrameRate`
    - `DisplayInfoImplementation::ResolutionChangeImpl`

---

## Open Queries

_No open queries._

---

## References

- `openspec/specs/displayinfo.spec.md` — canonical DisplayInfo spec (merged target), `updated` event `Source` enum
- `interfaces/IDisplayInfo.h` (`entservices-apis` repository) — `Exchange::IConnectionProperties::INotification::Source`, `Exchange::IDisplayProperties::FrameRate`

---

## Change History

- 2026-09-03 — displayinfo-framerate-change — Added `FRAMERATE_CHANGE` source, frame-rate caching, and change-detection requirements for the DeviceSettings backend.
