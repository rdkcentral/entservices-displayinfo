## Context

The DisplayInfo plugin provides display and graphics hardware information via a unified API, including a `colorimetry` property that reports the colorimetry capabilities of the display. However, the requirements and implementation of this property have not been clearly defined or consistently enforced. This change clarifies and extends the requirements for the existing `colorimetry` property, ensuring it accurately reflects the colorimetry info from the display’s EDID or platform capabilities, and is properly documented and tested.

## Goals / Non-Goals

**Goals:**
- Clarify and enforce the requirements for the existing `colorimetry` property in the DisplayInfo plugin API.
- Ensure the property returns a list of supported colorimetry standards from the display’s EDID or platform capabilities.
- Return an empty list if no display is connected or if the information is unavailable.
- Document the property and its behavior in the OpenSpec.

**Non-Goals:**
- No addition of new properties; only clarification and extension of the existing property.
- No changes to other display properties or control APIs.
- No direct control over colorimetry settings (read-only).
- No breaking changes to existing consumers.

## Decisions

- The property is read-only and its requirements are clarified and enforced.
- For built-in displays, the property reflects the panel’s colorimetry; for external displays, it reflects the connected TV’s info.
- If no display is connected or the information is unavailable, the property returns an empty list.
- The property is surfaced via JSON-RPC and documented in the OpenSpec.
- The implementation uses existing platform APIs (EDID parsing, DeviceSettings) to retrieve colorimetry info.

## Risks / Trade-offs

- [Risk] Platform APIs may not always provide complete or accurate colorimetry info → Mitigation: Document as best-effort, fallback to empty list if unavailable.
- [Risk] Clients may misinterpret the absence of colorimetry info → Mitigation: Clearly document empty list semantics.

## Migration Plan

- Update the implementation and documentation of the property in a backward-compatible way.
- Notify consumers of the clarified and extended property behavior.
- No migration required for existing clients.

## Open Questions

- Are there edge cases where colorimetry info is ambiguous or conflicting between built-in and external displays?
- Should the property be exposed in other plugin APIs for consistency?
