# DisplayInfo Plugin Specification

## Overview

The **DisplayInfo** plugin is a WPEFramework (Thunder) plugin that exposes display and graphics
capabilities of the host device over JSON-RPC. It abstracts four hardware backends
(RDK DeviceSettings, Linux DRM/KMS, Broadcom VideoCore/BCM, and Nexus) behind a single
versioned API surface and fires events when the display connection state changes.

---

## Description

`DisplayInfo` runs as a WPEFramework plugin and optionally in its own out-of-process (OOP)
container. It is responsible for:

- Reporting GPU memory statistics (total and free).
- Reporting the current HDMI/display connection status, resolution, vertical refresh rate, and
  EDID data.
- Reporting and (where the platform permits) setting the preferred HDCP protection level.
- Reporting the active HDR mode and the HDR capabilities of the connected TV and the STB.
- Reporting physical display dimensions (width/height in centimetres).
- Notifying clients of display connection and resolution-change events in near-real-time.

The plugin satisfies the `Platform` precondition and consequently starts only after the platform
subsystem is ready.

---

## Requirements

### Functional

| ID | Requirement |
|----|-------------|
| REQ-F-01 | The plugin MUST expose a `displayinfo` JSON-RPC property (read-only) returning a composite object that contains `totalgpuram`, `freegpuram`, `audiopassthrough`, `connected`, `width`, `height`, `hdcpprotection`, and `hdrtype`. |
| REQ-F-02 | The plugin MUST fire an `updated` event whenever the display connection or resolution state changes. |
| REQ-F-03 | The plugin MUST implement `Exchange::IGraphicsProperties` to report total and free GPU RAM in bytes. |
| REQ-F-04 | The plugin MUST implement `Exchange::IConnectionProperties` to report HDMI connection state, audio passthrough status, display resolution (pixels), vertical refresh frequency, HDCP protection level, and EDID binary data. |
| REQ-F-05 | The plugin MUST implement `Exchange::IHDRProperties` to report the active HDR type and optionally the TV and STB HDR capability lists. |
| REQ-F-06 | The plugin MUST implement `Exchange::IDisplayProperties` to report physical display dimensions (cm) and colorimetry data. |
| REQ-F-07 | The plugin MUST accept an out-of-process execution mode (configured via `root.mode`). |
| REQ-F-08 | `IDisplayProperties` MUST NOT block plugin initialisation: if unavailable at startup, the plugin MUST still initialise successfully and return `ERROR_UNAVAILABLE` on related endpoints. |
| REQ-F-09 | HDCPProtection MUST support both getter and setter semantics on platforms that allow preference configuration (DeviceSettings, BCM/RPI). |
| REQ-F-10 | The plugin MUST register with the `Platform` precondition and MUST NOT activate before that precondition is satisfied. |
| REQ-F-11 | The DeviceSettings backend `Colorimetry()` MUST return an empty `IColorimetryIterator` and `ERROR_NONE` for all failure paths (display not connected, EDID read/verify failure, `device::Exception`). It MUST NOT return `ERROR_GENERAL` to callers. |

### Non-Functional

| ID | Requirement |
|----|-------------|
| REQ-NF-01 | Plugin initialisation MUST NOT block for more than 2 000 ms waiting for the OOP root object. |
| REQ-NF-02 | Event delivery latency for `updated` MUST be less than 500 ms after the hardware signal is received. |
| REQ-NF-03 | The plugin MUST be free of memory leaks on both the happy path and on interface acquisition failures during `Initialize`. |
| REQ-NF-04 | Exception-unsafe calls into platform libraries MUST be wrapped in try/catch with typed exception handlers (no bare `catch(...)`). |
| REQ-NF-05 | The DeviceSettings backend `Colorimetry()` MUST use `std::vector<unsigned char>` (or equivalent RAII container) for EDID byte buffers. Manual `new[]`/`delete[]` heap allocation is prohibited. |

### DeviceSettings Backend Colorimetry Scenarios

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

#### Scenario: Implementation uses vector for EDID buffer
- **WHEN** `Colorimetry()` reads EDID bytes from the display
- **THEN** the implementation SHALL store EDID bytes in a `std::vector<unsigned char>` (not a raw `new unsigned char[]`)

---

## Architecture / Design

### Component Diagram

```
  ┌──────────────────────────────────────────────────────────────┐
  │  WPEFramework / Thunder Host Process                         │
  │                                                              │
  │  ┌────────────────────────────────────────────────────────┐  │
  │  │  Plugin::DisplayInfo                                   │  │
  │  │  (IPlugin + IWeb + JSONRPC)                            │  │
  │  │                                                        │  │
  │  │  JSON-RPC: GET displayinfo  ──────────────────────►    │  │
  │  │  JSON-RPC Event: updated    ◄──────────────────────    │  │
  │  │                                                        │  │
  │  │  Notification (IConnectionProperties::INotification   │  │
  │  │               + RPC::IRemoteConnection::INotification)│  │
  │  └────────────────────┬───────────────────────────────────┘  │
  │                       │ RPC (out-of-process boundary)        │
  └───────────────────────┼──────────────────────────────────────┘
                          │
  ┌───────────────────────▼──────────────────────────────────────┐
  │  OOP Container  (optional – controlled by root.mode)         │
  │                                                              │
  │  ┌─────────────────────────────────────────────────────────┐ │
  │  │  DisplayInfoImplementation                              │ │
  │  │  + IGraphicsProperties   TotalGpuRam, FreeGpuRam       │ │
  │  │  + IConnectionProperties IsAudioPassthrough, Connected,│ │
  │  │                          Width, Height, VerticalFreq,  │ │
  │  │                          HDCPProtection (get/set), EDID│ │
  │  │  + IHDRProperties        HDRSetting, TVCapabilities,   │ │
  │  │                          STBCapabilities               │ │
  │  │  + IDisplayProperties    WidthInCentimeters,           │ │
  │  │                          HeightInCentimeters,          │ │
  │  │                          Colorimetry, EDID             │ │
  │  └──────────────────┬──────────────────────────────────────┘ │
  │                     │ compile-time backend selection         │
  │  ┌──────────────────┴──────────────────────────────────────┐ │
  │  │ DeviceSettings │ Linux/DRM  │  BCM/VideoCore │  Nexus  │ │
  │  └────────────────────────────────────────────────────────┘  │
  └──────────────────────────────────────────────────────────────┘
```

### Lifecycle

```
  Initialize(IShell*)
      │
      ├─ service->Root<IConnectionProperties>() ──► OOP implementation
      │       timeout = 2 000 ms
      │
      ├─ Register INotification
      ├─ IConfiguration::Configure(service)  (platform-specific config)
      │
      ├─ QueryInterface<IGraphicsProperties>
      ├─ QueryInterface<IHDRProperties>         (required)
      └─ QueryInterface<IDisplayProperties>     (optional – failure is non-fatal)

  Deinitialize(IShell*)
      │
      ├─ Unregister notifications
      ├─ Unregister JSON-RPC handlers (JConnectionProperties, JGraphicsProperties,
      │                                JHDRProperties, JDisplayProperties)
      ├─ Release all interface pointers
      └─ connection->Terminate()  (if OOP process)
```

### Platform Backend Selection (CMake)

| Backend | CMake condition | Transport to HW |
|---------|----------------|-----------------|
| DeviceSettings | `USE_DEVICESETTINGS` | libds + libIARMBus (DSMGR) |
| Linux / DRM | `LIBDRM_FOUND` (fallback) | libdrm + udev netlink socket |
| BCM / RPI | `BCM_HOST_FOUND` | Broadcom VideoCore API (`bcm_host`) |
| Nexus (Broadcom set-top) | `NXCLIENT_FOUND && NEXUS_FOUND` | nxclient RPC |

Only one backend is compiled per build target.

### Event Flow

```
  HW interrupt / udev event / DSMGR IARM event
          │
          ▼
  Backend-specific callback
          │
          ▼
  IConnectionProperties::INotification::Updated(Source)
          │
          ▼
  Notification::Updated (in plugin process)
          │
          ▼
  JConnectionProperties::Event::Updated(_parent, source)
          │
          ▼
  JSON-RPC "updated" event delivered to all registered clients
```

---

## External Interfaces

### JSON-RPC Property – `displayinfo`

**Method type:** Property (read-only)  
**Endpoint:** `DisplayInfo.1.displayinfo`

#### Response object (`DisplayinfoData`)

| Field | Type | Description |
|-------|------|-------------|
| `totalgpuram` | `uint64` | Total GPU RAM in bytes |
| `freegpuram` | `uint64` | Free GPU RAM in bytes |
| `audiopassthrough` | `bool` | `true` if audio output is in passthrough mode |
| `connected` | `bool` | `true` if a display is connected on the primary output |
| `width` | `uint32` | Horizontal resolution in pixels (from EDID) |
| `height` | `uint32` | Vertical resolution in pixels (from EDID) |
| `hdcpprotection` | enum | HDCP negotiation level (see below) |
| `hdrtype` | enum | Active HDR mode (see below) |

#### `HdcpprotectionType` enum

| Value | Meaning |
|-------|---------|
| `HDCP_UNENCRYPTED` | No HDCP encryption |
| `HDCP_1X` | HDCP 1.x |
| `HDCP_2X` | HDCP 2.x |
| `HDCP_AUTO` | Platform selects best available |

#### `HdrtypeType` enum

| Value | Meaning |
|-------|---------|
| `HDR_OFF` | SDR (no HDR) |
| `HDR_10` | HDR10 |
| `HDR_10PLUS` | HDR10+ |
| `HDR_HLG` | Hybrid Log-Gamma |
| `HDR_DOLBYVISION` | Dolby Vision |
| `HDR_TECHNICOLOR` | Technicolor |

#### Error codes

| Code | Meaning |
|------|---------|
| `ERROR_NONE` (0) | Success |
| `ERROR_UNAVAILABLE` | Interface not available (e.g. `IDisplayProperties` absent) |
| `ERROR_GENERAL` | Platform library call failed |

---

### JSON-RPC Event – `updated`

**Event name:** `DisplayInfo.1.updated`  
Fired whenever the display connection or resolution changes.

#### Event payload (`Source` enum)

| Value | Trigger |
|-------|---------|
| `PRE_RESOLUTION_CHANGE` | Resolution change is about to happen |
| `POST_RESOLUTION_CHANGE` | Resolution change has completed |
| `HDMI_CHANGE` | HDMI hot-plug state changed |
| `HDCP_CHANGE` | HDCP handshake state changed |

---

### JSON-RPC Property – `isaudiopassthrough`

**Method type:** Property (read-only)  
**Endpoint:** `DisplayInfo.1.isaudiopassthrough`  
**Interface method:** `Exchange::IConnectionProperties::IsAudioPassthrough(bool&)`

Whether the primary audio output is currently in passthrough mode.

| Condition | Value | Return code |
|-----------|-------|-------------|
| Passthrough active (DeviceSettings) | `true` | `ERROR_NONE` |
| Non-passthrough (DeviceSettings) | `false` | `ERROR_NONE` |
| `device::Exception` thrown (DeviceSettings) | `false` | `ERROR_GENERAL` |
| Linux backend | undefined | `ERROR_UNAVAILABLE` |

---

### JSON-RPC Property – `connected`

**Method type:** Property (read-only)  
**Endpoint:** `DisplayInfo.1.connected`  
**Interface method:** `Exchange::IConnectionProperties::Connected(bool&)`

Whether a display is currently connected on the primary video output port.

| Condition | Value | Return code |
|-----------|-------|-------------|
| Display connected (DeviceSettings) | `true` | `ERROR_NONE` |
| Display not connected (DeviceSettings) | `false` | `ERROR_NONE` |
| `device::Exception` thrown (DeviceSettings) | undefined | `ERROR_GENERAL` |
| DRM display handle present (Linux) | `true` or `false` | `ERROR_NONE` |
| No DRM display handle (Linux) | undefined | `ERROR_UNAVAILABLE` |

---

### JSON-RPC Property – `totalgpuram`

**Method type:** Property (read-only)  
**Endpoint:** `DisplayInfo.1.totalgpuram`  
**Interface method:** `Exchange::IGraphicsProperties::TotalGpuRam(uint64_t&)`

Total GPU RAM in bytes.

| Condition | Value | Return code |
|-----------|-------|-------------|
| DeviceSettings | Value from `SoC_GetTotalGpuRam()` | `ERROR_NONE` |
| Linux | Value parsed from `gpuMemoryFile` × `gpuMemoryUnitMultiplier` | `ERROR_NONE` |

> **Note:** Linux backend returns `0` if the stats file is missing or the key is not found, without propagating an error.

---

### JSON-RPC Property – `freegpuram`

**Method type:** Property (read-only)  
**Endpoint:** `DisplayInfo.1.freegpuram`  
**Interface method:** `Exchange::IGraphicsProperties::FreeGpuRam(uint64_t&)`

Free GPU RAM in bytes. Same error-code behaviour as `totalgpuram`.

---

### JSON-RPC Property – `width`

**Method type:** Property (read-only)  
**Endpoint:** `DisplayInfo.1.width`  
**Interface method:** `Exchange::IConnectionProperties::Width(uint32_t&)`

Horizontal display resolution in pixels, parsed from the EDID of the connected display.

| Condition | Value | Return code |
|-----------|-------|-------------|
| Display connected, valid EDID (DeviceSettings) | Resolution width in pixels | `ERROR_NONE` |
| Display not connected or EDID fails (DeviceSettings) | `0` | `ERROR_NONE` |
| DRM display handle present (Linux) | Width from DRM connector | `ERROR_NONE` |
| No DRM display handle (Linux) | undefined | `ERROR_UNAVAILABLE` |

---

### JSON-RPC Property – `height`

**Method type:** Property (read-only)  
**Endpoint:** `DisplayInfo.1.height`  
**Interface method:** `Exchange::IConnectionProperties::Height(uint32_t&)`

Vertical display resolution in pixels, parsed from the EDID of the connected display. Same error-code behaviour as `width`.

---

### JSON-RPC Property – `verticalfreq`

**Method type:** Property (read-only)  
**Endpoint:** `DisplayInfo.1.verticalfreq`  
**Interface method:** `Exchange::IConnectionProperties::VerticalFreq(uint32_t&)`

Vertical refresh frequency in Hz, parsed from the EDID of the connected display.

| Condition | Value | Return code |
|-----------|-------|-------------|
| Display connected, valid EDID (DeviceSettings) | Refresh rate in Hz | `ERROR_NONE` |
| Display not connected (DeviceSettings) | undefined | `ERROR_GENERAL` |
| EDID verification fails (DeviceSettings) | undefined | `ERROR_GENERAL` |
| DRM display handle present (Linux) | Refresh rate from DRM connector | `ERROR_NONE` |
| No DRM display handle (Linux) | undefined | `ERROR_UNAVAILABLE` |

> **Note:** Unlike `width` and `height`, the DeviceSettings backend returns `ERROR_GENERAL` on disconnected and EDID-fail paths.

---

### JSON-RPC Property – `hdcpprotection`

**Method type:** Property (read-write)  
**Endpoint:** `DisplayInfo.1.hdcpprotection`  
**Interface methods:** `Exchange::IConnectionProperties::HDCPProtection(HDCPProtectionType&)` (get) and `Exchange::IConnectionProperties::HDCPProtection(const HDCPProtectionType)` (set)

Current HDCP protection level preference.

#### Get

| Condition | Value | Return code |
|-----------|-------|-------------|
| Port found (DeviceSettings) | `HDCP_1X`, `HDCP_2X`, or `HDCP_AUTO` | `ERROR_NONE` |
| No HDMI port connected (DeviceSettings) | `HDCP_UNENCRYPTED` | `ERROR_NONE` |
| `device::Exception` thrown (DeviceSettings) | last value unchanged | `ERROR_NONE` |
| DRM display handle present (Linux) | HDCP state from DRM | `ERROR_NONE` |
| No DRM display handle (Linux) | undefined | `ERROR_UNAVAILABLE` |

#### Set

| Condition | Return code |
|-----------|-------------|
| Preference applied (DeviceSettings) | `ERROR_NONE` |
| `SetHdmiPreference` fails or exception (DeviceSettings) | `ERROR_NONE` (failure logged only) |
| Linux backend | `ERROR_UNAVAILABLE` |

> **Note:** The setter is best-effort on DeviceSettings — failures are logged but not propagated to the caller.

---

### JSON-RPC Property – `edid`

**Method type:** Property (read-only)  
**Endpoint:** `DisplayInfo.1.edid`  
**Interface method:** `Exchange::IConnectionProperties::EDID(uint16_t& length, uint8_t data[])`

Raw EDID bytes from the connected display. `length` is an in/out parameter: pass in the buffer size, receive the actual bytes written.

| Condition | Data | Return code |
|-----------|------|-------------|
| Display connected (DeviceSettings) | Raw EDID bytes (up to `length` bytes) | `ERROR_NONE` |
| Display not connected (DeviceSettings) | ASCII bytes of `"unknown"` | `ERROR_GENERAL` |
| `device::Exception` thrown (DeviceSettings) | ASCII bytes of `"unknown"` | `ERROR_GENERAL` |
| DRM display handle present (Linux) | Raw EDID bytes | `ERROR_NONE` |
| No DRM display handle or `length == 0` (Linux) | undefined | `ERROR_UNAVAILABLE` |

> **Note:** On DeviceSettings, if not connected the data buffer is populated with ASCII `"unknown"`. Always check the return code before interpreting the buffer.

---

### JSON-RPC Property – `hdrsetting`

**Method type:** Property (read-only)  
**Endpoint:** `DisplayInfo.1.hdrsetting`  
**Interface method:** `Exchange::IHDRProperties::HDRSetting(HDRType&)`

Active HDR mode on the primary output port.

| Condition | Value | Return code |
|-----------|-------|-------------|
| HDR output active, display connected (DeviceSettings) | `HDR_10` | `ERROR_NONE` |
| No HDR or display not connected (DeviceSettings) | `HDR_OFF` | `ERROR_NONE` |
| Linux — reads `hdrLevelFilepath` | `HDR_OFF` / `HDR_10` / `HDR_HLG` / `HDR_10PLUS` / `HDR_DOLBYVISION` | `ERROR_NONE` |

---

### JSON-RPC Property – `tvcapabilities`

**Method type:** Property (read-only)  
**Endpoint:** `DisplayInfo.1.tvcapabilities`  
**Interface method:** `Exchange::IHDRProperties::TVCapabilities(IHDRIterator*&)`

Array of HDR formats supported by the connected TV.

| Condition | Value | Return code |
|-----------|-------|-------------|
| Display connected, capabilities read (DeviceSettings) | Array of `HDRType` values | `ERROR_NONE` |
| Display not connected or any exception (DeviceSettings) | `[HDR_OFF]` | `ERROR_NONE` |
| Linux backend | undefined | `ERROR_UNAVAILABLE` |

---

### JSON-RPC Property – `stbcapabilities`

**Method type:** Property (read-only)  
**Endpoint:** `DisplayInfo.1.stbcapabilities`  
**Interface method:** `Exchange::IHDRProperties::STBCapabilities(IHDRIterator*&)`

Array of HDR formats supported by the STB itself (independent of connected display).

| Condition | Value | Return code |
|-----------|-------|-------------|
| Capabilities read (DeviceSettings) | Array of `HDRType` values | `ERROR_NONE` |
| Any exception (DeviceSettings) | `[HDR_OFF]` | `ERROR_NONE` |
| Linux backend | undefined | `ERROR_UNAVAILABLE` |

---

### JSON-RPC Property – `widthincentimeters`

**Method type:** Property (read-only)  
**Endpoint:** `DisplayInfo.1.widthincentimeters`  
**Interface method:** `Exchange::IDisplayProperties::WidthInCentimeters(uint8_t&)`

Physical width of the connected display in centimetres, from the EDID display size descriptor.

| Condition | Value | Return code |
|-----------|-------|-------------|
| Display connected, EDID has physical size (DeviceSettings) | Physical width in cm | `ERROR_NONE` |
| Display not connected, EDID too short, or exception (DeviceSettings) | `0` | `ERROR_NONE` |
| Linux backend | undefined | `ERROR_UNAVAILABLE` |

---

### JSON-RPC Property – `heightincentimeters`

**Method type:** Property (read-only)  
**Endpoint:** `DisplayInfo.1.heightincentimeters`  
**Interface method:** `Exchange::IDisplayProperties::HeightInCentimeters(uint8_t&)`

Physical height of the connected display in centimetres. Same error-code behaviour as `widthincentimeters`.

---

### JSON-RPC Property – `colorspace`

**Method type:** Property (read-only)  
**Endpoint:** `DisplayInfo.1.colorspace`  
**Interface method:** `Exchange::IDisplayProperties::ColorSpace(ColourSpaceType&)`

Active colour space on the primary video output.

#### `ColourSpaceType` enum

| Value | Meaning |
|-------|---------|
| `FORMAT_UNKNOWN` | Unknown or not queried |
| `FORMAT_OTHER` | Auto / platform-selected |
| `FORMAT_RGB_444` | RGB 4:4:4 |
| `FORMAT_YCBCR_444` | YCbCr 4:4:4 |
| `FORMAT_YCBCR_422` | YCbCr 4:2:2 |
| `FORMAT_YCBCR_420` | YCbCr 4:2:0 |

| Condition | Value | Return code |
|-----------|-------|-------------|
| Display connected (DeviceSettings) | Mapped `ColourSpaceType` | `ERROR_NONE` |
| Display not connected or exception (DeviceSettings) | undefined | `ERROR_GENERAL` |
| Linux / RPI backends | undefined | `ERROR_UNAVAILABLE` |

---

### JSON-RPC Property – `framerate`

**Method type:** Property (read-only)  
**Endpoint:** `DisplayInfo.1.framerate`  
**Interface method:** `Exchange::IDisplayProperties::FrameRate(FrameRateType&)`

Active frame rate of the primary video output.

#### `FrameRateType` enum

| Value | Meaning |
|-------|---------|
| `FRAMERATE_UNKNOWN` | Unknown |
| `FRAMERATE_23_976` | 23.976 Hz |
| `FRAMERATE_24` | 24 Hz |
| `FRAMERATE_25` | 25 Hz |
| `FRAMERATE_29_97` | 29.97 Hz |
| `FRAMERATE_30` | 30 Hz |
| `FRAMERATE_47_952` | 47.952 Hz |
| `FRAMERATE_48` | 48 Hz |
| `FRAMERATE_50` | 50 Hz |
| `FRAMERATE_59_94` | 59.94 Hz |
| `FRAMERATE_60` | 60 Hz |
| `FRAMERATE_119_88` | 119.88 Hz |
| `FRAMERATE_120` | 120 Hz |

| Condition | Value | Return code |
|-----------|-------|-------------|
| Resolution readable (DeviceSettings) | Mapped `FrameRateType` | `ERROR_NONE` |
| `device::Exception` thrown (DeviceSettings) | `FRAMERATE_UNKNOWN` | `ERROR_GENERAL` |
| Linux / RPI backends | undefined | `ERROR_UNAVAILABLE` |

---

### JSON-RPC Property – `colourdepth`

**Method type:** Property (read-only)  
**Endpoint:** `DisplayInfo.1.colourdepth`  
**Interface method:** `Exchange::IDisplayProperties::ColourDepth(ColourDepthType&)`

Active colour depth (bits per channel) on the primary video output.

#### `ColourDepthType` enum

| Value | Meaning |
|-------|---------|
| `COLORDEPTH_UNKNOWN` | 0 bpc or unknown |
| `COLORDEPTH_8_BIT` | 8 bits per channel |
| `COLORDEPTH_10_BIT` | 10 bits per channel |
| `COLORDEPTH_12_BIT` | 12 bits per channel |

| Condition | Value | Return code |
|-----------|-------|-------------|
| Display connected (DeviceSettings) | Mapped `ColourDepthType` | `ERROR_NONE` |
| Display not connected or exception (DeviceSettings) | undefined | `ERROR_GENERAL` |
| Linux / RPI backends | undefined | `ERROR_UNAVAILABLE` |

---

### JSON-RPC Property – `quantizationrange`

**Method type:** Property (read-only)  
**Endpoint:** `DisplayInfo.1.quantizationrange`  
**Interface method:** `Exchange::IDisplayProperties::QuantizationRange(QuantizationRangeType&)`

Active quantization range on the primary video output.

#### `QuantizationRangeType` enum

| Value | Meaning |
|-------|---------|
| `QUANTIZATIONRANGE_UNKNOWN` | Unknown |
| `QUANTIZATIONRANGE_LIMITED` | Limited range (16–235) |
| `QUANTIZATIONRANGE_FULL` | Full range (0–255) |

| Condition | Value | Return code |
|-----------|-------|-------------|
| Display connected (DeviceSettings) | Mapped `QuantizationRangeType` | `ERROR_NONE` |
| Display not connected or exception (DeviceSettings) | undefined | `ERROR_GENERAL` |
| Linux / RPI backends | undefined | `ERROR_UNAVAILABLE` |

---

### JSON-RPC Property – `colorimetry`

**Method type:** Property (read-only)  
**Endpoint:** `DisplayInfo.1.colorimetry`  
**Interface method:** `Exchange::IDisplayProperties::Colorimetry(IColorimetryIterator*&)`

Array of colorimetry modes advertised by the connected display via its EDID CEA-861 extension block.

#### `ColorimetryType` enum

Values parsed from EDID CEA-861 colorimetry metadata bits:
`COLORIMETRY_UNKNOWN`, `COLORIMETRY_XVYCC601`, `COLORIMETRY_XVYCC709`, `COLORIMETRY_SYCC601`,
`COLORIMETRY_OPYCC601`, `COLORIMETRY_OPRGB`, `COLORIMETRY_BT2020YCCBCBRC`, `COLORIMETRY_BT2020RGB_YCBCR`, `COLORIMETRY_OTHER`.

| Condition | Value | Return code |
|-----------|-------|-------------|
| Display connected, valid EDID (DeviceSettings) | Array of `ColorimetryType` values | `ERROR_NONE` |
| Display not connected (DeviceSettings) | empty | `ERROR_GENERAL` |
| EDID verification fails (DeviceSettings) | empty | `ERROR_GENERAL` |
| Linux / RPI backends | undefined | `ERROR_UNAVAILABLE` |

---

### JSON-RPC Property – `eotf`

**Method type:** Property (read-only)  
**Endpoint:** `DisplayInfo.1.eotf`  
**Interface method:** `Exchange::IDisplayProperties::EOTF(EotfType&)`

Electro-Optical Transfer Function in use on the primary video output.

#### `EotfType` enum

| Value | Meaning |
|-------|---------|
| `EOTF_UNKNOWN` | Unknown or SDR |
| `EOTF_OTHER` | Unrecognised HDR standard |
| `EOTF_BT1886` | BT.1886 (SDR gamma) |
| `EOTF_SMPTE_ST_2084` | SMPTE ST 2084 (HDR10 PQ) |
| `EOTF_BT2100` | BT.2100 (HLG) |

| Condition | Value | Return code |
|-----------|-------|-------------|
| Display connected, HDR standard recognised (DeviceSettings) | `EOTF_SMPTE_ST_2084` or `EOTF_BT2100` | `ERROR_NONE` |
| Display connected, SDR or unrecognised (DeviceSettings) | `EOTF_UNKNOWN` | `ERROR_NONE` |
| Display not connected or exception (DeviceSettings) | undefined | `ERROR_GENERAL` |
| Linux / RPI backends | undefined | `ERROR_UNAVAILABLE` |

---

### Configuration Object

Configuration is injected via `IShell::ConfigLine()` and parsed per platform.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `root.mode` | string | `"Off"` | Plugin execution mode (`Off` = in-process, `Local` = OOP) |
| `useBestMode` | bool | – | Request best available display mode at startup |
| `drmDeviceName` | string | – | DRM device node to open (Linux backend) |
| `drmSubsystemPath` | string | – | Path to DRM subsystem / EDID file |
| `hdcpLevelFilepath` | string | – | File path from which HDCP level is read |
| `hdrLevelFilepath` | string | – | File path from which HDR level string is read |
| `gpuMemoryFile` | string | – | File path for GPU memory stats |
| `gpuMemoryFreePattern` | string | – | Key pattern for free GPU memory in stats file |
| `gpuMemoryTotalPattern` | string | – | Key pattern for total GPU memory in stats file |
| `gpuMemoryUnitMultiplier` | uint32 | 1 | Unit multiplier applied to parsed GPU memory values |
| `hdcplevel` | uint | – | Preferred HDCP level override (BCM/RPI) |

---

## Performance

- **OOP startup timeout:** 2 000 ms (`service->Root<>()` call in `Initialize`).
- **GPU memory reads:** Parsed from a flat text file per call (Linux backend); no caching.
  On high-frequency polling, callers should throttle requests.
- **EDID reads:** Involved on every `Width()`, `Height()`, `VerticalFreq()`, and `EDID()` call
  in DeviceSettings backend. Results are not cached — callers should cache if latency is
  critical.
- **Event delivery:** Udev netlink socket (Linux) or IARM callback (DeviceSettings) to
  WPEFramework worker pool. No additional processing thread — bounded latency.

---

## Security

- **IPC boundary:** The OOP container communicates with the in-process plugin exclusively via
  WPEFramework RPC (COM-RPC) — no external network surface.
- **HDCP setter:** HDCPProtection write access is unrestricted at the plugin layer. Deployments
  should use WPEFramework access-control lists (ACL) to limit which callers may invoke the
  setter JSON-RPC path.
- **EDID data:** Raw EDID bytes are returned as-is from the display. Clients must not treat
  EDID data as trusted input without validation.
- **No user input paths:** All JSON-RPC endpoints are read-only properties or getter/setter
  pairs operating on hardware state. There are no free-form string inputs susceptible to
  injection.
- **Exception handling:** All platform library calls MUST be wrapped in typed exception
  handlers (not bare `catch(...)`) to prevent information leakage via unhandled exceptions
  terminating the process.

---

## Versioning & Compatibility

| Version | Notes |
|---------|-------|
| 1.0.6 | Current release. `IDisplayProperties` introduced as optional (non-fatal if absent). |
| 1.0.x | Backward-compatible property additions only. No breaking changes within 1.x. |

- The JSON-RPC interface version is advertised as `DisplayInfo.1`.
- Adding new fields to `DisplayinfoData` is backwards-compatible (clients ignore unknown
  fields).
- Removing or renaming existing fields or enum values is a breaking change requiring a major
  version bump.

---

## Conformance Testing & Validation

### L1 Tests (`Tests/L1Tests/`)

| Test file | Scope |
|-----------|-------|
| `tests/test_DisplayInfo.cpp` | Unit tests for `DisplayInfoImplementation` (DeviceSettings backend) using GTest mocks for `device::Host`, video output ports, EDID parser, IARM, and DRM. |

Test cases:

| Test name | Coverage |
|-----------|----------|
| `Info_AllProperties` | Aggregated `displayinfo` property — all fields |
| `WidthAndHeight` | `Width()` and `Height()` from EDID |
| `VerticalFrequency` | `VerticalFreq()` from EDID |
| `EDID` | Raw EDID byte retrieval |
| `WidthInCentimeters` | Physical width from EDID display size descriptor |
| `HeightInCentimeters` | Physical height from EDID display size descriptor |
| `ColorSpace` | Colour space enum mapping |
| `FrameRate` | Frame rate enum mapping (all supported values) |
| `ColourDepth` | Colour depth enum mapping |
| `QuantizationRange` | Quantization range enum mapping |
| `Colorimetry` | Colorimetry EDID bitmask parsing |
| `GetHDCPProtection` | HDCP getter enum mapping |
| `SetHDCPProtection` | HDCP setter round-trip |
| `EOTF` | EOTF enum mapping (HDR10, HLG, unknown) |
| `TVCapabilities` | TV HDR capability bitmask parsing |
| `STBCapabilities` | STB HDR capability bitmask parsing |
| `Connected_ExceptionHandling` | `Connected()` throws `device::Exception` |
| `IsAudioPassthrough_ExceptionHandling` | `IsAudioPassthrough()` throws `device::Exception` |
| `ColorSpace_ExceptionHandling` | `ColorSpace()` throws `device::Exception` |
| `FrameRate_ExceptionHandling` | `FrameRate()` throws `device::Exception` |
| `ColourDepth_ExceptionHandling` | `ColourDepth()` throws `device::Exception` |
| `QuantizationRange_ExceptionHandling` | `QuantizationRange()` throws `device::Exception` |
| `EOTF_ExceptionHandling` | `EOTF()` throws `device::Exception` |
| `GetHDCPProtection_ExceptionHandling` | `HDCPProtection` getter throws `device::Exception` |
| `SetHDCPProtection_ExceptionHandling` | `HDCPProtection` setter throws `device::Exception` |
| `TVCapabilities_ExceptionHandling` | `TVCapabilities()` throws `device::Exception` |
| `STBCapabilities_ExceptionHandling` | `STBCapabilities()` throws `device::Exception` |
| `EDID_ExceptionHandling` | `EDID()` throws `device::Exception` |
| `ResolutionChange_NotificationTest` | `Updated` event dispatch on resolution change |

### L2 Tests (`Tests/L2Tests/`)

Integration tests validating end-to-end JSON-RPC call flow through the plugin stack
(structure present; test cases TBD per L2 test framework requirements).

### Manual Validation Checklist

- [ ] Plugin activates on a real device after `Platform` is ready.
- [ ] `displayinfo` property returns plausible values for connected and disconnected states.
- [ ] `updated` event fires on HDMI hot-plug.
- [ ] `updated` event fires on resolution change.
- [ ] Plugin deactivates cleanly with no resource leaks (check OOP process exit).

---

## Covered Code

- `plugin/DisplayInfo.cpp`:
    - `DisplayInfo::Initialize`
    - `DisplayInfo::Deinitialize`
    - `DisplayInfo::Information`
    - `DisplayInfo::Inbound`
    - `DisplayInfo::Process`
    - `DisplayInfo::Info`
    - `DisplayInfo::Deactivated`
    - `DisplayInfo::Notification::Updated`
    - `DisplayInfo::Notification::Deactivated`
- `plugin/DisplayInfo.h`:
    - `class DisplayInfo`
    - `class DisplayInfo::Notification`
- `plugin/DisplayInfoJsonRpc.cpp`:
    - `DisplayInfo::RegisterAll`
    - `DisplayInfo::UnregisterAll`
    - `DisplayInfo::get_displayinfo`
    - `DisplayInfo::event_updated`
- `plugin/DisplayInfoTracing.h`:
    - `class HDCPDetailedInfo` (trace/log helper for HDCP events)
- `plugin/Module.h`:
    - Module name definition (`Plugin_DisplayInfo`)
- `plugin/Module.cpp`:
    - `MODULE_NAME_DECLARATION` / `SERVICE_REGISTRATION` macro invocation
- `plugin/DeviceSettings/PlatformImplementation.cpp`:
    - `DisplayInfoImplementation::DisplayInfoImplementation`
    - `DisplayInfoImplementation::~DisplayInfoImplementation`
    - `DisplayInfoImplementation::TotalGpuRam`
    - `DisplayInfoImplementation::FreeGpuRam`
    - `DisplayInfoImplementation::Register`
    - `DisplayInfoImplementation::Unregister`
    - `DisplayInfoImplementation::IsAudioPassthrough`
    - `DisplayInfoImplementation::Connected`
    - `DisplayInfoImplementation::Width`
    - `DisplayInfoImplementation::Height`
    - `DisplayInfoImplementation::VerticalFreq`
    - `DisplayInfoImplementation::HDCPProtection` (getter + setter)
    - `DisplayInfoImplementation::WidthInCentimeters`
    - `DisplayInfoImplementation::HeightInCentimeters`
    - `DisplayInfoImplementation::EDID`
    - `DisplayInfoImplementation::PortName`
    - `DisplayInfoImplementation::HDRSetting`
    - `DisplayInfoImplementation::TVCapabilities`
    - `DisplayInfoImplementation::STBCapabilities`
    - `DisplayInfoImplementation::ColorSpace`
    - `DisplayInfoImplementation::FrameRate`
    - `DisplayInfoImplementation::ColourDepth`
    - `DisplayInfoImplementation::QuantizationRange`
    - `DisplayInfoImplementation::Colorimetry`
    - `DisplayInfoImplementation::EOTF`
    - `DisplayInfoImplementation::ResolutionChangeImpl`
    - `DisplayInfoImplementation::OnResolutionPreChange`
    - `DisplayInfoImplementation::OnResolutionPostChange`
- `plugin/DeviceSettings/SoC_abstraction.h`:
    - `SoC_GetTotalGpuRam`
    - `SoC_GetFreeGpuRam`
    - `SoC_GetGraphicsWidth`
    - `SoC_GetGraphicsHeight`
- `plugin/DeviceSettings/RPI/SoC_abstraction.cpp`:
    - `parseLine` / `getMemInfo` (GPU memory parsing via `/proc/meminfo`)
    - `getGraphicSize` / `getPrimaryPlane` (DRM primary plane resolution)
- `plugin/DeviceSettings/RPI/kms.h`:
    - `kms_ctx` struct (DRM KMS context: connector, encoder, CRTC, plane IDs)
- `plugin/DeviceSettings/RPI/kms.c`:
    - KMS device setup and mode-setting helpers
- `plugin/Linux/DRMConnector.h`:
    - `Linux::DRMConnector::DRMConnector`
    - `Linux::DRMConnector::IsConnected`
    - `Linux::DRMConnector::Width`
    - `Linux::DRMConnector::Height`
    - `Linux::DRMConnector::RefreshRate`
    - `Linux::DRMConnector::InitializeWithConnector`
- `plugin/Linux/PlatformImplementation.cpp`:
    - `UdevObserverType::Register` / `Unregister` / `ReceiveData` (netlink hot-plug)
    - `GraphicsProperties::TotalGpuRam`
    - `GraphicsProperties::FreeGpuRam`
    - `HDRProperties::HDRSetting`
    - `HDRProperties::TVCapabilities`
    - `HDRProperties::STBCapabilities`
    - `HDRProperties::GetHDRLevel`
    - `DisplayProperties::Width` / `Height` / `RefreshRate` / `Connected`
    - `DisplayProperties::HDCP` / `EDID` / `Reauthenticate` / `RequeryProperties`
    - `DisplayInfoImplementation::Configure`
    - `DisplayInfoImplementation::Register` / `Unregister`
    - `DisplayInfoImplementation::IsAudioPassthrough`
    - `DisplayInfoImplementation::Connected`
    - `DisplayInfoImplementation::Width` / `Height` / `VerticalFreq`
    - `DisplayInfoImplementation::EDID`
    - `DisplayInfoImplementation::WidthInCentimeters` / `HeightInCentimeters`
    - `DisplayInfoImplementation::HDCPProtection` (getter; setter returns `ERROR_UNAVAILABLE`)
    - `DisplayInfoImplementation::PortName`
    - `DisplayInfoImplementation::Dispatch`
    - `DisplayInfoImplementation::EventQueue::Worker` (async event dispatch thread)
- `plugin/RPI/PlatformImplementation.cpp`:
    - `DisplayInfoImplementation::Configure`
    - `DisplayInfoImplementation::TotalGpuRam` / `FreeGpuRam`
    - `DisplayInfoImplementation::Register` / `Unregister`
    - `DisplayInfoImplementation::IsAudioPassthrough`
    - `DisplayInfoImplementation::Connected`
    - `DisplayInfoImplementation::Width` / `Height` / `VerticalFreq`
    - `DisplayInfoImplementation::HDCPProtection` (getter + setter)
    - `DisplayInfoImplementation::EDID`
    - `DisplayInfoImplementation::WidthInCentimeters` / `HeightInCentimeters`
    - `DisplayInfoImplementation::PortName`
    - `DisplayInfoImplementation::TVCapabilities` / `STBCapabilities`
    - `DisplayInfoImplementation::HDRSetting`
    - `DisplayInfoImplementation::ColorSpace`
    - `DisplayInfoImplementation::FrameRate`
    - `DisplayInfoImplementation::ColourDepth`
    - `DisplayInfoImplementation::QuantizationRange`
    - `DisplayInfoImplementation::Colorimetry`
    - `DisplayInfoImplementation::EOTF`
- `plugin/DisplayInfo.conf.in`:
    - Plugin configuration template
- `plugin/DisplayInfo.config`:
    - CMake-driven configuration generation
- `Tests/L1Tests/tests/test_DisplayInfo.cpp`:
    - `DisplayInfoTest` (GTest base fixture)
    - `DisplayInfoTestTest` (derived fixture providing live service)
    - `DisplayInfoTestTest::Info_AllProperties`
    - `DisplayInfoTestTest::WidthAndHeight`
    - `DisplayInfoTestTest::VerticalFrequency`
    - `DisplayInfoTestTest::EDID`
    - `DisplayInfoTestTest::WidthInCentimeters`
    - `DisplayInfoTestTest::HeightInCentimeters`
    - `DisplayInfoTestTest::ColorSpace`
    - `DisplayInfoTestTest::FrameRate`
    - `DisplayInfoTestTest::ColourDepth`
    - `DisplayInfoTestTest::QuantizationRange`
    - `DisplayInfoTestTest::Colorimetry`
    - `DisplayInfoTestTest::GetHDCPProtection`
    - `DisplayInfoTestTest::SetHDCPProtection`
    - `DisplayInfoTestTest::EOTF`
    - `DisplayInfoTestTest::TVCapabilities`
    - `DisplayInfoTestTest::STBCapabilities`
    - `DisplayInfoTestTest::Connected_ExceptionHandling`
    - `DisplayInfoTestTest::IsAudioPassthrough_ExceptionHandling`
    - `DisplayInfoTestTest::ColorSpace_ExceptionHandling`
    - `DisplayInfoTestTest::FrameRate_ExceptionHandling`
    - `DisplayInfoTestTest::ColourDepth_ExceptionHandling`
    - `DisplayInfoTestTest::QuantizationRange_ExceptionHandling`
    - `DisplayInfoTestTest::EOTF_ExceptionHandling`
    - `DisplayInfoTestTest::GetHDCPProtection_ExceptionHandling`
    - `DisplayInfoTestTest::SetHDCPProtection_ExceptionHandling`
    - `DisplayInfoTestTest::TVCapabilities_ExceptionHandling`
    - `DisplayInfoTestTest::STBCapabilities_ExceptionHandling`
    - `DisplayInfoTestTest::EDID_ExceptionHandling`
    - `DisplayInfoTestTest::ResolutionChange_NotificationTest`

---

## Open Queries

- **OQ-01:** `TVCapabilities` and `STBCapabilities` in the Linux/DRM backend return
  `ERROR_UNAVAILABLE`. Is there a plan to implement HDR capability discovery via EDID
  extension blocks for that backend?
- **OQ-02:** The `DisplayInfoJsonRpc.cpp` `event_updated()` path calls `Notify("updated")`
  directly (legacy path), while new code uses `JConnectionProperties::Event::Updated`.
  Should `event_updated()` and the associated `RegisterAll/UnregisterAll` be deprecated
  or removed?
- **OQ-03:** GPU memory reads in the Linux backend parse a file on every call with no
  caching. Is a cache-with-TTL strategy planned for high-frequency polling clients?
- **OQ-04:** The Nexus backend source is fetched from an external git repository at build
  time (`GetExternalCode`). Should its interfaces and behaviour be covered by this spec
  or a separate backend-specific spec?

---

## References

- WPEFramework plugin development guide (internal)
- `interfaces/IDisplayInfo.h`, `interfaces/json/JsonData_DisplayInfo.h`
- `interfaces/json/JConnectionProperties.h`, `JGraphicsProperties.h`, `JHDRProperties.h`, `JDisplayProperties.h`
- HDMI HDCP specification (HDCP 1.4 / 2.2 / 2.3)
- EDID standard — VESA EDID v1.4
- RDK DeviceSettings API documentation (internal)

---

## Change History

- 2026-04-29 — Initial spec generated from codebase exploration (v1.0.6).
- 2026-04-29 — Added individual JSON-RPC property subsections for all 20 auto-bound endpoints (`isaudiopassthrough`, `connected`, `totalgpuram`, `freegpuram`, `width`, `height`, `verticalfreq`, `hdcpprotection`, `edid`, `hdrsetting`, `tvcapabilities`, `stbcapabilities`, `widthincentimeters`, `heightincentimeters`, `colorspace`, `framerate`, `colourdepth`, `quantizationrange`, `colorimetry`, `eotf`); per-condition return-code tables and enum value tables added for each. Added `PortName`, `TVCapabilities`, `STBCapabilities`, `ColorSpace`, `FrameRate`, `ColourDepth`, `QuantizationRange`, `Colorimetry`, `EOTF` to Covered Code.
- 2026-04-29 — openspec-templater — Restructured to match spec template; Covered Code expanded via full codebase scan including all 29 L1 test cases; Conformance Testing updated with per-test-case table.
- 2026-04-29 — openspec-templater — Restructured to match spec template; Covered Code expanded via full codebase scan.
- 2026-04-29 — displayinfo-colorimetry change — Added `IDisplayProperties` orphaned methods to Covered Code (closes gap G-01); `Colorimetry` disconnected-path fix documented.
- 2026-04-29 — openspec-sync-specs — Merged ADDED requirements from displayinfo-colorimetry change: REQ-F-11 (Colorimetry error handling), REQ-NF-05 (RAII memory management), DeviceSettings backend Colorimetry scenarios.
