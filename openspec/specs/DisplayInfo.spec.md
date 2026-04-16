

# DisplayInfo Plugin Spec

## Overview
The DisplayInfo plugin provides a unified, event-driven interface for querying display and graphics hardware information on RDK-based devices. It abstracts platform-specific details and exposes display status, resolution, EDID, and related properties via JSON-RPC.

## Key Interfaces
- **JSON-RPC API**: Main interface for querying display information and subscribing to events.
- **Platform Abstraction Layer**: DeviceSettings, Linux DRM, and RPI BCM implementations.

## Architectural Sketch
```
┌──────────────────────────────┐
│   User / UI / Client        │
└─────────────▲────────────────┘
              │  JSON-RPC
┌─────────────┴───────────────┐
│   DisplayInfo Plugin        │
│   ├─ API Layer              │
│   ├─ Core Logic             │
│   └─ Event Notifications    │
└─────────────▲───────────────┘
              │
┌─────────────┴───────────────┐
│ Platform Abstraction Layer  │
│   ├─ DeviceSettings Impl    │
│   ├─ Linux DRM Impl         │
│   └─ RPI BCM Impl           │
└─────────────▲───────────────┘
              │
┌─────────────┴───────────────┐
│   Hardware / HAL            │
└─────────────────────────────┘
```

## Extensibility
- Platform abstraction allows support for new hardware backends.
- JSON-RPC API can be extended with new properties or events.

## Integration Points
- Consumed by other plugins, external clients, and UI components.
- Integrates with platform-specific libraries (DS, DRM, BCM).

## Unknowns & Gaps
- No explicit error handling or security model documented.
- Platform-specific quirks (e.g., EDID parsing) may not be fully abstracted.

## Event Types & Error Handling
### Event Types
- `updated`: Notifies clients when display connection properties change (e.g., HDMI hotplug, resolution change).

### Error Handling
- Errors are surfaced via JSON-RPC error responses.
- Platform or hardware failures may result in partial or missing data.

## Configuration Mapping
Reads settings from config files/environment variables:
- Device names, file paths for EDID, HDCP, HDR, GPU memory, etc.


## API Surface
### Property: `displayinfo`
- **Description:** Returns general display and connection information
- **Method:** GET
- **Response Fields:**
  - `Width` (uint32): Display width in pixels
  - `Height` (uint32): Display height in pixels
  - `Connected` (bool): HDMI connection status
  - `Audiopassthrough` (bool): Audio passthrough enabled
  - `Totalgpuram` (uint64): Total GPU RAM (bytes)
  - `Freegpuram` (uint64): Free GPU RAM (bytes)
  - `Hdcpprotection` (enum): HDCP protection level (e.g., HDCP_1X, HDCP_2X, HDCP_AUTO)
  - `Hdrtype` (enum): Current HDR type/status (e.g., HDR_OFF, HDR_10, HDR_10PLUS, HDR_HLG, HDR_DOLBYVISION, HDR_TECHNICOLOR, HDR_SDR)
  - `VerticalFreq` (uint32): Display refresh rate (Hz)
  - `WidthInCentimeters` (uint8): Physical display width (cm)
  - `HeightInCentimeters` (uint8): Physical display height (cm)
  - `EDID` (string): Raw EDID data (base64-encoded)
  - `ColorSpace` (enum): Color space (e.g., RGB, YCbCr444, YCbCr422, YCbCr420)
  - `FrameRate` (enum): Frame rate (e.g., 24, 25, 30, 60, etc.)
  - `ColourDepth` (enum): Color depth (e.g., 8, 10, 12 bit)
  - `QuantizationRange` (enum): Quantization range (limited, full, unknown)
  - `Colorimetry` (array<enum>): Supported colorimetry standards (e.g., XVYCC601, XVYCC709, SYCC601, OPYCC601, OPRGB, BT2020YCCBCBRC, BT2020RGB_YCBCR, DCI_P3, UNKNOWN)
  - `EOTF` (enum): Electro-Optical Transfer Function in use (e.g., EOTF_SMPTE_ST_2084 for HDR10, EOTF_BT2100 for HLG, EOTF_UNKNOWN)
  - `TVCapabilities` (array<enum>): HDR formats supported by the connected TV (e.g., HDR_10, HDR_10PLUS, HDR_HLG, HDR_DOLBYVISION, HDR_TECHNICOLOR, HDR_SDR, HDR_OFF)
  - `STBCapabilities` (array<enum>): HDR formats supported by the STB (same enum as TVCapabilities)
  - `HDRSetting` (enum): Current HDR format in use (e.g., HDR_10, HDR_OFF)
  - (Optional) Other platform-specific fields

## Platform Abstraction
- DeviceSettings (C++: DeviceSettings/PlatformImplementation.cpp, uses RDK/DS library, IARM Bus)
- Linux DRM/KMS
- RPI BCM (Broadcom VideoCore)

## Non-Goals
- No direct control over display settings (read-only)
- No explicit performance, security, or extensibility guarantees
