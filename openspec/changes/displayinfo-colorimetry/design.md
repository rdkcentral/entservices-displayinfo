## Context

The `DisplayInfo` plugin aggregates four C++ interfaces (`IGraphicsProperties`, `IConnectionProperties`, `IHDRProperties`, `IDisplayProperties`) into a single WPEFramework JSON-RPC service. `IDisplayProperties` — which includes `Colorimetry()` — is already registered at runtime via `Exchange::JDisplayProperties::Register(*this, _displayProperties)` in `DisplayInfo::Initialize`. This means the JSON-RPC auto-binding for `colorimetry` is already wired; the implementation exists in the DeviceSettings backend, and the spec is simply missing.

The gap is a combination of:
1. **Behaviour contract missing**: No spec defines the expected return value when no display is connected. The DeviceSettings backend currently returns `ERROR_GENERAL` in that case (should be `ERROR_NONE` with empty iterator).
2. **Spec coverage gap**: `Colorimetry`, `ColorSpace`, `FrameRate`, `ColourDepth`, `QuantizationRange`, and `EOTF` are orphaned (identified as gap G-01 in coverage report).
3. **Linux/DRM backend**: Returns `ERROR_UNAVAILABLE` for colorimetry (no EDID/DRM colorimetry parsing available), which the JSON-RPC layer should treat as empty list.
4. **RPI backend**: Returns `ERROR_UNAVAILABLE` (stub only).

Backend summary:

| Backend | `Colorimetry()` before this change | Behaviour after this change |
|---------|------------------------------------|-----------------------------||
| DeviceSettings | `ERROR_GENERAL` when disconnected or EDID fails | `ERROR_NONE` + empty iterator in ALL paths including EDID parse failure |
| Linux/DRM | `ERROR_UNAVAILABLE` | Unchanged — `ERROR_UNAVAILABLE` propagated to caller |
| BCM/RPI | `ERROR_UNAVAILABLE` | Unchanged — `ERROR_UNAVAILABLE` propagated to caller |
| Nexus | External; unknown | Out of scope |

## Goals / Non-Goals

**Goals:**
- Define the complete behavioural contract for `DisplayInfo.colorimetry` in a dedicated spec.
- Fix the DeviceSettings backend: return empty `IColorimetryIterator` with `ERROR_NONE` in all non-success paths (disconnected, EDID unavailable, EDID verification failure).
- Eliminate manual heap allocation (`new[]`/`delete[]`) in `Colorimetry()` — use RAII containers.
- Wrap all DeviceSettings library calls in a typed exception handler to prevent unhandled exceptions.
- Document the Linux/DRM and RPI `ERROR_UNAVAILABLE` handling as platform-specific behaviour (separate future change).
- Add L1 test cases covering connected, disconnected, and EDID-parse-fail scenarios.
- Close spec coverage gap G-01 by adding `Colorimetry` (and the other orphaned `IDisplayProperties` methods) to the `displayinfo.spec.md` Covered Code section.

**Non-Goals:**
- Implementing EDID colorimetry parsing for the Linux/DRM backend (separate change).
- Implementing colorimetry for the BCM/RPI backend beyond the current stub.
- Changing the Nexus backend (out of scope and fetched externally).
- Adding a `colorimetry` field to the existing `displayinfo` composite property — it remains a separate property.

## Decisions

### D-01: Separate JSON-RPC property, not a field in `displayinfo`

`IDisplayProperties::Colorimetry()` returns an iterator, not a scalar. Adding it to the `displayinfo` composite property would require embedding an array inside the existing `DisplayinfoData` struct, which is a breaking change to the schema. Instead, the `JDisplayProperties` auto-binding already exposes it as an independent property `DisplayInfo.1.colorimetry`.

**Alternative considered:** Add `colorimetry` as an array field to `DisplayinfoData`. Rejected — breaking schema change, and large aggregated responses with iterators degrade performance.

### D-02: `ERROR_NONE` + empty iterator is the canonical response for ALL non-connected paths in DeviceSettings

The manual implementation extends D-02 further than the original proposal: **all** error paths in the DeviceSettings backend (disconnected, EDID unavailable, EDID verification failure, device exception) return `ERROR_NONE` with an empty `colorimetryCaps` list. This unifies error handling and aligns with the least-surprise principle — clients receive a consistent "no data" signal rather than an error they must distinguish from genuine API failures.

**Implementation detail:** The rewritten `Colorimetry()` uses a `try/catch (const device::Exception&)` block wrapping the entire EDID read and parse path. If any step fails, `colorimetryCaps` remains empty and the method always returns `Core::ERROR_NONE` at the end. This also eliminates all manual `new[]/delete[]` allocations in favour of `std::vector<unsigned char>`, removing a previous Coverity finding.

**Alternative considered:** Keep `ERROR_GENERAL` for EDID parse failures to distinguish from a disconnected state. Rejected in the manual implementation — the distinction has no actionable meaning for callers; empty list is the complete and correct response in both cases.

### D-03: Fix only DeviceSettings backend in this change

The Linux/DRM and RPI backends already return `ERROR_UNAVAILABLE`. Upon reading `JDisplayProperties.h` (OQ-01 resolved), it is confirmed that `JDisplayProperties` does **not** map `ERROR_UNAVAILABLE` to an empty array — it propagates the error code directly to the caller. The auto-binding only writes to the JSON result array when `_errorCode == ERROR_NONE` and the iterator is non-null.

Consequence: on Linux/DRM and RPI platforms, `DisplayInfo.colorimetry` will respond with `ERROR_UNAVAILABLE`. This is acceptable for this change — the user requirement (`no display → empty list + ERROR_NONE`) applies only to the DeviceSettings backend scenario. Fixing Linux/DRM and RPI to return `ERROR_NONE + empty iterator` is a separate future change.

The spec scenario "Backend does not support colorimetry discovery" has been updated accordingly to reflect `ERROR_UNAVAILABLE` (not empty list) for those backends.

**No JSON-RPC-layer change required** — only the DeviceSettings backend disconnected-path fix is in scope.

### D-04: No new JSON-RPC registration code required

`Exchange::JDisplayProperties::Register(*this, _displayProperties)` is already called in `DisplayInfo::Initialize` when `_displayProperties != nullptr`. The `colorimetry` JSON-RPC property is therefore already registered. The implementation task is solely fixing the DeviceSettings backend return value for the disconnected case.

## Risks / Trade-offs

| Risk | Mitigation |
|------|-----------|
| `JDisplayProperties` null-iterator behaviour is undocumented | Resolved (OQ-01): binding only populates result when `ERROR_NONE`; null/empty iterator yields `[]` |
| DeviceSettings EDID failure path previously indistinguishable from disconnected | Resolved by manual impl: unified — both paths return `ERROR_NONE` + empty list |
| Manual `new[]`/`delete[]` in old implementation risked memory leak on exception | Resolved by manual impl: replaced with `std::vector<unsigned char>` (RAII) |
| Colorimetry EDID bitmask mapping may be incomplete (e.g., DCI-P3 maps to `COLORIMETRY_OTHER`) | Document known mapping limitations in spec Open Queries; no code change needed now |
| Nexus backend behaviour unknown | Mark Nexus as out of scope in spec; add open query |

## Open Questions

- ~~OQ-01~~: **RESOLVED** — `JDisplayProperties.h` confirmed via source inspection:
  - `colorimetry` IS auto-bound by `JDisplayProperties::Register`. ✓
  - `ERROR_NONE` + null iterator → empty JSON array `[]` returned to client. ✓
  - `ERROR_UNAVAILABLE` (or any non-`ERROR_NONE`) → error propagated to client; JSON array NOT populated. Consequence: D-03 was partially incorrect. Linux/DRM and RPI backends will surface `ERROR_UNAVAILABLE` to clients (not empty list). Scoped fix: DeviceSettings "no display" case only.
- OQ-02: Should `colorimetry` be included in the composite `displayinfo` property in a future change? If so, the schema change should be planned now to avoid a breaking bump later.
