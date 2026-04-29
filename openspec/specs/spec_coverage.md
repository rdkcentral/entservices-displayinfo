# OpenSpec Coverage Report — DisplayInfo Plugin

**Generated:** 2026-04-29  
**Repository:** `entservices-displayinfo`  
**Specs scanned:** `openspec/specs/displayinfo.spec.md`  
**Code scanned:** `plugin/` (all `.cpp`, `.h`, `.c` files)

---

## Overall Score

| Category | Weight | Score | Earned |
|----------|--------|-------|--------|
| Code to Spec Coverage | 40% | 91% | **36.5 / 40** |
| Architecture HLA Specification | 10% | 100% | **10 / 10** |
| External Interface Specification | 10% | 90% | **9 / 10** |
| Security Specification | 10% | 70% | **7 / 10** |
| Versioning & Compatibility | 10% | 80% | **8 / 10** |
| Conformance Testing & Validation | 10% | 60% | **6 / 10** |
| Performance Specification | 10% | 50% | **5 / 10** |
| **TOTAL** | **100%** | | **81.5 / 100** |

---

## 1. Code to Spec Coverage (36.5 / 40)

### 1a. Reference Coverage (17 / 20)

**Primary signal:** `## Covered Code` section in `displayinfo.spec.md`  
**Supplementary signal:** `// Spec:` comments in source files — **none found**

#### Method census by file

| File | Total Methods | Covered | Orphaned | Coverage |
|------|--------------|---------|----------|----------|
| `plugin/DisplayInfo.cpp` | 9 | 9 | 0 | 100% |
| `plugin/DisplayInfoJsonRpc.cpp` | 4 | 4 | 0 | 100% |
| `plugin/DisplayInfoTracing.h` | 1 | 1 | 0 | 100% |
| `plugin/Module.h` | 1 | 1 | 0 | 100% |
| `plugin/Module.cpp` | 1 | 1 | 0 | 100% |
| `plugin/DeviceSettings/PlatformImplementation.cpp` | 28 | 22 | 6 | 79% |
| `plugin/DeviceSettings/SoC_abstraction.h` | 4 | 4 | 0 | 100% |
| `plugin/DeviceSettings/RPI/SoC_abstraction.cpp` | 8 | 4 | 4 | 50% |
| `plugin/DeviceSettings/RPI/kms.h` | 1 | 1 | 0 | 100% |
| `plugin/DeviceSettings/RPI/kms.c` | 5 | 3 | 2 | 60% |
| `plugin/Linux/DRMConnector.h` | 6 | 5 | 1 | 83% |
| `plugin/Linux/PlatformImplementation.cpp` | 43 | 35 | 8 | 81% |
| `plugin/RPI/PlatformImplementation.cpp` | 27 | 22 | 5 | 81% |
| `Tests/L1Tests/tests/test_DisplayInfo.cpp` | 1 | 1 | 0 | 100% |
| **Total** | **139** | **113** | **26** | **81%** |

> **Note on private methods:** 16 of the 26 uncovered are private/internal helpers
> (e.g. `QueryDisplayProperties`, `GetMemory`, `GetValueForKey`, `GetMessageValues`).
> The 10 uncovered **public interface methods** are listed below.

**Score:** 20 × 0.85 (adjusted for private-method context) = **17 / 20**

---

### 1b. Spec Existence (10 / 10)

| Spec referenced | File exists? |
|-----------------|-------------|
| `displayinfo.spec.md` | ✅ `openspec/specs/displayinfo.spec.md` |

**Score: 10 / 10**

---

### 1c. Spec Completeness (5 / 5)

`displayinfo.spec.md` contains all required sections:

| Section | Present? |
|---------|---------|
| Overview | ✅ |
| Description | ✅ |
| Requirements | ✅ |
| Architecture / Design | ✅ |
| External Interfaces | ✅ |
| Performance | ✅ |
| Security | ✅ |
| Versioning & Compatibility | ✅ |
| Conformance Testing & Validation | ✅ |
| Covered Code | ✅ |
| Open Queries | ✅ |
| References | ✅ |
| Change History | ✅ |

**Score: 5 / 5**

---

### 1d. No Orphaned Code (4.5 / 5)

10 public interface methods across two backends have no spec coverage:

| File | Orphaned Method | Interface |
|------|----------------|-----------|
| `plugin/DeviceSettings/PlatformImplementation.cpp` | `ColorSpace` | `IDisplayProperties` |
| `plugin/DeviceSettings/PlatformImplementation.cpp` | `FrameRate` | `IDisplayProperties` |
| `plugin/DeviceSettings/PlatformImplementation.cpp` | `ColourDepth` | `IDisplayProperties` |
| `plugin/DeviceSettings/PlatformImplementation.cpp` | `QuantizationRange` | `IDisplayProperties` |
| `plugin/DeviceSettings/PlatformImplementation.cpp` | `Colorimetry` | `IDisplayProperties` |
| `plugin/DeviceSettings/PlatformImplementation.cpp` | `EOTF` | `IDisplayProperties` |
| `plugin/RPI/PlatformImplementation.cpp` | `FrameRate` | `IDisplayProperties` |
| `plugin/RPI/PlatformImplementation.cpp` | `ColourDepth` | `IDisplayProperties` |
| `plugin/RPI/PlatformImplementation.cpp` | `QuantizationRange` | `IDisplayProperties` |
| `plugin/RPI/PlatformImplementation.cpp` | `EOTF` | `IDisplayProperties` |

> **Pattern:** All orphaned public methods belong to `IDisplayProperties`. The spec covers this
> interface at a surface level but does not enumerate `ColorSpace`, `FrameRate`, `ColourDepth`,
> `QuantizationRange`, `Colorimetry`, and `EOTF` in its Covered Code or Requirements sections.

**Score:** 5 × 0.90 = **4.5 / 5**

---

## 2. Architecture HLA Specification (10 / 10)

| Sub-criterion | Max | Score | Notes |
|---------------|-----|-------|-------|
| Presence of HLA spec | 3 | **3** | Component diagram + lifecycle diagram present |
| Clarity of architecture diagrams | 3 | **3** | ASCII diagrams clearly show OOP boundary, event flow, backend selection |
| Component/module mapping | 2 | **2** | All five layers mapped: plugin, OOP container, four backends |
| Traceability to code | 2 | **2** | `## Covered Code` links architecture components to source files/methods |

**Score: 10 / 10**

---

## 3. External Interface Specification (9 / 10)

| Sub-criterion | Max | Score | Notes |
|---------------|-----|-------|-------|
| Presence of interface spec | 3 | **3** | Dedicated `## External Interfaces` section present |
| Defined inputs/outputs | 3 | **3** | All `DisplayinfoData` fields typed; both enums fully documented; error codes listed |
| Documentation completeness | 2 | **2** | Configuration keys fully documented; JSON-RPC property + event both described |
| Validation/examples | 2 | **1** | Test references exist but no JSON request/response example payloads provided |

**Gap:** No concrete JSON example of a `DisplayInfo.1.displayinfo` response or `updated` event payload.

**Score: 9 / 10**

---

## 4. Security Specification (7 / 10)

| Sub-criterion | Max | Score | Notes |
|---------------|-----|-------|-------|
| Presence of security spec | 3 | **3** | `## Security` section present |
| Threat model/analysis | 3 | **2** | IPC boundary, HDCP setter exposure, and EDID trust boundary called out; no formal threat model (STRIDE or equivalent) |
| Security requirements | 2 | **2** | ACL requirement for HDCP setter and typed exception handler requirement specified |
| Validation/testing | 2 | **0** | No security-specific tests or penetration evidence documented |

**Gaps:**
- No formal threat model (STRIDE, attack tree, or equivalent).
- No security test cases (e.g., ACL enforcement test, invalid EDID handling test).

**Score: 7 / 10**

---

## 5. Versioning & Compatibility (8 / 10)

| Sub-criterion | Max | Score | Notes |
|---------------|-----|-------|-------|
| Presence of versioning spec | 3 | **3** | `## Versioning & Compatibility` section present |
| Versioning scheme defined | 3 | **3** | Semver table, major/minor/patch semantics, `DisplayInfo.1` advertisement described |
| Backward/forward compatibility | 2 | **2** | Additive field rule and breaking-change definition stated |
| Migration/upgrade path | 2 | **0** | No migration guide for consumers upgrading across major versions |

**Gap:** No documented migration procedure for a future `DisplayInfo.2` major version bump.

**Score: 8 / 10**

---

## 6. Conformance Testing & Validation (6 / 10)

| Sub-criterion | Max | Score | Notes |
|---------------|-----|-------|-------|
| Presence of conformance tests | 3 | **3** | L1 GTest suite and L2 integration tests present |
| Test coverage | 3 | **2** | L1 covers most DeviceSettings methods; L2 test cases are TBD; no test for Linux/DRM or RPI backends |
| Test documentation | 2 | **1** | Test file listed but no `cmake --build` / `ctest` invocation documented |
| Validation results | 2 | **0** | No pass/fail test results recorded in the spec |

**Gaps:**
- L2 test cases not yet written.
- No backend-specific tests for Linux/DRM or RPI.
- No documented test-run instructions (build flags, `ctest` invocations).
- No captured test results (CI badge or results table).

**Score: 6 / 10**

---

## 7. Performance Specification (5 / 10)

| Sub-criterion | Max | Score | Notes |
|---------------|-----|-------|-------|
| Presence of performance spec | 3 | **3** | `## Performance` section present |
| Defined performance metrics | 3 | **2** | OOP startup timeout (2 000 ms) and event latency (<500 ms) specified; no GPU memory read latency target |
| Test coverage for performance | 2 | **0** | No automated performance tests referenced |
| Results & validation | 2 | **0** | No measured performance data documented |

**Gaps:**
- No latency target for EDID or GPU memory read calls.
- No performance test harness or benchmark results.

**Score: 5 / 10**

---

## Summary of Gaps & Suggestions

### Critical (blocking high scores)

| # | Gap | Affected Category | Suggested Fix |
|---|-----|------------------|---------------|
| G-01 | `IDisplayProperties` methods `ColorSpace`, `FrameRate`, `ColourDepth`, `QuantizationRange`, `Colorimetry`, `EOTF` not in spec | Code Coverage | Add these methods to `## Covered Code` and expand Requirements/External Interfaces |
| G-02 | No security tests or validation evidence | Security | Add a security test section or CI job testing ACL enforcement and EDID rejection |
| G-03 | No performance test results | Performance | Add benchmark results or CI performance job output |

### Moderate (score improvement)

| # | Gap | Affected Category | Suggested Fix |
|---|-----|------------------|---------------|
| G-04 | No `// Spec:` comments in source files | Code Coverage | Add `// Spec: displayinfo` comments at each covered method for supplementary traceability |
| G-05 | No JSON payload examples | External Interfaces | Add a sample `displayinfo` response and `updated` event JSON object |
| G-06 | No formal threat model | Security | Add a short STRIDE table or attack surface description |
| G-07 | L2 tests not yet written | Conformance Testing | Implement L2 test cases covering JSON-RPC end-to-end |
| G-08 | No test-run instructions | Conformance Testing | Document cmake/ctest invocations in Conformance section |

### Minor

| # | Gap | Affected Category | Suggested Fix |
|---|-----|------------------|---------------|
| G-09 | No migration path for major version bump | Versioning | Add a brief `DisplayInfo.1 → DisplayInfo.2` upgrade note |
| G-10 | Private internal helpers in Linux backend not covered | Code Coverage | Document or annotate `QueryDisplayProperties`, `QueryEDID`, `GetMemory` |
| G-11 | Open query OQ-02 (legacy event path) unresolved | Architecture | Decide and document deprecation of `RegisterAll`/`event_updated` |

---

## Spec Inventory

| Spec file | Status | Required sections | Completeness |
|-----------|--------|------------------|-------------|
| `openspec/specs/displayinfo.spec.md` | ✅ exists | 13 / 13 present | **100%** |

---

## Coverage Trend

| Date | Score | Notes |
|------|-------|-------|
| 2026-04-29 | **81.5 / 100** | Initial spec created and templated |
