# DisplayInfo Colorimetry — Change Spec

## Overview

This spec captures the requirements introduced by the `2026-04-29-displayinfo-colorimetry` change: exposing EDID colorimetry data from connected displays via the `DisplayInfo.colorimetry` JSON-RPC property.

---

## Description

Prior to this change, the `DisplayInfo` plugin had no JSON-RPC binding for `Exchange::IDisplayProperties::Colorimetry()`, which was already implemented in the DeviceSettings and RPI backends. This change adds the binding, defines the complete set of `ColorimetryType` enum values, and specifies all failure-path behaviours (no display connected, EDID unreadable, `device::Exception`).

---

## Requirements

### Requirement: Colorimetry property returns supported colorimetry modes
The plugin SHALL expose a read-only JSON-RPC property `DisplayInfo.colorimetry` that returns the list of colorimetry modes reported by the connected display's EDID or built-in panel capabilities.

#### Scenario: External display connected with colorimetry data in EDID
- **WHEN** an external display is connected and its EDID contains colorimetry extension data
- **THEN** `DisplayInfo.colorimetry` SHALL return an array of one or more `ColorimetryType` enum values parsed from the EDID colorimetry bitmask

#### Scenario: Built-in display (no external connection)
- **WHEN** the platform has a built-in display panel and no external display is connected
- **THEN** `DisplayInfo.colorimetry` SHALL return the colorimetry info of the built-in display panel

#### Scenario: No display connected
- **WHEN** no display is connected (HDMI disconnected, no built-in panel active)
- **THEN** `DisplayInfo.colorimetry` SHALL return an empty array
- **THEN** the return code SHALL be `ERROR_NONE` (success — not an error condition)

#### Scenario: Backend does not support colorimetry discovery
- **WHEN** the platform backend returns `ERROR_UNAVAILABLE` for the `Colorimetry()` interface method (e.g., Linux/DRM, BCM/RPI stubs)
- **THEN** `DisplayInfo.colorimetry` SHALL return `ERROR_UNAVAILABLE` to the caller
- **THEN** implementing colorimetry support for those backends is out of scope for this change (tracked as a future change)

#### Scenario: EDID read or parse fails on DeviceSettings backend
- **WHEN** the EDID bytes cannot be read or `EDID_Verify()` fails on the DeviceSettings backend
- **THEN** `DisplayInfo.colorimetry` SHALL return an empty array
- **THEN** the return code SHALL be `ERROR_NONE` (failure to parse is treated as "no colorimetry data", not an error)

#### Scenario: EDID present but colorimetry bitmask is zero
- **WHEN** the connected display's EDID is valid but the colorimetry extension block reports zero (no extended colorimetry capabilities)
- **THEN** `DisplayInfo.colorimetry` SHALL return an array containing `COLORIMETRY_UNKNOWN`

#### Scenario: Device library throws exception
- **WHEN** a call into the DeviceSettings library throws a `device::Exception`
- **THEN** `DisplayInfo.colorimetry` SHALL return an empty array
- **THEN** the return code SHALL be `ERROR_NONE`

### Requirement: Colorimetry enum values are well-defined
The `ColorimetryType` enumeration SHALL cover all colorimetry modes parsed from EDID extension blocks as defined by CTA-861.

#### Scenario: Known EDID colorimetry bitmask bits are mapped
- **WHEN** an EDID colorimetry bitmask contains bits for xvYCC601, xvYCC709, sYCC601, AdobeYCC601, AdobeRGB, BT2020cYCC/YCC, BT2020RGB, or DCI-P3
- **THEN** each set bit SHALL map to a corresponding `ColorimetryType` enum value as defined in `Exchange::IDisplayProperties`

#### Scenario: Unknown or unrecognised colorimetry data
- **WHEN** an EDID colorimetry bitmask contains bits not mapped to a known `ColorimetryType`
- **THEN** those bits SHALL map to `COLORIMETRY_OTHER`

---

## Architecture / Design

_Not applicable — single JSON-RPC property binding via `JDisplayProperties` auto-registration; no separate service layer. See `design.md` in this archive for implementation decisions._

---

## External Interfaces

### JSON-RPC Property – `colorimetry`

**Method type:** Property (read-only)  
**Endpoint:** `DisplayInfo.1.colorimetry`  
**Interface method:** `Exchange::IDisplayProperties::Colorimetry(IColorimetryIterator*&)`

#### `ColorimetryType` enum

| Value | CTA-861 bit | Description |
|-------|-------------|-------------|
| `COLORIMETRY_UNKNOWN` | — | Zero bitmask (no colorimetry data in EDID) |
| `COLORIMETRY_OTHER` | unrecognised bits | Bits present but not mapped |
| `xvYCC601` | bit 0 | xvYCC BT.601 |
| `xvYCC709` | bit 1 | xvYCC BT.709 |
| `sYCC601` | bit 2 | sYCC BT.601 |
| `AdobeYCC601` | bit 3 | AdobeYCC BT.601 |
| `AdobeRGB` | bit 4 | AdobeRGB |
| `BT2020cYCC` | bit 5 | BT.2020 cYCC |
| `BT2020YCC` | bit 6 | BT.2020 YCC |
| `BT2020RGB` | bit 7 | BT.2020 RGB |
| `DCI-P3` | bit 15 | DCI-P3 |

---

## Performance

_Not applicable — EDID read on every call; see canonical `displayinfo-colorimetry/spec.md` for details._

---

## Security

_Not applicable — read-only property with no user-supplied input._

---

## Versioning & Compatibility

Introduced in `DisplayInfo.1`. No breaking changes to existing properties.

---

## Conformance Testing & Validation

| Test name | Coverage |
|-----------|----------|
| `Colorimetry` | EDID colorimetry bitmask parsing and `ColorimetryType` mapping |

---

## Covered Code

- `plugin/DeviceSettings/PlatformImplementation.cpp`:
    - `DisplayInfoImplementation::Colorimetry`
- `plugin/RPI/PlatformImplementation.cpp`:
    - `DisplayInfoImplementation::Colorimetry` (stub — returns `ERROR_UNAVAILABLE`)
- `plugin/Linux/PlatformImplementation.cpp`:
    - `DisplayProperties::Colorimetry` (stub — returns `ERROR_UNAVAILABLE`)
- `plugin/DisplayInfoJsonRpc.cpp`:
    - Auto-registered via `JDisplayProperties` — `colorimetry` get handler

---

## Open Queries

- **OQ-01:** Should Linux/DRM and BCM/RPI backends implement colorimetry discovery via EDID reads in a future change?
- **OQ-02:** Is DCI-P3 reported via HDMI 2.x vendor-specific extension blocks or always via the standard CTA-861 colorimetry block?

---

## References

- `interfaces/IDisplayInfo.h` — `Exchange::IDisplayProperties::Colorimetry()`
- `interfaces/json/JDisplayProperties.h` — `JDisplayProperties` auto-registration
- CTA-861 — HDMI Extended Colorimetry Data Block
- EDID standard — VESA EDID v1.4

---

## Change History

- 2026-04-29 — displayinfo-colorimetry change — Initial requirements defined for Colorimetry property.
- 2026-07-24 — openspec-templater — Restructured to match spec template; full sections added.
