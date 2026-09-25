## 1. Interface Change (entservices-apis / ThunderInterfaces)

- [x] 1.1 Add `FRAMERATE_CHANGE` to `Exchange::IConnectionProperties::INotification::Source` in `interfaces/IDisplayInfo.h` (tracked in `entservices-apis`, `feature/RDKEMW-24427_2`)
- [x] 1.2 Add `virtual Core::hresult InitializeFrameRate() = 0;` to `Exchange::IConnectionProperties`
- [x] 1.3 Update `build_dependencies.sh` and `.github/workflows/L1-tests.yml` to pull `entservices-apis` from `feature/RDKEMW-24427_2` instead of `develop`

## 2. DeviceSettings Backend Implementation

- [x] 2.1 Add `_frameRateLock` (`Core::CriticalSection`) and `_cachedFrameRate` (`FrameRateType`, default `FRAMERATE_UNKNOWN`) members to `DisplayInfoImplementation`
- [x] 2.2 Implement `IsFrameRateChanged()`: query `FrameRate(newRate)` under `_frameRateLock`, compare against `_cachedFrameRate`, update the cache, return whether it changed
- [x] 2.3 Implement `InitializeFrameRate()` override: query `FrameRate(_cachedFrameRate)` under `_frameRateLock`, wrap in typed exception handlers, return `ERROR_NONE`/`ERROR_GENERAL`
- [x] 2.4 Update `OnResolutionPostChange()` to call `IsFrameRateChanged()` and, if true, call `ResolutionChangeImpl(FRAMERATE_CHANGE)` before the existing `ResolutionChangeImpl(POST_RESOLUTION_CHANGE)` call

## 3. Plugin Initialization

- [x] 3.1 In `DisplayInfo::Initialize`, call `_connectionProperties->InitializeFrameRate()` immediately after acquiring `_connectionProperties`, before `_connectionProperties->Register(&_notification)`

## 4. Linux and RPI Backend Stubs

- [x] 4.1 Add `InitializeFrameRate()` override to `plugin/Linux/PlatformImplementation.cpp` returning `Core::ERROR_NOT_SUPPORTED`
- [x] 4.2 Add `InitializeFrameRate()` override to `plugin/RPI/PlatformImplementation.cpp` returning `Core::ERROR_UNAVAILABLE`

## 5. L1 Test Fixture Fix

- [x] 5.1 Reorder `DisplayInfoTest` constructor in `Tests/L1Tests/tests/test_DisplayInfo.cpp` so `DRMMock`, `AudioOutputPortMock`, `VideoResolutionMock`, `VideoOutputPortMock`, and `VideoDeviceMock` are installed via `setImpl` **before** `dispatcher->Activate(&service)` / `plugin->Initialize(&service)` — `Initialize()` now calls `InitializeFrameRate()` → `FrameRate()`, which dereferences these backends

## 6. L1 Tests

- [x] 6.1 Add `InitializeFrameRate_Success`: mock a connected display with a known frame rate; assert `InitializeFrameRate()` returns `ERROR_NONE` and caches the value
- [x] 6.2 Add `InitializeFrameRate_ExceptionHandling`: mock `getResolution()` throwing `device::Exception`; assert `InitializeFrameRate()` returns `ERROR_GENERAL`
- [x] 6.3 Extend `ResolutionChange_NotificationTest` with a `FRAMERATE_CHANGE` scenario: cache an initial frame rate via `InitializeFrameRate()`, then trigger `OnResolutionPostChange` with an unchanged rate (expect no `FRAMERATE_CHANGE`) and then with a changed rate (expect `FRAMERATE_CHANGE` alongside `POST_RESOLUTION_CHANGE`)

## 7. Spec Updates

- [x] 7.1 Add `FRAMERATE_CHANGE` to the `updated` event `Source` table in `openspec/specs/displayinfo.spec.md`
- [x] 7.2 Document `InitializeFrameRate()` behavior and return codes per backend in `openspec/specs/displayinfo.spec.md`
- [x] 7.3 Add `DisplayInfoImplementation::InitializeFrameRate` / `IsFrameRateChanged` to Covered Code (DeviceSettings, Linux, RPI backends)
- [x] 7.4 Add new L1 test names to the Conformance Testing table
- [x] 7.5 Add a Change History entry for this change

## 8. Final Verification

- [ ] 8.1 Run the full L1 test suite and confirm no regressions
    > **Blocked:** Requires `entservices-testframework` repo and full build environment; run via CI (`.github/workflows/L1-tests.yml`).
- [ ] 8.2 Verify `DisplayInfo.1.updated` emits `FRAMERATE_CHANGE` on a real device when the HDMI sink negotiates a new frame rate
    > **Blocked:** Requires physical hardware with DeviceSettings backend.
