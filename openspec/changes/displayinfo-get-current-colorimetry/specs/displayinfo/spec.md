## ADDED Requirements

### Requirement: IDisplayProperties GetCurrentColorimetry method
The `Exchange::IDisplayProperties` interface SHALL include a `GetCurrentColorimetry(ColorimetryTypeInfo& info)` virtual method annotated as a `@property` (read-only), which returns the active colorimetry standard of the HDMI output link as a `ColorimetryTypeInfo` struct with a `colorimetry` field of type `ColorimetryType`.

#### Scenario: Interface method declared and bound
- **WHEN** `Exchange::JDisplayProperties::Register(*this, _displayProperties)` is called during plugin initialisation
- **THEN** the `DisplayInfo.1.getCurrentColorimetry` JSON-RPC property SHALL be registered and accessible to clients
- **THEN** the `info.colorimetry` field SHALL be serialised as the `colorimetry` key in the JSON-RPC response
