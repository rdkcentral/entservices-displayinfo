## MODIFIED Requirements

### Requirement: DisplayInfo plugin exposes colorimetry property
The DisplayInfo plugin SHALL provide a `colorimetry` property that returns a list of supported colorimetry standards as reported by the display’s EDID or platform capabilities. For built-in displays, this reflects the panel’s colorimetry; for external displays, it reflects the connected TV’s info. If no display is connected, the property SHALL return an empty list.

#### Scenario: Built-in display present
- **WHEN** the device has a built-in display panel
- **THEN** the `colorimetry` property returns the colorimetry info of the built-in panel

#### Scenario: External display connected
- **WHEN** an external display (e.g., TV) is connected
- **THEN** the `colorimetry` property returns the colorimetry info of the connected display

#### Scenario: No display connected
- **WHEN** no display is connected
- **THEN** the `colorimetry` property returns an empty list

#### Scenario: Platform cannot determine colorimetry
- **WHEN** platform APIs cannot determine colorimetry info
- **THEN** the `colorimetry` property returns an empty list
