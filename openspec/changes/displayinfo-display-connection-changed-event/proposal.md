# Proposal

## Why

The generic `updated` interface does not carry a dedicated HDMI connection-state
payload. The existing `DisplayConnectionChanged(bool connected)` notification
makes connection and disconnection explicit without polling.

## What Changes

- Use the generated JSON-RPC event `displayconnectionchanged` whenever
  the HDMI display connection state changes due to connection or disconnection.
- Reuse `IConnectionProperties::INotification::DisplayConnectionChanged(bool connected)`
  and `JConnectionProperties::Event::DisplayConnectionChanged`; do not alter
  the generator's event spelling or add an alias.
- Include a required `connected` boolean in the event payload.
- Add functional requirement REQ-F-13 to the DisplayInfo specification:
  the plugin MUST fire a `DisplayConnectionChanged` event whenever the HDMI
  display connection state changes, exposed as `displayconnectionchanged`
  over JSON-RPC.
- Preserve the existing `updated` event and its source values.
- Cover both transitions, repeated callbacks, unrelated updates, payload
  correctness, and activation/deactivation with focused validation.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `displayinfo`: Add a dedicated HDMI connection-state event contract.
  The existing canonical document is `openspec/specs/displayinfo.spec.md`;
  follow the existing changes' `specs/displayinfo/spec.md` delta convention.
  The CLI does not discover the canonical flat-file layout; this change does
  not relocate or duplicate that document.

## Impact

- `plugin/DisplayInfo.h` and `plugin/DisplayInfo.cpp`: event dispatch and lifecycle.
- `plugin/DeviceSettings/PlatformImplementation.cpp`: the HDMI hot-plug delegate
  now feeds the existing observers through a per-instance FIFO dispatcher.
  Duplicate suppression and lifecycle validation remain pending.
- `plugin/Linux/PlatformImplementation.cpp` and
  `plugin/RPI/PlatformImplementation.cpp`: verify hot-plug detection and
  transition-state delivery through their existing observer paths.
- Nexus implementation is supplied externally at build time; verify its
  HDMI notification contract rather than assume conformance.
- Shared Exchange interfaces/generated bindings: reuse the installed callback
  and its generated payload. No companion interface or generator change is needed.
  Plugin, backends, test mocks, and RPC binaries must use matching updated headers.
- `Tests/L1Tests/tests/test_DisplayInfo.cpp`: focused backend and JSON-RPC tests.
- No new runtime dependency or breaking change to existing endpoints.

## Non-goals

- Do not rename, replace, or alter the existing `updated` event.
- Do not introduce resolution, frame-rate, HDCP, or HDMI-input events.
- Do not add multi-port support, polling, debounce policy, or new configuration.
- Do not migrate the specification layout or repair unrelated configuration,
  Git object-store, or legacy JSON-RPC issues.
