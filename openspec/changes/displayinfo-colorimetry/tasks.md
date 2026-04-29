## 1. Verify JSON-RPC Auto-Binding

- [x] 1.1 Read `JDisplayProperties.h` generated code and confirm `colorimetry` property is auto-bound when `JDisplayProperties::Register` is called
- [x] 1.2 Confirm how `JDisplayProperties` handles a null or empty `IColorimetryIterator*` output — verify it emits an empty JSON array with `ERROR_NONE` (not an error)
- [x] 1.3 Document the findings in design.md OQ-01 with resolution

## 2. Fix DeviceSettings Backend — Disconnected Path

- [x] 2.1 In `plugin/DeviceSettings/PlatformImplementation.cpp`, locate `DisplayInfoImplementation::Colorimetry()`
- [x] 2.2 Change the `GetEdidBytes()` failure branch: instead of setting `ret = ERROR_GENERAL` and returning, set the output iterator to an empty `ColorimetryIteratorImplementation` list and return `ERROR_NONE`
- [x] 2.3 Retain `ERROR_GENERAL` for the EDID parse failure path (`EDID_Verify` fails) — this is a genuine error distinct from "not connected"
    > **Updated by manual implementation:** `ERROR_GENERAL` is NOT retained for EDID parse failures. All non-connected paths (parse fail, exception) now return `ERROR_NONE` + empty list. Manual implementation also replaces `new[]`/`delete[]` with `std::vector` and wraps the full EDID path in `try/catch (const device::Exception&)`.
- [x] 2.4 Verify the iterator is never left as a dangling pointer in any error path (Coverity compliance)
    > RAII `std::vector` used; iterator always created at the end of the function.

## 3. Update Spec — displayinfo.spec.md Covered Code

- [x] 3.1 In `openspec/specs/displayinfo.spec.md`, add `DisplayInfoImplementation::Colorimetry`, `DisplayInfoImplementation::ColorSpace`, `DisplayInfoImplementation::FrameRate`, `DisplayInfoImplementation::ColourDepth`, `DisplayInfoImplementation::QuantizationRange`, and `DisplayInfoImplementation::EOTF` to the Covered Code section for both DeviceSettings and RPI backends (closing gap G-01)
- [x] 3.2 Update `## Change History` in `displayinfo.spec.md` with a 2026-04-29 entry

## 4. Add L1 Tests

- [x] 4.1 In `Tests/L1Tests/tests/test_DisplayInfo.cpp`, add test `Colorimetry_Connected_ReturnsColorimetryList`: mock `GetEdidBytes` returning valid EDID bytes with a known colorimetry bitmask; assert returned iterator contains expected `ColorimetryType` values
- [x] 4.2 Add test `Colorimetry_Disconnected_ReturnsEmptyList`: mock `isDisplayConnected()` returning false; assert returned iterator is empty and return code is `ERROR_NONE`
- [x] 4.3 Add test `Colorimetry_EdidParseFails_ReturnsEmptyList`: mock `isDisplayConnected()` returning true, EDID bytes provided, but `EDID_Verify()` failing; assert return code is `ERROR_NONE` and iterator is empty (manual impl no longer returns `ERROR_GENERAL` here)
- [ ] 4.4 Confirm tests build and pass under the `USE_DEVICESETTINGS` build flag

## 5. Final Verification

- [ ] 5.1 Run existing L1 test suite and confirm no regressions
- [ ] 5.2 Re-run openspec coverage check (`/opsx:coverage`) and confirm gap G-01 is closed and overall score improves
- [ ] 5.3 Verify `DisplayInfo.1.colorimetry` JSON-RPC property returns correct value on a device with DeviceSettings backend — connected display with known colorimetry
