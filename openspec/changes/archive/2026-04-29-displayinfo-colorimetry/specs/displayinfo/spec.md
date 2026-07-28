# DisplayInfo — Colorimetry Change Delta Spec

## Overview

This spec captures the requirements added or modified in the `displayinfo` canonical spec as part of the `2026-04-29-displayinfo-colorimetry` change: closing gap G-01 (orphaned `IDisplayProperties` methods with no spec coverage) and enforcing correct error-handling and RAII semantics for the DeviceSettings `Colorimetry()` implementation.

---

## Description

The `2026-04-29-displayinfo-colorimetry` change identified that `Exchange::IDisplayProperties` methods (`Colorimetry`, `ColorSpace`, `FrameRate`, `ColourDepth`, `QuantizationRange`, `EOTF`) were implemented in both DeviceSettings and RPI backends but were not covered in the `displayinfo` spec. This delta adds the missing spec coverage and tightens the error-handling and memory-management requirements for the DeviceSettings `Colorimetry()` implementation.

---

## Requirements

### Requirement: IDisplayProperties methods are covered by spec
The spec SHALL document all public methods of `Exchange::IDisplayProperties` implemented in each platform backend, including `Colorimetry`, `ColorSpace`, `FrameRate`, `ColourDepth`, `QuantizationRange`, and `EOTF`.

#### Scenario: Covered Code section lists all IDisplayProperties methods
- **WHEN** a reviewer audits the spec's Covered Code section
- **THEN** it SHALL include entries for `Colorimetry`, `ColorSpace`, `FrameRate`, `ColourDepth`, `QuantizationRange`, and `EOTF` for both DeviceSettings and RPI backends

### Requirement: Colorimetry always returns success with empty list on non-connected paths (DeviceSettings)
The DeviceSettings backend `Colorimetry()` implementation SHALL return an empty `IColorimetryIterator` with `ERROR_NONE` for all non-success paths: display not connected, EDID read failure, EDID verification failure, and `device::Exception`. It SHALL never return `ERROR_GENERAL` to callers.

#### Scenario: No display connected — DeviceSettings backend
- **WHEN** `isDisplayConnected()` returns false
- **THEN** `Colorimetry()` SHALL set the output iterator to an empty list
- **THEN** `Colorimetry()` SHALL return `ERROR_NONE`

#### Scenario: EDID present but verification fails — DeviceSettings backend
- **WHEN** `EDID_Verify()` returns a non-OK status
- **THEN** `Colorimetry()` SHALL set the output iterator to an empty list
- **THEN** `Colorimetry()` SHALL return `ERROR_NONE`

#### Scenario: DeviceSettings library exception — DeviceSettings backend
- **WHEN** a `device::Exception` is thrown during EDID retrieval or parsing
- **THEN** `Colorimetry()` SHALL set the output iterator to an empty list
- **THEN** `Colorimetry()` SHALL return `ERROR_NONE`

### Requirement: Colorimetry implementation uses RAII memory management
The DeviceSettings backend `Colorimetry()` implementation SHALL NOT use manual heap allocation (`new[]`/`delete[]`) for EDID byte buffers. It SHALL use `std::vector` or equivalent RAII containers.

#### Scenario: Implementation uses vector for EDID buffer
- **WHEN** `Colorimetry()` reads EDID bytes from the display
- **THEN** the implementation SHALL store EDID bytes in a `std::vector<unsigned char>` (not a raw `new unsigned char[]`)

---

## Architecture / Design

_Not applicable — this is a delta spec for an existing plugin; no new architecture introduced._

---

## External Interfaces

_Not applicable — no new JSON-RPC surface added by this delta; the `colorimetry` property binding is covered by the `displayinfo-colorimetry` spec in this same archive._

---

## Performance

_Not applicable — no performance-sensitive changes introduced by this delta._

---

## Security

_Not applicable — changes are internal implementation correctness (RAII, error-path) with no security surface impact._

---

## Versioning & Compatibility

Changes are backwards-compatible: the error-handling fix changes `ERROR_GENERAL` to `ERROR_NONE` on disconnected/EDID-fail paths, which is less restrictive for clients that previously checked for errors.

---

## Conformance Testing & Validation

| Test name | Coverage |
|-----------|----------|
| `Colorimetry` | EDID bitmask parse; empty list on disconnect / EDID fail / exception |

---

## Covered Code

- `plugin/DeviceSettings/PlatformImplementation.cpp`:
    - `DisplayInfoImplementation::Colorimetry`
    - `DisplayInfoImplementation::ColorSpace`
    - `DisplayInfoImplementation::FrameRate`
    - `DisplayInfoImplementation::ColourDepth`
    - `DisplayInfoImplementation::QuantizationRange`
    - `DisplayInfoImplementation::EOTF`
- `plugin/RPI/PlatformImplementation.cpp`:
    - `DisplayInfoImplementation::Colorimetry`
    - `DisplayInfoImplementation::ColorSpace`
    - `DisplayInfoImplementation::FrameRate`
    - `DisplayInfoImplementation::ColourDepth`
    - `DisplayInfoImplementation::QuantizationRange`
    - `DisplayInfoImplementation::EOTF`

---

## Open Queries

_No open queries._

---

## References

- `openspec/specs/displayinfo.spec.md` — canonical DisplayInfo spec (merged target)
- `interfaces/IDisplayInfo.h` — `Exchange::IDisplayProperties`

---

## Change History

- 2026-04-29 — displayinfo-colorimetry change — Delta requirements defined: gap G-01 coverage, Colorimetry error-handling, RAII memory management.
- 2026-07-24 — openspec-templater — Restructured to match spec template; all sections added.
