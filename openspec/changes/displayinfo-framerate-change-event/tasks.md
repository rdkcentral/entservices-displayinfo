## 1. Interface Change (entservices-apis / ThunderInterfaces)

- [x] 1.1 Add `FRAMERATE_CHANGE` to `Exchange::IConnectionProperties::INotification::Source` in `interfaces/IDisplayInfo.h` (tracked in `entservices-apis`, `feature/RDKEMW-24427_2`)
- [x] 1.2 ~~Add `virtual Core::hresult InitializeFrameRate() = 0;` to `Exchange::IConnectionProperties`~~ — superseded; no interface method added, see task 2.3
- [x] 1.3 Update `build_dependencies.sh` and `.github/workflows/L1-tests.yml` to pull `entservices-apis` from `feature/RDKEMW-24427_2` instead of `develop`

## 2. DeviceSettings Backend Implementation

- [x] 2.1 Add `_frameRateLock` (`Core::CriticalSection`) and `_cachedFrameRate` (`FrameRateType`, default `FRAMERATE_UNKNOWN`) members to `DisplayInfoImplementation`
- [x] 2.2 Implement `IsFrameRateChanged()`: query `FrameRate(newRate)` under `_frameRateLock`, compare against `_cachedFrameRate`, update the cache, return whether it changed
- [x] 2.3 Implement `CacheInitialFrameRateAsync()`: spawned as a joinable thread from the `DisplayInfoImplementation` constructor; queries `FrameRate(_cachedFrameRate)` under `_frameRateLock`, wraps in typed exception handlers, and retries once after a fixed delay if the first attempt fails (replaces the originally-planned `InitializeFrameRate()` interface method)
- [x] 2.4 Update `OnResolutionPostChange()` to call `IsFrameRateChanged()` and, if true, call `ResolutionChangeImpl(FRAMERATE_CHANGE)` before the existing `ResolutionChangeImpl(POST_RESOLUTION_CHANGE)` call
- [x] 2.5 **(Bugfix)** Fix use-after-free: keep `_frameRateCacheThread` joinable (not detached); in `~DisplayInfoImplementation()` join the thread before any other teardown, so it can never touch `this` after destruction has started

## 3. Plugin Initialization

- [x] 3.1 ~~In `DisplayInfo::Initialize`, call `_connectionProperties->InitializeFrameRate()`~~ — not needed; caching is triggered automatically by the `DisplayInfoImplementation` constructor via `CacheInitialFrameRateAsync()`

## 4. Linux and RPI Backend Stubs

- [x] 4.1 ~~Add `InitializeFrameRate()` override to `plugin/Linux/PlatformImplementation.cpp`~~ — not needed; no interface method exists
- [x] 4.2 ~~Add `InitializeFrameRate()` override to `plugin/RPI/PlatformImplementation.cpp`~~ — not needed; no interface method exists

## 5. L1 Test Fixture Fix

- [x] 5.1 Reorder `DisplayInfoTest` constructor in `Tests/L1Tests/tests/test_DisplayInfo.cpp` so `DRMMock`, `AudioOutputPortMock`, `VideoResolutionMock`, `VideoOutputPortMock`, and `VideoDeviceMock` are installed via `setImpl` **before** `dispatcher->Activate(&service)` / `plugin->Initialize(&service)` — `Initialize()` constructs `DisplayInfoImplementation`, whose constructor spawns `CacheInitialFrameRateAsync()` → `FrameRate()`, which dereferences these backends
- [x] 5.2 **(Bugfix, found via L1)** Diagnosed an L1 segfault plus `getFrameRate()` mock-saturation failures in `ResolutionChange_NotificationTest` to a detached cache thread outliving its `DisplayInfoImplementation` and later touching freed/swapped mocks from prior tests; fixed at the source in task 2.5 rather than in the tests

## 6. L1 Tests

- [x] 6.1 ~~Add `InitializeFrameRate_Success`~~ — removed; no interface method to call directly, covered indirectly via `ResolutionChange_NotificationTest`
- [x] 6.2 ~~Add `InitializeFrameRate_ExceptionHandling`~~ — removed; no interface method to call directly
- [x] 6.3 Extend `ResolutionChange_NotificationTest` with a `FRAMERATE_CHANGE` scenario: seed an initial frame rate (24 fps) by invoking `OnResolutionPostChange` once, then trigger it again with an unchanged rate (expect no `FRAMERATE_CHANGE`) and then with a changed rate (expect `FRAMERATE_CHANGE` alongside `POST_RESOLUTION_CHANGE`)
- [x] 6.4 Hoist `DisplayInfoNotificationHandler` out of `ResolutionChange_NotificationTest` to file scope so it can be shared with a new test, without duplicating the notification-sink boilerplate
- [x] 6.5 Add `FrameRateChange_InitialCacheAsyncAtActivation`: mock a 1080p60 display (`FRAMERATE_60`) before constructing `DisplayInfoImplementation` via `Root<>()`, wait for the constructor-spawned `CacheInitialFrameRateAsync()` thread to cache the baseline, then assert no `FRAMERATE_CHANGE` when `OnResolutionPostChange` reports an unchanged rate and `FRAMERATE_CHANGE` alongside `POST_RESOLUTION_CHANGE` when it changes to `FRAMERATE_50`

## 7. Spec Updates

- [x] 7.1 Add `FRAMERATE_CHANGE` to the `updated` event `Source` table in `openspec/specs/displayinfo.spec.md`
- [x] 7.2 Document `CacheInitialFrameRateAsync()` behavior (async, constructor-spawned, best-effort retry) in `openspec/specs/displayinfo.spec.md`, replacing the retired `InitializeFrameRate()` section
- [x] 7.3 Add `DisplayInfoImplementation::CacheInitialFrameRateAsync` / `IsFrameRateChanged` to Covered Code (DeviceSettings backend only)
- [x] 7.4 Add new L1 test names to the Conformance Testing table; remove the retired `InitializeFrameRate_*` entries
- [x] 7.5 Add a Change History entry for this change
- [x] 7.6 Add `FrameRateChange_InitialCacheAsyncAtActivation` to Conformance Testing and Covered Code in both `openspec/specs/displayinfo.spec.md` and this change's delta spec; add a Change History entry noting the async-activation test and the `DisplayInfoNotificationHandler` hoist

## 8. Final Verification

- [ ] 8.1 Run the full L1 test suite and confirm no regressions
    > **Blocked:** Requires `entservices-testframework` repo and full build environment; run via CI (`.github/workflows/L1-tests.yml`).
- [ ] 8.2 Verify `DisplayInfo.1.updated` emits `FRAMERATE_CHANGE` on a real device when the HDMI sink negotiates a new frame rate
    > **Blocked:** Requires physical hardware with DeviceSettings backend.
