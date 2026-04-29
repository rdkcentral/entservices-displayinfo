# DisplayInfo Colorimetry Spec

## Overview

The **DisplayInfo Colorimetry** capability exposes EDID colorimetry data from connected displays
via the `DisplayInfo.colorimetry` read-only JSON-RPC property on the `DisplayInfo` plugin.

---

## Description

`DisplayInfo.colorimetry` returns an array of `ColorimetryType` enum values representing the
colorimetry modes reported by the connected display's EDID extension block (CTA-861). It is
implemented via `Exchange::IDisplayProperties::Colorimetry()`.

On the DeviceSettings backend the EDID is read from hardware on every call; all failure paths
(no display, unreadable EDID, failed `EDID_Verify()`, `device::Exception`) are treated as
"no colorimetry data" and return an empty list with `ERROR_NONE`. Backends that have not
implemented colorimetry discovery (Linux/DRM, BCM/RPI stubs) return `ERROR_UNAVAILABLE`.

---

## Requirements

### Functional

| ID | Requirement |
|----|-------------|
| REQ-F-01 | `DisplayInfo.colorimetry` SHALL be a read-only JSON-RPC property returning an array of `ColorimetryType` enum values. |
| REQ-F-02 | When an EDID colorimetry extension is present, each set bit in the colorimetry bitmask SHALL map to the corresponding `ColorimetryType` value as defined in `Exchange::IDisplayProperties`. |
| REQ-F-03 | When the colorimetry bitmask is zero, the property SHALL return an array containing `COLORIMETRY_UNKNOWN`. |
| REQ-F-04 | When no display is connected, the property SHALL return an empty array and `ERROR_NONE`. |
| REQ-F-05 | When EDID read or parse fails on the DeviceSettings backend, the property SHALL return an empty array and `ERROR_NONE`. |
| REQ-F-06 | When a `device::Exception` is thrown in the DeviceSettings backend, the property SHALL return an empty array and `ERROR_NONE`. |
| REQ-F-07 | When the platform backend does not support colorimetry discovery (Linux/DRM, BCM/RPI stubs), the property SHALL return `ERROR_UNAVAILABLE`. |
| REQ-F-08 | The `ColorimetryType` enumeration SHALL include values covering all colorimetry modes defined in CTA-861: `xvYCC601`, `xvYCC709`, `sYCC601`, `AdobeYCC601`, `AdobeRGB`, `BT2020cYCC`, `BT2020YCC`, `BT2020RGB`, `DCI-P3`, `COLORIMETRY_UNKNOWN`, and `COLORIMETRY_OTHER`. |

### Non-Functional

| ID | Requirement |
|----|-------------|
| REQ-NF-01 | The DeviceSettings backend `Colorimetry()` MUST NOT allocate EDID buffers with raw `new[]`/`delete[]`; it MUST use `std::vector<unsigned char>` or equivalent RAII containers. |

---

## Scenarios

### Scenario: External display connected with colorimetry data in EDID
- **WHEN** an external display is connected and its EDID contains colorimetry extension data
- **THEN** `DisplayInfo.colorimetry` SHALL return an array of one or more `ColorimetryType` enum values parsed from the EDID colorimetry bitmask

### Scenario: Built-in display (no external connection)
- **WHEN** the platform has a built-in display panel and no external display is connected
- **THEN** `DisplayInfo.colorimetry` SHALL return the colorimetry info of the built-in display panel

### Scenario: No display connected
- **WHEN** no display is connected (HDMI disconnected, no built-in panel active)
- **THEN** `DisplayInfo.colorimetry` SHALL return an empty array
- **THEN** the return code SHALL be `ERROR_NONE` (success — not an error condition)

### Scenario: Backend does not support colorimetry discovery
- **WHEN** the platform backend returns `ERROR_UNAVAILABLE` for the `Colorimetry()` interface method (e.g., Linux/DRM, BCM/RPI stubs)
- **THEN** `DisplayInfo.colorimetry` SHALL return `ERROR_UNAVAILABLE` to the caller
- **THEN** implementing colorimetry support for those backends is out of scope for this change (tracked as a future change)

### Scenario: EDID read or parse fails on DeviceSettings backend
- **WHEN** the EDID bytes cannot be read or `EDID_Verify()` fails on the DeviceSettings backend
- **THEN** `DisplayInfo.colorimetry` SHALL return an empty array
- **THEN** the return code SHALL be `ERROR_NONE` (failure to parse is treated as "no colorimetry data", not an error)

### Scenario: EDID present but colorimetry bitmask is zero
- **WHEN** the connected display's EDID is valid but the colorimetry extension block reports zero (no extended colorimetry capabilities)
- **THEN** `DisplayInfo.colorimetry` SHALL return an array containing `COLORIMETRY_UNKNOWN`

### Scenario: Device library throws exception
- **WHEN** a call into the DeviceSettings library throws a `device::Exception`
- **THEN** `DisplayInfo.colorimetry` SHALL return an empty array
- **THEN** the return code SHALL be `ERROR_NONE`

### Scenario: Known EDID colorimetry bitmask bits are mapped
- **WHEN** an EDID colorimetry bitmask contains bits for xvYCC601, xvYCC709, sYCC601, AdobeYCC601, AdobeRGB, BT2020cYCC/YCC, BT2020RGB, or DCI-P3
- **THEN** each set bit SHALL map to a corresponding `ColorimetryType` enum value as defined in `Exchange::IDisplayProperties`

### Scenario: Unknown or unrecognised colorimetry data
- **WHEN** an EDID colorimetry bitmask contains bits not mapped to a known `ColorimetryType`
- **THEN** those bits SHALL map to `COLORIMETRY_OTHER`

---

## External Interfaces

### JSON-RPC Property – `colorimetry`

**Method type:** Property (read-only)  
**Endpoint:** `DisplayInfo.1.colorimetry`

#### Response

An array of `ColorimetryType` enum values. May be empty when no display is connected or EDID
is unreadable.

#### `ColorimetryType` enum

| Value | CTA-861 bit | Description |
|-------|-------------|-------------|
| `COLORIMETRY_UNKNOWN` | — | Zero bitmask (no colorimetry data reported in EDID) |
| `COLORIMETRY_OTHER` | unrecognised bits | Bits present but not mapped to a known mode |
| `xvYCC601` | bit 0 | xvYCC BT.601 |
| `xvYCC709` | bit 1 | xvYCC BT.709 |
| `sYCC601` | bit 2 | sYCC BT.601 |
| `AdobeYCC601` | bit 3 | AdobeYCC BT.601 |
| `AdobeRGB` | bit 4 | AdobeRGB |
| `BT2020cYCC` | bit 5 | BT.2020 cYCC |
| `BT2020YCC` | bit 6 | BT.2020 YCC |
| `BT2020RGB` | bit 7 | BT.2020 RGB |
| `DCI-P3` | bit 15 | DCI-P3 (HDMI 2.x extended colorimetry) |

#### Error codes

| Code | Meaning |
|------|---------|
| `ERROR_NONE` (0) | Success (including empty-list cases) |
| `ERROR_UNAVAILABLE` | Backend does not support colorimetry discovery |

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

- **OQ-01:** Should Linux/DRM and BCM/RPI backends implement colorimetry discovery via EDID
  reads in a future change?
- **OQ-02:** Is DCI-P3 reported via HDMI 2.x vendor-specific extension blocks on the
  DeviceSettings backend, or is it always set via the standard CTA-861 colorimetry block?

---

## References

- `interfaces/IDisplayInfo.h` — `Exchange::IDisplayProperties::Colorimetry()`
- `interfaces/json/JDisplayProperties.h` — `JDisplayProperties` auto-registration
- CTA-861 — HDMI Extended Colorimetry Data Block (CEA Extension)
- EDID standard — VESA EDID v1.4

---

## Change History

- 2026-04-29 — displayinfo-colorimetry change — Initial spec created for Colorimetry property capability.
- 2026-04-29 — openspec-sync-specs — Synced from change spec to canonical specs directory.
