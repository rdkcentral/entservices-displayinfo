## MODIFIED Requirements

### Requirement: IDisplayProperties methods are covered by spec
The spec SHALL document all public methods of `Exchange::IDisplayProperties` implemented in each platform backend, including `Colorimetry`, `ColorSpace`, `FrameRate`, `ColourDepth`, `QuantizationRange`, and `EOTF`.

#### Scenario: Covered Code section lists all IDisplayProperties methods
- **WHEN** a reviewer audits the spec's Covered Code section
- **THEN** it SHALL include entries for `Colorimetry`, `ColorSpace`, `FrameRate`, `ColourDepth`, `QuantizationRange`, and `EOTF` for both DeviceSettings and RPI backends

## ADDED Requirements

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
