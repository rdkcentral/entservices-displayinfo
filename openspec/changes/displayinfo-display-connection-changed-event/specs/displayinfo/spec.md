# DisplayInfo - Display Connection Changed Event

## Overview

Add a dedicated HDMI display connection-state event to the existing DisplayInfo
API without replacing the generic `updated` event.

## Description

Subscribed clients receive `DisplayConnectionChanged` when a display is
connected to or disconnected from the primary HDMI output. The required
`connected` boolean describes the new state associated with that transition.
`DisplayConnectionChanged` is the C++ notification/helper name; the generated
JSON-RPC event name is `displayconnectionchanged` and MUST remain unchanged.
This delta targets the canonical
[DisplayInfo specification](../../../../specs/displayinfo.spec.md).

## Requirements

The following functional requirement is added; existing requirements remain
unchanged.

## ADDED Requirements

### Requirement: REQ-F-13 DisplayConnectionChanged on HDMI connection state changes

The plugin MUST fire a `DisplayConnectionChanged` notification, exposed as the
generated JSON-RPC event `displayconnectionchanged`, whenever the HDMI
display connection state changes due to a display being connected or
disconnected. The event MUST be delivered to subscribed clients and MUST NOT
be emitted for a callback that does not change the HDMI connection state.

#### Scenario: Display connected
- **GIVEN** the primary HDMI output is disconnected and a client is subscribed
- **WHEN** a display is connected to that output
- **THEN** the client MUST receive a `DisplayConnectionChanged` event

#### Scenario: Display disconnected
- **GIVEN** the primary HDMI output is connected and a client is subscribed
- **WHEN** the display is disconnected from that output
- **THEN** the client MUST receive a `DisplayConnectionChanged` event

#### Scenario: Repeated notification without a transition
- **GIVEN** the primary HDMI connection state has already been reported
- **WHEN** another hardware callback reports the same connection state
- **THEN** the plugin MUST NOT emit another `DisplayConnectionChanged` event

#### Scenario: Unrelated update
- **WHEN** resolution, frame rate, or HDCP changes without an HDMI connection transition
- **THEN** the plugin MUST NOT emit `DisplayConnectionChanged`

#### Scenario: Startup without a connection transition
- **WHEN** the plugin activates and no hardware callback has been received
- **THEN** the plugin MUST NOT emit `DisplayConnectionChanged` merely for activation
- **AND** the connection state is treated as unknown; the plugin does not query the initial state
- **AND** the first hardware callback after activation or reactivation is reported, because there is no earlier reported state to compare it with

### Requirement: DisplayConnectionChanged carries the new connection state

The JSON-RPC event MUST use the exact case-sensitive name
`displayconnectionchanged`. Its payload MUST contain a required boolean
`connected`, set to `true` for connection and `false` for disconnection.
Each payload MUST describe its associated transition, not a later state.
An unavailable connection state MUST NOT be represented as `false`.

#### Scenario: Connection payload
- **WHEN** a display connection transition is reported
- **THEN** `DisplayInfo.1.displayconnectionchanged` MUST carry `{"connected":true}`

#### Scenario: Disconnection payload
- **WHEN** a display disconnection transition is reported
- **THEN** `DisplayInfo.1.displayconnectionchanged` MUST carry `{"connected":false}`

#### Scenario: Successive transitions
- **GIVEN** a client is subscribed and the primary HDMI output is disconnected
- **WHEN** connection and disconnection occur before notification delivery completes
- **THEN** the client MUST receive both events in transition order, first with
  `connected` equal to `true`, then with `connected` equal to `false`

### Requirement: Preserve existing DisplayInfo event compatibility

The plugin MUST preserve the existing `updated` event name, payload, and
source values. Adding `DisplayConnectionChanged` MUST NOT remove existing
HDMI or resolution notifications or change existing property contracts.

#### Scenario: Existing HDMI subscriber
- **GIVEN** a client subscribes only to `updated`
- **WHEN** the existing HDMI notification path reports a connection-state change
- **THEN** the client MUST continue to receive `updated` with source `HDMI_CHANGE`

#### Scenario: Existing resolution subscriber
- **GIVEN** a client subscribes to `updated`
- **WHEN** the display resolution changes without an HDMI connection-state change
- **THEN** the existing resolution notifications MUST remain unchanged
- **AND** no `DisplayConnectionChanged` event MUST be emitted

## Architecture / Design

Reuse hardware-driven connection notifications and existing JSON-RPC
subscriptions. See [design](../../design.md) for backend integration and
transition-state handling.

## External Interfaces

**Event:** `DisplayInfo.1.displayconnectionchanged`  
**Subscription name:** `displayconnectionchanged`  
**C++ notification:** `DisplayConnectionChanged(bool connected)`  
**Payload:** `{"connected":true}` or `{"connected":false}`

| Field | Type | Meaning |
|-------|------|---------|
| `connected` | boolean, required | New primary HDMI output connection state |

No new client-callable method or configuration parameter is introduced.
This event concerns HDMI output displays, not HDMI input sources or an
always-present internal panel.

## Performance

Use hardware-driven delivery without adding periodic polling or startup
waiting. Existing `updated` delivery requirements remain unchanged.

## Security

Use the existing JSON-RPC subscription and access-control mechanisms.
The payload contains only a connection-state boolean, not EDID or identifiers.

## Versioning & Compatibility

Additive event within `DisplayInfo.1`; existing subscribers need no migration.
The lowercase wire spelling is generated by the existing tool and is part
of the API contract. The C++ callback remains mixed-case. Matching updated
interface headers and COM-RPC binaries are required; compatibility with older
binaries is not inferred from the additive JSON-RPC event.

## Conformance Testing & Validation

- Assert the exact event name and required boolean payload on both transitions.
- Assert no extra connection event on repeated callbacks or unrelated updates.
- Exercise a connect/disconnect sequence queued before delivery.
- Verify unchanged `updated` payloads and resolution notifications.
- Verify activation does not emit a synthetic transition and deactivation
  stops delivery; reactivation establishes a fresh baseline.
- Validate hardware hot-plug on each enabled backend, including the
  externally supplied Nexus implementation before claiming its conformance.

## Covered Code

The repository uses `plugin/`, not `src/` and `include/`; relevant integration
points for implementation are:

- [plugin/DisplayInfo.h](../../../../../plugin/DisplayInfo.h):
  - `DisplayInfo::Notification::Updated`
  - `DisplayInfo::Notification::DisplayConnectionChanged`
- [plugin/DisplayInfo.cpp](../../../../../plugin/DisplayInfo.cpp):
  - `DisplayInfo::Initialize`, `DisplayInfo::Deinitialize`
- [plugin/DeviceSettings/PlatformImplementation.cpp](../../../../../plugin/DeviceSettings/PlatformImplementation.cpp):
  - `DisplayInfoImplementation::DSVideoPortNotification`
  - `DisplayInfoImplementation::DSDisplayHDMIHotPlugNotification::OnDisplayHDMIHotPlug`
  - `DisplayInfoImplementation::dispatchEvent`, `DisplayInfoImplementation::DispatchDisplayEvents`
  - `DisplayInfoImplementation::DisplayConnectionChangedImpl`
  - `DisplayInfoImplementation::ResolutionChangeImpl`
  - `DisplayInfoImplementation::Register`, `DisplayInfoImplementation::Unregister`
- [plugin/Linux/PlatformImplementation.cpp](../../../../../plugin/Linux/PlatformImplementation.cpp):
  - `DisplayInfoImplementation::Dispatch`
  - `DisplayInfoImplementation::EventQueue::Worker`
- [plugin/RPI/PlatformImplementation.cpp](../../../../../plugin/RPI/PlatformImplementation.cpp):
  - `DisplayInfoImplementation::Register`, `DisplayInfoImplementation::Unregister`
- [Tests/L1Tests/tests/test_DisplayInfo.cpp](../../../../../Tests/L1Tests/tests/test_DisplayInfo.cpp):
  - `DisplayConnectionChanged_QueuedTransitionsInOrder` (added, not run)
  - `DisplayConnectionChanged_DispatchesOnOwningInstance` (added, not run)
  - Existing resolution notification helpers (new callback override added)

## Open Queries

_No open queries._

## References

- [Canonical DisplayInfo specification](../../../../specs/displayinfo.spec.md)
- [Proposal](../../proposal.md)
- [Design](../../design.md)

## Change History

- 2026-11-20 - Proposed REQ-F-13 and the dedicated HDMI connection event
  with a user-confirmed `connected` boolean payload.
- 2026-11-20 - Reconciled the existing callback and generated lowercase
  `displayconnectionchanged` wire event; recorded implemented FIFO delivery
  and outstanding runtime validation without altering generated bindings.
