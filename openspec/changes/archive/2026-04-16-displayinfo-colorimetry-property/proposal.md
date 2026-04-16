## Why

The DisplayInfo plugin currently lacks a dedicated property for exposing the colorimetry capabilities of the connected or built-in display. This information is essential for advanced video applications and diagnostics, as colorimetry impacts color accuracy and compatibility with various content types. Adding this property aligns the API with platform capabilities and industry standards.

## What Changes


- Clarify and document the requirements for the existing `colorimetry` property in the DisplayInfo plugin API.
- Ensure the property returns a list of supported colorimetry standards as reported by the display’s EDID or platform capabilities.
- For built-in displays, it reflects the panel's colorimetry info; for external displays, it reflects the connected TV's info.
- If no display is connected, the property returns an empty list.
- Documentation/specs updated to describe the clarified property and its behavior.

## Capabilities

### Modified Capabilities
- `displayinfo`: The requirements for the DisplayInfo plugin are clarified and extended for the `colorimetry` property.

## Impact

- Affects the DisplayInfo plugin (C++ backend and JSON-RPC API)
- Updates to the DisplayInfo OpenSpec documentation/spec
- May impact clients or plugins that consume display capability information
- No breaking changes; property is additive and optional
