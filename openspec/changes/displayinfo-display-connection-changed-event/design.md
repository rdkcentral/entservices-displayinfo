# Design

## Context

See [proposal](proposal.md) for motivation and
[spec delta](specs/displayinfo/spec.md) for the behavioral contract.

`DisplayInfo::Notification::Updated` currently forwards every source through
the generated `JConnectionProperties::Event::Updated` helper. Its callback
contains a source enum, not a connection-state boolean.

The installed `IConnectionProperties::INotification` also already provides
`DisplayConnectionChanged(const bool connected)`. The plugin forwards this
through `JConnectionProperties::Event::DisplayConnectionChanged`, whose generated
wire event is `displayconnectionchanged` and whose JSON payload key is `connected`.

The DeviceSettings implementation now has a COM-RPC HDMI hot-plug delegate
alongside its resolution delegate. Linux and RPI have existing `HDMI_CHANGE`
paths; their dedicated connection-callback integration remains pending.
Nexus source is supplied externally.
The legacy `DisplayInfoJsonRpc.cpp` is not part of the plugin target; do not
implement the event in that unused path.

Shared Exchange interfaces and generated bindings are supplied by dependencies
and are not owned by this checkout. Evidence here is the locally updated recipe
sysroot, not a verified upstream release: build all consumers and RPC bindings
against matching updated headers.

## Goals / Non-Goals

**Goals:**
- Deliver the user-confirmed boolean payload through the active plugin path.
- Preserve connection/disconnection ordering and established `updated` behavior.
- Use backend event subscriptions, bounded work, and lifecycle-safe observers.

**Non-Goals:**
- No polling, new backend-selection option, or legacy-path cleanup.
- No additional Exchange interface, interface-ID change, or generated-binding edit.
- No internal-panel or HDMI-input connection reporting.

## Decisions

### 1. Add a distinct, case-sensitive JSON-RPC event

Reuse `JConnectionProperties::Event::DisplayConnectionChanged` to emit
`displayconnectionchanged` with a typed JSON object containing `connected`.
The mixed-case name identifies the C++ callback/helper, not the wire event.
Preserve the generator's default spelling and the existing `Updated` helper
call. Do not hand-register a differently spelled event or introduce an alias.

### 2. Capture state at the hardware notification boundary

Use the connection boolean carried by the backend's authoritative hot-plug
callback. If its hardware API supplies only a signal, query the connection state
synchronously at that boundary, check the result, then queue the captured
boolean. Preserve FIFO ordering rather than re-querying live state later.

Maintain a known/unknown previous-state marker per primary HDMI output under
the existing appropriate synchronization. Capture the initial baseline during
backend setup without introducing waits or an activation event. Suppress only
repeated observations of the same state, not real successive transitions.
Do not hold the state lock while invoking external observers.

Querying `Connected()` only in `Notification::Updated` was rejected: a rapid
connect/disconnect can make both delayed notifications read the final state.
A failed query must be logged explicitly, must not become a false
disconnection, and must not overwrite the last valid baseline. If no baseline
was obtainable, the first authoritative hot-plug callback is still delivered.

### 3. Reuse the installed connection notification contract

Deliver the captured boolean through
`IConnectionProperties::INotification::DisplayConnectionChanged(bool connected)`,
using the existing `Register(INotification*)` and `Unregister(INotification*)`
methods and observer ownership. No companion observer is necessary.

DeviceSettings supplies
`IDeviceSettingsDisplay::IDisplayHDMIHotPlugNotification::OnDisplayHDMIHotPlug(DisplayEvent)`.
The backend registers this delegate with
`IDeviceSettingsDisplay::Register("DisplayInfo", delegate)` and unregisters it
before closing its DeviceSettings link. Connected/disconnected enum values
capture the transition before worker-pool delivery.

`dispatchEvent` queues these values per implementation and submits only one
active `DispatchJob` for that queue. `DispatchDisplayEvents` drains it FIFO
and calls `DisplayConnectionChangedImpl` on the job's owning instance, not
the global `_instance`. Resolution dispatch remains unchanged.

The installed callback is pure virtual, so all observer implementations,
including test helpers, need its override. The two existing L1 helpers have
been updated. Matching COM-RPC binaries are still required; a header rename
or syntax check alone does not prove an OOP round-trip or binary compatibility
with older dependencies. Keep that validation task open.

### 4. Integrate each backend's hot-plug path, not resolution callbacks

- DeviceSettings: use the dedicated `DSDisplayHDMIHotPlugNotification` delegate,
  not resolution callbacks. Its registration and FIFO delivery are implemented;
  duplicate suppression and runtime lifecycle checks remain pending.
- Linux: capture the state when connector properties are refreshed and
  preserve it through the existing asynchronous dispatch path.
- RPI: use the existing display callback and connection status; filter callbacks
  that do not represent a connection-state transition.
- Nexus: verify the externally supplied implementation's contract against the
  same tests. Coordinate an upstream backend update if it lacks the required
  state-bearing callback; do not add a guessed local implementation.

Use existing registration/release patterns. On teardown, unregister hardware
and plugin observers and drain or cancel queued work before releasing state.
Reactivation establishes a new baseline with no stale event delivery.

## Risks / Trade-offs

- State-only signal or delayed callback -> Capture the boolean before queueing;
  test queued connection/disconnection, not only direct callback invocation.
- Mismatched dependency binaries -> Rebuild against the installed updated
  callback contract and verify COM-RPC round-trips; no new interface is proposed.
- Duplicate hardware signals -> Compare known states without time-based debounce.
- Startup/teardown race -> Establish baseline with registration synchronized
  against callbacks and unregister before releasing resources.
- External Nexus implementation -> Require backend validation before declaring
  support; record any upstream prerequisite explicitly.
- Canonical flat-file spec layout -> Update `openspec/specs/displayinfo.spec.md`
  explicitly when implementing; do not blindly archive into a new parallel
  `openspec/specs/displayinfo/spec.md`.

## Migration Plan

1. Verify matching builds of the existing state-bearing Exchange contract.
2. Deploy the plugin and enabled backend implementations with those dependencies.
3. Existing clients continue using `updated`; new clients subscribe to
   `displayconnectionchanged`.
4. Rollback restores the previous plugin/backend dependency set; existing
   endpoints remain usable, while the new event is no longer available.

## Open Questions

_No open questions. Runtime validation and other-backend integration remain
tracked tasks; completed source changes must not be mistaken for full conformance._

## Implementation Evidence

- ARM GCC 11.3 syntax checks for `PlatformImplementation.cpp` and
  `DisplayInfo.cpp` passed with the existing C++11, `-Wall`, and `-Werror` flags.
- The two existing L1 observers now implement the required callback.
- FIFO and owning-instance regression tests were added, but were not run:
  the supplied target sysroot lacks `gtest/gtest.h`.
- Strict OpenSpec validation is blocked by pre-existing invalid YAML in
  `openspec/config.yaml`; this change does not repair that unrelated file.
