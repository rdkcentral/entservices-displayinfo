## MODIFIED Requirements

### Requirement: IDisplayProperties methods are covered by spec
The spec SHALL document all public methods of `Exchange::IDisplayProperties` implemented in each platform backend, including `Colorimetry`, `ColorSpace`, `FrameRate`, `ColourDepth`, `QuantizationRange`, and `EOTF`.

#### Scenario: Covered Code section lists all IDisplayProperties methods
- **WHEN** a reviewer audits the spec's Covered Code section
- **THEN** it SHALL include entries for `Colorimetry`, `ColorSpace`, `FrameRate`, `ColourDepth`, `QuantizationRange`, and `EOTF` for both DeviceSettings and RPI backends

## ADDED Requirements

### Requirement: Colorimetry disconnected path returns empty list with success
The DeviceSettings backend `Colorimetry()` implementation SHALL return an empty `IColorimetryIterator` with `ERROR_NONE` when no display is connected, rather than `ERROR_GENERAL`.

#### Scenario: No display connected — DeviceSettings backend
- **WHEN** `GetEdidBytes()` returns a non-`ERROR_NONE` code indicating no connected display
- **THEN** `Colorimetry()` SHALL set the output iterator to an empty list
- **THEN** `Colorimetry()` SHALL return `ERROR_NONE`

#### Scenario: EDID present but parse fails — DeviceSettings backend
- **WHEN** `GetEdidBytes()` returns `ERROR_NONE` but `EDID_Verify()` fails
- **THEN** `Colorimetry()` SHALL return `ERROR_GENERAL` (unchanged behaviour)
