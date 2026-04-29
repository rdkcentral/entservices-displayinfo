## Why

The `DisplayInfo` plugin currently exposes HDR type and HDCP protection level through its JSON-RPC surface, but does not expose colorimetry information reported by the connected display's EDID or platform capabilities. Clients (e.g., media pipelines, colour-management subsystems) need colorimetry data to make correct colour-space decisions, and there is no way to retrieve it today without reading raw EDID bytes and parsing them independently.

## What Changes

- Expose a new JSON-RPC property `DisplayInfo.colorimetry` that returns the list of colorimetry modes supported by the connected display (or the built-in panel).
- The property queries `Exchange::IDisplayProperties::Colorimetry()` which is already implemented in the DeviceSettings and RPI backends but has no JSON-RPC binding or spec coverage.
- If no display is connected the property returns an empty array with `ERROR_NONE` (success).
- The Linux/DRM backend returns `ERROR_UNAVAILABLE` for colorimetry (no DRM colorimetry parsing yet) — the JSON-RPC layer maps this to an empty list.

## Capabilities

### New Capabilities

- `displayinfo-colorimetry`: Exposes colorimetry data of the connected/built-in display via the `DisplayInfo.colorimetry` JSON-RPC property. Covers the full read path from EDID/platform → `IDisplayProperties::Colorimetry()` → JSON-RPC response, including the empty-list-on-no-display behaviour.

### Modified Capabilities

- `displayinfo`: The existing `displayinfo` spec gains new coverage entries for `Colorimetry`, `ColorSpace`, `FrameRate`, `ColourDepth`, `QuantizationRange`, and `EOTF` — `IDisplayProperties` methods that are currently orphaned (identified as gap G-01 in the coverage report).

## Impact

- **JSON-RPC surface:** New read-only property `DisplayInfo.1.colorimetry` added; no breaking changes to existing properties.
- **Code:** `plugin/DisplayInfoJsonRpc.cpp` — new `get_colorimetry` handler registered via `JDisplayProperties`; no changes to implementation backends.
- **Specs:** New `openspec/specs/displayinfo-colorimetry/spec.md`; updated `## Covered Code` in `openspec/specs/displayinfo.spec.md`.
- **Tests:** New L1 test cases covering: colorimetry returned when display connected, empty list when disconnected, `ERROR_UNAVAILABLE` from Linux backend treated as empty list.
- **No breaking changes.** Existing clients are unaffected.
