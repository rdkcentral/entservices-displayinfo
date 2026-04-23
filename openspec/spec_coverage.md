# OpenSpec Coverage Report — DisplayInfo Plugin

**Generated:** 2026-04-23
**Spec File:** openspec/specs/DisplayInfo.spec.md
**Plugin Version:** 1.0.6

---

## Overall Score: 38 / 100

| Category | Score | Max |
|---|---|---|
| Code to Spec Coverage | 5 | 40 |
| Architecture HLA Specification | 10 | 10 |
| External Interface Specification | 7 | 10 |
| Security Specification | 2 | 10 |
| Versioning & Compatibility Specification | 7 | 10 |
| Conformance Testing Automation & Validation | 6 | 10 |
| Performance Specification | 1 | 10 |
| **TOTAL** | **38** | **100** |

---

## Category Breakdown

### 1. Code to Spec Coverage (5 / 40)

| Sub-Criterion | Score | Max | Evidence |
|---|---|---|---|
| Reference Coverage | 0 | 20 | No `// Spec:` comments found in any of the 13 plugin source files |
| Spec Existence | 0 | 10 | No spec reference comments exist in code — not applicable |
| Spec Completeness | 5 | 5 | `DisplayInfo.spec.md` contains Title, Description, and Requirements sections |
| No Orphaned Code | 0 | 5 | All 13 files under `plugin/` lack `// Spec:` references |

#### Files Scanned Under `plugin/`

| File | Has `// Spec:` Reference |
|---|---|
| `plugin/DisplayInfo.cpp` | No |
| `plugin/DisplayInfo.h` | No |
| `plugin/DisplayInfoJsonRpc.cpp` | No |
| `plugin/DisplayInfoTracing.h` | No |
| `plugin/Module.cpp` | No |
| `plugin/Module.h` | No |
| `plugin/DeviceSettings/PlatformImplementation.cpp` | No |
| `plugin/DeviceSettings/SoC_abstraction.h` | No |
| `plugin/DeviceSettings/RPI/SoC_abstraction.cpp` | No |
| `plugin/DeviceSettings/RPI/kms.h` | No |
| `plugin/Linux/PlatformImplementation.cpp` | No |
| `plugin/Linux/DRMConnector.h` | No |
| `plugin/RPI/PlatformImplementation.cpp` | No |

**Findings:** None of the 13 plugin source files contain a `// Spec: DisplayInfo` (or equivalent) comment. This is the primary driver of the low overall score. The spec itself is complete and well-formed (5/5 on completeness), but without code-side references, reference coverage and orphaned code scores are both 0.

---

### 2. Architecture HLA Specification (10 / 10)

| Sub-Criterion | Score | Max | Evidence |
|---|---|---|---|
| Presence of HLA Spec | 3 | 3 | Dedicated `## Architecture / Design` section present in spec |
| Clarity of Architecture Diagrams | 3 | 3 | Detailed Mermaid `graph TD` diagram with labeled nodes and arrows |
| Component/Module Mapping | 2 | 2 | Client, Thunder, Plugin, Notification, J\* handlers, exchange interfaces, DS/Linux/RPI implementations, IARM, DRM, BCM, and hardware all mapped |
| Traceability to Code | 2 | 2 | All diagram components trace to named source files and classes referenced in `## Covered Code` |

**Findings:** Full marks. The Architecture section is comprehensive, uses a Mermaid diagram (not ASCII art), labels all arrows with the data/control flow type (JSON-RPC, IARM Event, DS API Call, BCM Host API, etc.), and every component maps to an actual source file.

---

### 3. External Interface Specification (7 / 10)

| Sub-Criterion | Score | Max | Evidence |
|---|---|---|---|
| Presence of Interface Spec | 3 | 3 | Dedicated `## External Interfaces` section present |
| Defined Inputs/Outputs | 3 | 3 | Full field table for `displayinfo` property with types, descriptions, and error conditions; `updated` event payload documented |
| Documentation Completeness | 1 | 2 | `displayinfo` property and `updated` event documented; individual J\* registered endpoints (from `JGraphicsProperties`, `JConnectionProperties`, `JHDRProperties`, `JDisplayProperties`) not enumerated |
| Validation/Examples | 0 | 2 | No JSON-RPC request/response body examples present in spec |

**Findings:** The composite `displayinfo` GET property and `updated` event are well documented. However, the plugin also registers individual JSON-RPC endpoints via `JGraphicsProperties::Register`, `JConnectionProperties::Register`, `JHDRProperties::Register`, and `JDisplayProperties::Register` in `DisplayInfo::Initialize` — these generated endpoints (e.g., standalone `hdcpprotection` writable property, as evidenced by the `SetHDCPProtection_ExceptionHandling` L1 test case) are not itemized in the spec. No usage examples with JSON-RPC request/response bodies are provided.

---

### 4. Security Specification (2 / 10)

| Sub-Criterion | Score | Max | Evidence |
|---|---|---|---|
| Presence of Security Spec | 1 | 3 | `## Security` section present but marked `_Not applicable_` with a single sentence |
| Threat Model/Analysis | 0 | 3 | No threat model, attack surface analysis, or data flow security analysis present |
| Security Requirements | 1 | 2 | Brief reference to Thunder's plugin isolation/sandboxing; no explicit access control requirements |
| Validation/Testing | 0 | 2 | No security test cases or validation criteria documented |

**Findings:** The Security section is a placeholder. EDID data exposure and HDCP status are security-sensitive (EDID can reveal device fingerprints; HDCP state reveals content protection posture), yet no access control policy, trust level requirements, or sandboxing specifications are described. The absence of a threat model is the largest gap.

---

### 5. Versioning & Compatibility Specification (7 / 10)

| Sub-Criterion | Score | Max | Evidence |
|---|---|---|---|
| Presence of Versioning Spec | 3 | 3 | `## Versioning & Compatibility` section present |
| Versioning Scheme Defined | 2 | 3 | Version 1.0.6 (major.minor.patch) cited from source; no formal policy on when to bump each component defined |
| Backward/Forward Compatibility | 2 | 2 | Additive-only field policy noted; stable `updated` event signature documented |
| Migration/Upgrade Path | 0 | 2 | No migration or upgrade path instructions provided |

**Findings:** The versioning section correctly identifies the current version and states the additive-compatibility policy. However, the versioning scheme itself lacks formal rules (e.g., "breaking changes increment Major, new features increment Minor"). No migration guidance for consumers upgrading between versions is provided.

---

### 6. Conformance Testing Automation & Validation (6 / 10)

| Sub-Criterion | Score | Max | Evidence |
|---|---|---|---|
| Presence of Conformance Tests | 2 | 3 | L1 test file `Tests/L1Tests/tests/test_DisplayInfo.cpp` present with 13 test cases; L2 CMake exists but no DisplayInfo L2 test case found |
| Test Coverage | 2 | 3 | Tests cover `displayinfo` aggregated property, `connected`, `audiopassthrough`, `colorspace`, `framerate`, `colourdepth`, `quantizationrange`, `eotf`, `hdcpprotection` (get+set), `tvcapabilities`, `stbcapabilities`, `edid`, and `updated` notification — but no explicit traceability links between test cases and spec requirements |
| Test Documentation | 2 | 2 | `Tests/README.md` details how to build and run tests locally using the `act` tool and GitHub Actions workflows |
| Validation Results | 0 | 2 | No test results, pass/fail metrics, or coverage percentages documented in spec or test files |

**Findings:** L1 test coverage is broad across the major display properties. However, there is no L2 integration test for DisplayInfo (the `Tests/L2Tests/CMakeLists.txt` has no `PLUGIN_DISPLAYINFO` conditional). No requirement traceability comments (e.g., `// Tests REQ-001`) appear in the test file, and no recorded test results are available in the spec.

---

### 7. Performance Specification (1 / 10)

| Sub-Criterion | Score | Max | Evidence |
|---|---|---|---|
| Presence of Performance Spec | 1 | 3 | `## Performance` section present but entirely marked `_Not applicable_` |
| Defined Performance Metrics | 0 | 3 | No latency, throughput, or response time metrics defined |
| Test Coverage for Performance | 0 | 2 | No performance benchmarks or load tests in test suite |
| Results & Validation | 0 | 2 | No performance results documented |

**Findings:** The performance section is a placeholder. For a display information plugin, relevant metrics would include maximum latency of the `displayinfo` property response under load, HDMI hotplug event propagation latency to the `updated` notification, and EDID read time. None of these are specified.

---

## Gaps & Issues

### Unspecified Methods (registered in code but missing from spec)

| Method / Endpoint | Source File | Notes |
|---|---|---|
| Individual `IGraphicsProperties` JSON-RPC endpoints (e.g., `totalgpuram`, `freegpuram`) | `plugin/DisplayInfo.cpp` via `JGraphicsProperties::Register` | Generated by `JGraphicsProperties` header; not enumerated individually in spec |
| Individual `IConnectionProperties` JSON-RPC endpoints (e.g., `connected`, `audiopassthrough`, `width`, `height`, `hdcpprotection` writable) | `plugin/DisplayInfo.cpp` via `JConnectionProperties::Register` | `SetHDCPProtection_ExceptionHandling` L1 test implies a writable `hdcpprotection` endpoint exists — contradicts spec's "read-only" requirement |
| Individual `IHDRProperties` JSON-RPC endpoints (e.g., standalone `hdrtype`, `tvcapabilities`, `stbcapabilities`, `hdrsetting`) | `plugin/DisplayInfo.cpp` via `JHDRProperties::Register` | Not documented as individual endpoints in spec |
| Individual `IDisplayProperties` JSON-RPC endpoints (e.g., standalone `colorspace`, `framerate`, `colourdepth`, `quantizationrange`, `eotf`) | `plugin/DisplayInfo.cpp` via `JDisplayProperties::Register` | Not documented as individual endpoints in spec |

### Orphaned Code (no `// Spec:` reference comment found)

All 13 source files under `plugin/` are orphaned:

- `plugin/DisplayInfo.cpp`
- `plugin/DisplayInfo.h`
- `plugin/DisplayInfoJsonRpc.cpp`
- `plugin/DisplayInfoTracing.h`
- `plugin/Module.cpp`
- `plugin/Module.h`
- `plugin/DeviceSettings/PlatformImplementation.cpp`
- `plugin/DeviceSettings/SoC_abstraction.h`
- `plugin/DeviceSettings/RPI/SoC_abstraction.cpp`
- `plugin/DeviceSettings/RPI/kms.h`
- `plugin/Linux/PlatformImplementation.cpp`
- `plugin/Linux/DRMConnector.h`
- `plugin/RPI/PlatformImplementation.cpp`

### Spec Sections with Incomplete Code Traceability

The `## Covered Code` section in the spec omits the following files that exist under `plugin/`:

| Missing File | Present in Workspace |
|---|---|
| `plugin/DisplayInfoTracing.h` | Yes |
| `plugin/DeviceSettings/RPI/SoC_abstraction.cpp` | Yes |
| `plugin/DeviceSettings/RPI/kms.h` | Yes |
| `plugin/DeviceSettings/RPI/kms.c` | Yes (from workspace structure) |

### Open Queries Affecting Scores

| Open Query (from spec) | Impacted Category | Score Impact |
|---|---|---|
| "No explicit error handling model for EDID parsing failure — unclear if partial fields or full error should be returned" | External Interface Specification — Documentation Completeness | −1 pt (Category 3) |
| "No explicit security model or access control policy" | Security Specification — all sub-criteria | −8 pts (Category 4) |
| "IDisplayProperties non-essential at init — which fields are affected when unavailable?" | External Interface Specification — Documentation Completeness | Contributes to −1 pt (Category 3) |
| "Colorimetry behavioral requirements out of scope (archived change)" | External Interface Specification — Documentation Completeness | Noted; no additional deduction since field is documented |

---

## Suggestions for Improvement

### 1. Code to Spec Coverage _(scored 5/40 — primary gap)_
- **Add `// Spec: DisplayInfo` comments** to all files under `plugin/`. At minimum, add to the top of every `.cpp` and `.h` file, e.g.:
  ```cpp
  // Spec: DisplayInfo — openspec/specs/DisplayInfo.spec.md
  ```
  This single action would raise the category score from 5 to 40 (assuming the spec remains complete and valid).
- **Consider method-level traceability** for key implementation functions. For example, in `DisplayInfo::Initialize`, add `// Spec: DisplayInfo — Requirements: IConnectionProperties acquisition and IConfiguration::Configure()` to link code directly to requirements.
- **Add `plugin/DisplayInfoTracing.h`, `plugin/DeviceSettings/RPI/SoC_abstraction.cpp`, and `plugin/DeviceSettings/RPI/kms.h`** to the `## Covered Code` section of the spec to eliminate the traceability gap.

### 2. External Interface Specification _(scored 7/10)_
- **Document individual J\* registered endpoints** in the spec. Read the `JGraphicsProperties`, `JConnectionProperties`, `JHDRProperties`, and `JDisplayProperties` header files and enumerate every registered JSON-RPC method/property/event individually in the External Interfaces section. Specifically, clarify whether `hdcpprotection` is writable (the L1 test `SetHDCPProtection_ExceptionHandling` implies it is, which conflicts with the spec's "read-only" requirement).
- **Add at least one JSON-RPC usage example** per method group — a request JSON body (or empty for GET) and an example response body with realistic field values. This directly addresses the 0/2 Validation/Examples sub-criterion.

### 3. Security Specification _(scored 2/10)_
- **Write a minimal threat model** identifying at least three attack surfaces: (a) unauthenticated read of EDID data (device fingerprinting), (b) unrestricted `hdcpprotection` write if the property is writable, and (c) out-of-process implementation boundary (RPC connection hijack). Document mitigations for each.
- **Define access control requirements** — specify whether the plugin requires a particular Thunder security token or trust level for property access, referencing Thunder's security framework.
- **Add a security test case** (L1 or L2) that validates a low-trust client cannot write to `hdcpprotection` (if it is restricted), and document expected behavior in the spec.

### 4. Versioning & Compatibility Specification _(scored 7/10)_
- **Define a formal versioning policy** — document under what conditions the Major, Minor, and Patch components are incremented (e.g., "Major incremented on breaking JSON-RPC API changes; Minor on new optional fields; Patch on bug fixes").
- **Add a migration guide** — even a short bullet list describing what a consumer must change when upgrading from 1.0.x to a future 1.1.x (e.g., "new optional response fields will be present; existing fields will not be removed or renamed").

### 5. Conformance Testing Automation & Validation _(scored 6/10)_
- **Add a DisplayInfo L2 test** under `Tests/L2Tests/tests/` with a corresponding `PLUGIN_DISPLAYINFO` conditional in `Tests/L2Tests/CMakeLists.txt` to achieve integration-level coverage.
- **Add explicit requirement traceability** in test case names or comments (e.g., `// REQ: Plugin MUST notify subscribed clients with updated event`), linking each test to a bullet in the spec's `## Requirements` section.
- **Document expected test results** — either in `Tests/README.md` or in the spec's Conformance Testing section, state the minimum pass rate criteria (e.g., "all L1 test cases must pass").

### 6. Performance Specification _(scored 1/10)_
- **Define at least two measurable performance metrics**: (a) maximum response time for the `displayinfo` property GET under normal conditions (e.g., ≤ 100 ms), and (b) maximum latency from HDMI hotplug event to `updated` notification delivery (e.g., ≤ 500 ms).
- **Add a performance test** — even a simple L1 test that measures and asserts the response time of `get_displayinfo()` to validate the defined metric.

---

## References

- Spec: [openspec/specs/DisplayInfo.spec.md](openspec/specs/DisplayInfo.spec.md)
- Plugin Source: [plugin/DisplayInfo.cpp](plugin/DisplayInfo.cpp), [plugin/DisplayInfo.h](plugin/DisplayInfo.h)
- JSON-RPC Registration: [plugin/DisplayInfoJsonRpc.cpp](plugin/DisplayInfoJsonRpc.cpp)
- Platform Implementations: [plugin/DeviceSettings/PlatformImplementation.cpp](plugin/DeviceSettings/PlatformImplementation.cpp), [plugin/Linux/PlatformImplementation.cpp](plugin/Linux/PlatformImplementation.cpp), [plugin/RPI/PlatformImplementation.cpp](plugin/RPI/PlatformImplementation.cpp)
- Tests: [Tests/L1Tests/tests/test_DisplayInfo.cpp](Tests/L1Tests/tests/test_DisplayInfo.cpp), [Tests/L2Tests/CMakeLists.txt](Tests/L2Tests/CMakeLists.txt)
- Test Documentation: [Tests/README.md](Tests/README.md)
- Skill: [.github/skills/openspec-coverage/SKILL.md](.github/skills/openspec-coverage/SKILL.md)
