# Tasks

References point to [the spec delta](specs/displayinfo/spec.md) and
[the design](design.md). This checklist separates completed source work from
runtime validation so that existing changes are not implemented twice.
Preserve the generated `displayconnectionchanged` event and `connected`
payload; reuse the installed callback without introducing a companion interface.

## 1. Verify connection notification dependencies

- [x] 1.1 Inspect and document the installed `DisplayConnectionChanged(bool connected)` callback, existing observer registration, DeviceSettings hot-plug callback, and generated lowercase event/boolean payload. Verified by reading the locally updated sysroot headers and matching compiled plugin call sites; no upstream release or binary compatibility is assumed. Spec: External Interfaces and payload requirement.
- [ ] 1.2 Verify in-process and OOP round-trips with both boolean values using matching updated API headers, mocks, and RPC binaries; record the tested dependency revisions. Spec: payload requirement and Versioning & Compatibility.

## 2. Implement backend hot-plug transition delivery

Depends on group 1.

- [x] 2.1 Wire DeviceSettings hot-plug states into a per-instance FIFO with one active worker and owner-based notification dispatch. Add queued-order and instance-isolation regression cases and implement the required callback in both existing L1 observers. Verified source delivery/registration paths and passing ARM C++11 `-Wall -Werror` syntax checks; test execution remains in 2.2/4.1. Spec: REQ-F-13 and successive transitions.
- [ ] 2.2 Add DeviceSettings duplicate-state suppression and startup-baseline handling without polling; extend and run the connection tests for both transitions, duplicate callbacks, unrelated updates, FIFO ordering, and instance isolation. Update Covered Code for the final methods. Spec: REQ-F-13 and startup/unrelated-update scenarios.
- [ ] 2.3 Integrate Linux's captured state through its connector-refresh/event-queue path; add/run backend tests for queued transition ordering and unchanged `updated` delivery, and document actual integration methods. Spec: successive transitions and compatibility.
- [ ] 2.4 Integrate RPI's display callback with state-bearing notifications and duplicate suppression; verify connect, disconnect, and non-connection callbacks with backend tests or target evidence, and document the tested path. Spec: REQ-F-13 and payload scenarios.
- [ ] 2.5 Verify the external Nexus backend against the same callback contract; deliver an upstream update if needed, document its revision, and verify target connect/disconnect subscriptions. Record missing upstream dependencies as blockers, not successful conformance. Spec: Conformance Testing & Validation.

## 3. Expose the JSON-RPC event and lifecycle

Depends on group 1 and the selected backend's group 2 integration.

- [x] 3.1 Forward the existing notification through `JConnectionProperties::Event::DisplayConnectionChanged`, preserving the generated wire name and payload and the existing `Updated` helper. Verified installed helper/schema definitions and a passing `DisplayInfo.cpp` ARM syntax check. Spec: External Interfaces and payload requirement.
- [x] 3.2 Add REQ-F-13, the generated event contract, actual Covered Code, test descriptions, and change history to `openspec/specs/displayinfo.spec.md` without relocating it. Verify the required statement, boolean payload, exact wire spelling, and relative links. Spec: Requirements, External Interfaces, Covered Code, and Change History.
- [ ] 3.3 Add/run JSON-RPC subscription and legacy-compatibility tests for the exact lowercase event name, both boolean payloads, `updated(HDMI_CHANGE)`, resolution/frame-rate/HDCP notifications, and absence of connection events for unrelated updates. Update the canonical conformance table with executed evidence. Spec: compatibility and payload scenarios.
- [ ] 3.4 Complete lifecycle handling for baseline reset and stale queued work across activation/deactivation/reactivation; add/run focused cases and update lifecycle notes. Spec: startup scenario and Conformance Testing & Validation.

## 4. Validate integrated behavior

Depends on groups 2 and 3.

- [ ] 4.1 Build the plugin, matching RPC bindings, mocks, and L1 test library; run the focused notification/lifecycle suite. Verify compilation and successful tests, not merely syntax checks. Spec: Versioning & Compatibility and Conformance Testing & Validation.
- [ ] 4.2 Validate subscriptions on each enabled backend in both supported execution modes, including HDMI connect/disconnect, queued transitions, resolution-only changes, and restart; record payload/order and lifecycle evidence. Spec: all event scenarios.
- [ ] 4.3 Run `openspec validate displayinfo-display-connection-changed-event --strict` and check canonical/delta consistency and absence of a duplicate canonical document. Spec: Requirements, External Interfaces, and Change History.

## Current blockers and evidence

- L1 syntax-check execution stops at missing `gtest/gtest.h` in the supplied
  ARM target sysroot. The two added regression tests have not been run.
  Use a matching native L1/testframework environment or provision appropriate
  target test dependencies; host GTest alone does not repair the ARM sysroot.
- Strict OpenSpec validation stops at pre-existing invalid YAML in
  `openspec/config.yaml`. Independent documentation structure/link checks
  do not replace strict validation.
- Duplicate suppression/baseline handling and Linux, RPI, Nexus, subscription,
  and lifecycle work remain unchecked. FIFO/owner source fixes alone do not
  satisfy every requirement in this change.

## Workflow follow-up

- Archive after implementation, dependency delivery, and target validation are complete.
- Check the archive operation against the existing flat-file canonical spec
  layout; preserve `openspec/specs/displayinfo.spec.md` as the source of truth.
