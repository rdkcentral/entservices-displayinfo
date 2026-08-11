## Why

The `DisplayInfo` plugin currently exposes colorimetry capabilities reported by the display EDID (via `DisplayInfo.colorimetry`), but does not expose the **active/current colorimetry** of the HDMI output link. Media pipelines and Firebolt-compliant clients need to query the current colorimetry standard in use on the active video output in order to make correct colour-space decisions at runtime. There is no way to retrieve this single active-colorimetry value without reading DS matrix coefficient data independently.

## What Changes

- Add a new JSON-RPC property `DisplayInfo.getCurrentColorimetry` that returns the single active colorimetry standard of the HDMI output link as a string enum value.
- The property reads `dsDisplayMatrixCoefficients_t` from the DeviceSettings backend via `vPort.getMatrixCoefficients()` and maps it to a `ColorimetryType` enum string.
- If no display is connected or EDID parsing fails, the property returns `"COLORIMETRY_UNKNOWN"`.
- A new method `CurrentColorimetry(ColorimetryType& value)` is added to the `Exchange::IDisplayProperties` interface and auto-bound through `JDisplayProperties`.
- A new implementation is added to the DeviceSettings backend (`PlatformImplementation.cpp`).
- The Linux/DRM and RPI backends return `ERROR_UNAVAILABLE` (stub only — not in scope for this change).

## Capabilities

### New Capabilities

- `displayinfo-get-current-colorimetry`: Exposes the active colorimetry standard of the HDMI output link via the `DisplayInfo.getCurrentColorimetry` JSON-RPC property. Covers the full read path from DeviceSettings `vPort.getMatrixCoefficients()` → `IDisplayProperties::CurrentColorimetry()` → JSON-RPC response, including fallback to `COLORIMETRY_UNKNOWN` when no display is connected.

### Modified Capabilities

- `displayinfo`: The existing `displayinfo` spec gains coverage for the new `CurrentColorimetry` method added to `IDisplayProperties`.

## Impact

- **JSON-RPC surface:** New read-only property `DisplayInfo.1.getCurrentColorimetry` added; no breaking changes to existing properties.
- **Interface:** `Exchange::IDisplayProperties` gains a new virtual method `CurrentColorimetry(ColorimetryType& value)` — this is an interface change in the `ThunderInterfaces` / `entservices-interfaces` repository.
- **Code:** `plugin/DeviceSettings/PlatformImplementation.cpp` — new `CurrentColorimetry()` override using `vPort.getMatrixCoefficients()`; `plugin/Linux/PlatformImplementation.cpp` — stub returning `ERROR_UNAVAILABLE`; `plugin/RPI/PlatformImplementation.cpp` — stub returning `ERROR_UNAVAILABLE`.
- **Specs:** New `openspec/specs/displayinfo-get-current-colorimetry/spec.md`; updated `## Covered Code` in `openspec/specs/displayinfo.spec.md`.
- **Tests:** New L1 test cases covering: active colorimetry returned when display connected (each DS matrix coefficient mapping), `COLORIMETRY_UNKNOWN` when no display connected, `ERROR_UNAVAILABLE` from Linux backend.
- **No breaking changes.** Existing clients are unaffected.
