## ADDED Requirements

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
- **THEN** `DisplayInfo.colorimetry` SHALL return an empty array
- **THEN** the return code SHALL be `ERROR_NONE`

#### Scenario: EDID present but colorimetry bitmask is zero
- **WHEN** the connected display's EDID is valid but the colorimetry extension block reports zero (no extended colorimetry capabilities)
- **THEN** `DisplayInfo.colorimetry` SHALL return an array containing `COLORIMETRY_UNKNOWN`

### Requirement: Colorimetry enum values are well-defined
The `ColorimetryType` enumeration SHALL cover all colorimetry modes parsed from EDID extension blocks as defined by CTA-861.

#### Scenario: Known EDID colorimetry bitmask bits are mapped
- **WHEN** an EDID colorimetry bitmask contains bits for xvYCC601, xvYCC709, sYCC601, AdobeYCC601, AdobeRGB, BT2020cYCC/YCC, BT2020RGB, or DCI-P3
- **THEN** each set bit SHALL map to a corresponding `ColorimetryType` enum value as defined in `Exchange::IDisplayProperties`

#### Scenario: Unknown or unrecognised colorimetry data
- **WHEN** an EDID colorimetry bitmask contains bits not mapped to a known `ColorimetryType`
- **THEN** those bits SHALL map to `COLORIMETRY_OTHER`
