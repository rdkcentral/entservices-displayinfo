## ADDED Requirements

### Requirement: getCurrentColorimetry property exposed
The `DisplayInfo` plugin SHALL expose a `getCurrentColorimetry` read-only JSON-RPC property that returns a single `colorimetry` string enum value representing the active colorimetry standard on the HDMI output link.

#### Scenario: Display connected with known matrix coefficient
- **WHEN** a display is connected and `vPort.getMatrixCoefficients()` returns a known `dsDisplayMatrixCoefficients_t` value
- **THEN** `DisplayInfo.getCurrentColorimetry` SHALL return the corresponding `ColorimetryType` string (e.g., `"COLORIMETRY_BT2020RGB_YCBCR"`)
- **THEN** the return code SHALL be `ERROR_NONE`

#### Scenario: No display connected on any port
- **WHEN** no display is connected on any video output port (all `vPort.isDisplayConnected()` calls return false across all ports returned by `getVideoOutputPorts()`)
- **THEN** `DisplayInfo.getCurrentColorimetry` SHALL return `"COLORIMETRY_UNKNOWN"`
- **THEN** the return code SHALL be `ERROR_NONE`

### Requirement: DS matrix coefficient to colorimetry mapping
The DeviceSettings backend SHALL map `dsDisplayMatrixCoefficients_t` values to `ColorimetryType` enum values as follows: `dsDISPLAY_MATRIXCOEFFICIENT_BT_709` → `COLORIMETRY_BT709`; `dsDISPLAY_MATRIXCOEFFICIENT_SMPTE_170M` → `COLORIMETRY_SMPTE170M`; `dsDISPLAY_MATRIXCOEFFICIENT_XvYCC_709` → `COLORIMETRY_XVYCC709`; `dsDISPLAY_MATRIXCOEFFICIENT_eXvYCC_601` → `COLORIMETRY_XVYCC601`; `dsDISPLAY_MATRIXCOEFFICIENT_BT_2020_NCL` → `COLORIMETRY_BT2020RGB_YCBCR`; `dsDISPLAY_MATRIXCOEFFICIENT_BT_2020_CL` → `COLORIMETRY_BT2020YCCBCBRC`; `dsDISPLAY_MATRIXCOEFFICIENT_UNKNOWN` → `COLORIMETRY_UNKNOWN`; all other values not in the above list → `COLORIMETRY_OTHER`.

#### Scenario: BT.709 matrix coefficient active
- **WHEN** `vPort.getMatrixCoefficients()` returns `dsDISPLAY_MATRIXCOEFFICIENT_BT_709`
- **THEN** the property SHALL return `"COLORIMETRY_BT709"`

#### Scenario: BT.2020 NCL matrix coefficient active
- **WHEN** `vPort.getMatrixCoefficients()` returns `dsDISPLAY_MATRIXCOEFFICIENT_BT_2020_NCL`
- **THEN** the property SHALL return `"COLORIMETRY_BT2020RGB_YCBCR"`

#### Scenario: BT.2020 CL matrix coefficient active
- **WHEN** `vPort.getMatrixCoefficients()` returns `dsDISPLAY_MATRIXCOEFFICIENT_BT_2020_CL`
- **THEN** the property SHALL return `"COLORIMETRY_BT2020YCCBCBRC"`

#### Scenario: SMPTE 170M matrix coefficient active
- **WHEN** `vPort.getMatrixCoefficients()` returns `dsDISPLAY_MATRIXCOEFFICIENT_SMPTE_170M`
- **THEN** the property SHALL return `"COLORIMETRY_SMPTE170M"`

#### Scenario: xvYCC 709 matrix coefficient active
- **WHEN** `vPort.getMatrixCoefficients()` returns `dsDISPLAY_MATRIXCOEFFICIENT_XvYCC_709`
- **THEN** the property SHALL return `"COLORIMETRY_XVYCC709"`

#### Scenario: xvYCC 601 matrix coefficient active
- **WHEN** `vPort.getMatrixCoefficients()` returns `dsDISPLAY_MATRIXCOEFFICIENT_eXvYCC_601`
- **THEN** the property SHALL return `"COLORIMETRY_XVYCC601"`

#### Scenario: DS UNKNOWN sentinel matrix coefficient active
- **WHEN** `vPort.getMatrixCoefficients()` returns `dsDISPLAY_MATRIXCOEFFICIENT_UNKNOWN`
- **THEN** the property SHALL return `"COLORIMETRY_UNKNOWN"`

#### Scenario: Genuinely unmapped matrix coefficient active
- **WHEN** `vPort.getMatrixCoefficients()` returns a `dsDisplayMatrixCoefficients_t` value that is not `dsDISPLAY_MATRIXCOEFFICIENT_UNKNOWN` and has no defined mapping
- **THEN** the property SHALL return `"COLORIMETRY_OTHER"`

### Requirement: Port iteration strategy
The DeviceSettings backend `GetCurrentColorimetry()` SHALL iterate all available video output ports returned by `device::Host::getInstance().getVideoOutputPorts()` and use the first port for which `vPort.isDisplayConnected()` returns true to obtain the matrix coefficient. If no connected port is found, the method SHALL return `COLORIMETRY_UNKNOWN` with `ERROR_NONE`.

#### Scenario: First connected port used when multiple ports present
- **WHEN** `getVideoOutputPorts()` returns multiple ports and the first connected port is not the platform default port
- **THEN** `GetCurrentColorimetry()` SHALL use the first port in the list for which `isDisplayConnected()` returns true
- **THEN** the matrix coefficient SHALL be read from that port

#### Scenario: No ports available
- **WHEN** `getVideoOutputPorts()` returns an empty list
- **THEN** `GetCurrentColorimetry()` SHALL return `COLORIMETRY_UNKNOWN` with `ERROR_NONE`

### Requirement: DeviceSettings exception handling
The DeviceSettings backend `GetCurrentColorimetry()` SHALL wrap all DeviceSettings library calls in a typed `catch (const device::Exception&)` handler. On exception, the method SHALL set `info.colorimetry` to `COLORIMETRY_UNKNOWN` and return `ERROR_NONE`.

#### Scenario: DeviceSettings library throws exception
- **WHEN** a `device::Exception` is thrown during `getMatrixCoefficients()`
- **THEN** `GetCurrentColorimetry()` SHALL set `info.colorimetry` to `COLORIMETRY_UNKNOWN`
- **THEN** `GetCurrentColorimetry()` SHALL return `ERROR_NONE`

### Requirement: Unsupported platform backends return ERROR_UNAVAILABLE
The Linux/DRM and BCM/RPI backend implementations of `GetCurrentColorimetry()` SHALL return `ERROR_UNAVAILABLE` (stub only — no DS matrix coefficient API available on those platforms).

#### Scenario: Linux/DRM backend queried
- **WHEN** the plugin is running on a Linux/DRM platform and `DisplayInfo.getCurrentColorimetry` is called
- **THEN** the property SHALL return `ERROR_UNAVAILABLE`

#### Scenario: RPI backend queried
- **WHEN** the plugin is running on a BCM/RPI platform and `DisplayInfo.getCurrentColorimetry` is called
- **THEN** the property SHALL return `ERROR_UNAVAILABLE`
