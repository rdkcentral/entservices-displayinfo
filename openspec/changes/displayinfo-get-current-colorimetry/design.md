## Context

The `DisplayInfo` plugin aggregates four C++ interfaces (`IGraphicsProperties`, `IConnectionProperties`, `IHDRProperties`, `IDisplayProperties`) into a single WPEFramework JSON-RPC service. `IDisplayProperties` already provides the iterator-based `Colorimetry()` method (added in the prior `2026-04-29-displayinfo-colorimetry` change) which returns the **set** of colorimetry modes supported by the display EDID.

However, no property currently exposes the **single active/current colorimetry** standard on the HDMI output link. The DeviceSettings library exposes this via `device::VideoOutputPort::getMatrixCoefficients()`, which returns a `dsDisplayMatrixCoefficients_t` scalar. The Thunder interface `Exchange::IDisplayProperties` does not yet have a `CurrentColorimetry()` method, and no JSON-RPC binding exists for it.

The Firebolt specification defines `DisplayInfo.getCurrentColorimetry` as a property that returns a single `colorimetry` string enum value mapping from DS matrix coefficient to the Firebolt colorimetry standard name. This is distinct from `DisplayInfo.colorimetry` (EDID capability array).

Backend summary:

| Backend | `getMatrixCoefficients()` availability | Behaviour |
|---------|---------------------------------------|-----------|
| DeviceSettings | Available via `vPort.getMatrixCoefficients()` | Returns mapped `ColorimetryType` enum string |
| Linux/DRM | No equivalent DS API | `ERROR_UNAVAILABLE` |
| BCM/RPI | No equivalent DS API | `ERROR_UNAVAILABLE` |

## Goals / Non-Goals

**Goals:**
- Add `CurrentColorimetry(ColorimetryType& value)` virtual method to `Exchange::IDisplayProperties` interface (in the `entservices-interfaces` / `ThunderInterfaces` repository).
- Implement `CurrentColorimetry()` in the DeviceSettings backend using `vPort.getMatrixCoefficients()` with the DS-to-Thunder enum mapping.
- Expose the new method as a read-only JSON-RPC property via `JDisplayProperties` auto-binding (no manual registration needed — auto-generated from interface annotation).
- Return `COLORIMETRY_UNKNOWN` (with `ERROR_NONE`) when no display is connected.
- Add L1 test cases for: each DS matrix coefficient mapping, disconnected display path, and `ERROR_UNAVAILABLE` from Linux/RPI backends.

**Non-Goals:**
- Implementing `CurrentColorimetry()` for the Linux/DRM backend (no DS API available; out of scope).
- Implementing `CurrentColorimetry()` for the BCM/RPI backend beyond a stub.
- Changing the Nexus backend (out of scope and fetched externally).
- Adding notifications or polling for colorimetry changes (no IARM event exists for this; must be polled per spec).
- Modifying the existing `Colorimetry()` iterator-based property.

## Decisions

### D-01: New `CurrentColorimetry()` method in `IDisplayProperties` rather than repurposing `Colorimetry()`

The existing `Colorimetry()` returns an `IColorimetryIterator*` (an array of supported modes from EDID). `getCurrentColorimetry` returns a **single scalar** representing the **active** standard. These are semantically distinct: one is display capability, the other is active output state. A new method `CurrentColorimetry(ColorimetryType& value)` in `IDisplayProperties` provides the correct scalar semantics.

**Alternative considered:** Add an index parameter to `Colorimetry()`. Rejected — the semantics are fundamentally different (capabilities vs. active state) and mixing them would confuse the API.

### D-02: `COLORIMETRY_UNKNOWN` returned (with `ERROR_NONE`) when no display connected

When `vPort.isDisplayConnected()` returns `false`, the method sets `value = COLORIMETRY_UNKNOWN` and returns `ERROR_NONE`. This is consistent with the Firebolt specification which states: "If no display is connected or EDID parsing fails, returns `COLORIMETRY_UNKNOWN`."

**Alternative considered:** Return `ERROR_GENERAL` when no display connected (same as other disconnected paths in DS backend). Rejected — the Firebolt spec is explicit that `COLORIMETRY_UNKNOWN` is the correct "no data" response, not an error. This simplifies client code.

### D-03: Auto-binding via `JDisplayProperties::Register` — no manual registration

Adding `@property` annotation to `CurrentColorimetry()` in the `IDisplayProperties` IDL causes the Thunder code generator to include it in the auto-generated `JDisplayProperties.h`. Once included, `Exchange::JDisplayProperties::Register(*this, _displayProperties)` in `DisplayInfo::Initialize` will automatically expose the property as `DisplayInfo.1.getCurrentColorimetry`. No changes to `DisplayInfoJsonRpc.cpp` are needed.

**Alternative considered:** Manual registration in `DisplayInfoJsonRpc.cpp` with a custom `get_getCurrentColorimetry` handler. Rejected — the auto-binding approach is consistent with all other `IDisplayProperties` methods and avoids duplicated registration code.

### D-04: DS enum mapping at the interface boundary (in DeviceSettings backend)

The mapping from `dsDisplayMatrixCoefficients_t` to `Exchange::IDisplayProperties::ColorimetryType` is performed inside `DisplayInfoImplementation::CurrentColorimetry()` in `PlatformImplementation.cpp`. The existing `ColorimetryType` enum already covers all required values (`COLORIMETRY_BT709`, `COLORIMETRY_SMPTE170M`, `COLORIMETRY_XVYCC709`, `COLORIMETRY_XVYCC601`, `COLORIMETRY_BT2020RGB_YCBCR`, `COLORIMETRY_BT2020YCCBCBRC`, `COLORIMETRY_UNKNOWN`). No new enum values need to be added to the interface.

**Mapping table:**

| `dsDisplayMatrixCoefficients_t` | `ColorimetryType` |
|---------------------------------|-------------------|
| `dsDISPLAY_MATRIXCOEFFICIENT_BT_709` | `COLORIMETRY_BT709` |
| `dsDISPLAY_MATRIXCOEFFICIENT_SMPTE_170M` | `COLORIMETRY_SMPTE170M` |
| `dsDISPLAY_MATRIXCOEFFICIENT_XvYCC_709` | `COLORIMETRY_XVYCC709` |
| `dsDISPLAY_MATRIXCOEFFICIENT_eXvYCC_601` | `COLORIMETRY_XVYCC601` |
| `dsDISPLAY_MATRIXCOEFFICIENT_BT_2020_NCL` | `COLORIMETRY_BT2020RGB_YCBCR` |
| `dsDISPLAY_MATRIXCOEFFICIENT_BT_2020_CL` | `COLORIMETRY_BT2020YCCBCBRC` |
| All other / unknown values | `COLORIMETRY_UNKNOWN` |

## Risks / Trade-offs

| Risk | Mitigation |
|------|-----------|
| `IDisplayProperties` is an external interface — the interface change must land in `entservices-interfaces` before the plugin can be built against it. | Tasks ordered: interface change first (Task 1), then backend implementation (Task 2). A build-time stub can be used during development. |
| `vPort.getMatrixCoefficients()` may not be available on all DS platform versions. | Wrap the call in `try/catch (const device::Exception&)` with fallback to `COLORIMETRY_UNKNOWN` + `ERROR_NONE`. |
| `JDisplayProperties` is auto-generated — adding a new method requires regenerating `JDisplayProperties.h`. | Document in tasks that the code generator must be run against the updated IDL; the generated header is committed with the plugin. |
| The `NF_COLORIMETRY_SYCC601`, `NF_COLORIMETRY_OPYCC601`, and `NF_COLORIMETRY_OPRGB` Firebolt values have no DS matrix coefficient mapping. | These values cannot be returned by this property. Document as a known limitation in the spec. |

## Open Questions

- OQ-01: Does the version of `rdk-halif-device_settings` available in this build environment expose `getMatrixCoefficients()` on `device::VideoOutputPort`? Verify against the DS HAL header before implementation.
- OQ-02: Does the existing `ColorimetryType` enum in `IDisplayProperties` include all required values (`COLORIMETRY_BT709`, `COLORIMETRY_SMPTE170M`, `COLORIMETRY_XVYCC709`, `COLORIMETRY_XVYCC601`, `COLORIMETRY_BT2020RGB_YCBCR`, `COLORIMETRY_BT2020YCCBCBRC`, `COLORIMETRY_UNKNOWN`)? Verify against `interfaces/IDisplayInfo.h` before adding new interface method.
