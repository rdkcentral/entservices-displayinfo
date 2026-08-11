## 1. Interface Change (entservices-interfaces / ThunderInterfaces)

- [x] 1.1 Verify that `Exchange::IDisplayProperties` (`interfaces/IDisplayInfo.h`) contains all required `ColorimetryType` enum values: `COLORIMETRY_BT709`, `COLORIMETRY_SMPTE170M`, `COLORIMETRY_XVYCC709`, `COLORIMETRY_XVYCC601`, `COLORIMETRY_BT2020RGB_YCBCR`, `COLORIMETRY_BT2020YCCBCBRC`, `COLORIMETRY_UNKNOWN`
- [x] 1.2 Add `virtual Core::hresult CurrentColorimetry(ColorimetryType& value /* @out */) const = 0;` to `Exchange::IDisplayProperties` with `@property` and `@brief` annotations in the IDL/header
- [x] 1.3 Regenerate `JDisplayProperties.h` from the updated interface (run the Thunder code generator) so that `getCurrentColorimetry` is auto-bound in the generated header
- [x] 1.4 Verify `dsDisplayMatrixCoefficients_t` enum availability in the DS HAL header (`dsAVDTypes.h` or equivalent) — confirm `dsDISPLAY_MATRIXCOEFFICIENT_BT_709`, `dsDISPLAY_MATRIXCOEFFICIENT_SMPTE_170M`, `dsDISPLAY_MATRIXCOEFFICIENT_XvYCC_709`, `dsDISPLAY_MATRIXCOEFFICIENT_eXvYCC_601`, `dsDISPLAY_MATRIXCOEFFICIENT_BT_2020_NCL`, `dsDISPLAY_MATRIXCOEFFICIENT_BT_2020_CL` are all defined

## 2. DeviceSettings Backend Implementation

- [x] 2.1 Add `CurrentColorimetry(ColorimetryType& value)` override to `DisplayInfoImplementation` in `plugin/DeviceSettings/PlatformImplementation.cpp`
- [x] 2.2 Implement the method body: call `device::Host::getInstance().getDefaultVideoPortName()` and `device::Host::getInstance().getVideoOutputPort()` to obtain the port; if `!vPort.isDisplayConnected()`, set `value = COLORIMETRY_UNKNOWN` and return `ERROR_NONE`
- [x] 2.3 Call `vPort.getMatrixCoefficients()` and apply the DS-to-Thunder enum mapping switch statement (all 6 known coefficients + default → `COLORIMETRY_UNKNOWN`)
- [x] 2.4 Wrap all DeviceSettings calls in `try { ... } catch (const device::Exception& err) { LOGERR(...); value = COLORIMETRY_UNKNOWN; }` and always return `ERROR_NONE`

## 3. Linux and RPI Backend Stubs

- [x] 3.1 Add `CurrentColorimetry(ColorimetryType& value)` override to `plugin/Linux/PlatformImplementation.cpp` returning `Core::ERROR_UNAVAILABLE` (N/A — Linux backend does not implement `IDisplayProperties`; interface not exposed)
- [x] 3.2 Add `CurrentColorimetry(ColorimetryType& value)` override to `plugin/RPI/PlatformImplementation.cpp` returning `Core::ERROR_UNAVAILABLE`

## 4. Spec Coverage Update

- [x] 4.1 Add `CurrentColorimetry` to the `## Covered Code` section in `openspec/specs/displayinfo.spec.md` (cross-reference `Exchange::IDisplayProperties::CurrentColorimetry`)

## 5. L1 Tests

- [x] 5.1 Add test case in `Tests/L1Tests/tests/test_DisplayInfo.cpp`: display connected, `getMatrixCoefficients()` returns `dsDISPLAY_MATRIXCOEFFICIENT_BT_709` → property returns `COLORIMETRY_BT709`, `ERROR_NONE`
- [x] 5.2 Add test case: display connected, `getMatrixCoefficients()` returns `dsDISPLAY_MATRIXCOEFFICIENT_BT_2020_NCL` → property returns `COLORIMETRY_BT2020RGB_YCBCR`, `ERROR_NONE`
- [x] 5.3 Add test case: display connected, `getMatrixCoefficients()` returns `dsDISPLAY_MATRIXCOEFFICIENT_BT_2020_CL` → property returns `COLORIMETRY_BT2020YCCBCBRC`, `ERROR_NONE`
- [x] 5.4 Add test case: display connected, `getMatrixCoefficients()` returns `dsDISPLAY_MATRIXCOEFFICIENT_SMPTE_170M` → property returns `COLORIMETRY_SMPTE170M`, `ERROR_NONE`
- [x] 5.5 Add test case: display connected, `getMatrixCoefficients()` returns `dsDISPLAY_MATRIXCOEFFICIENT_XvYCC_709` → property returns `COLORIMETRY_XVYCC709`, `ERROR_NONE`
- [x] 5.6 Add test case: display connected, `getMatrixCoefficients()` returns `dsDISPLAY_MATRIXCOEFFICIENT_eXvYCC_601` → property returns `COLORIMETRY_XVYCC601`, `ERROR_NONE`
- [x] 5.7 Add test case: display connected, `getMatrixCoefficients()` returns unknown/unmapped value → property returns `COLORIMETRY_UNKNOWN`, `ERROR_NONE`
- [x] 5.8 Add test case: no display connected (`isDisplayConnected()` returns false) → property returns `COLORIMETRY_UNKNOWN`, `ERROR_NONE`
- [x] 5.9 Add test case: `device::Exception` thrown by `getMatrixCoefficients()` → property returns `COLORIMETRY_UNKNOWN`, `ERROR_NONE`
- [x] 5.10 Add test case: Linux backend `CurrentColorimetry()` returns `ERROR_UNAVAILABLE` (N/A — Linux backend doesn't implement `IDisplayProperties`; covered by RPI stub test)
