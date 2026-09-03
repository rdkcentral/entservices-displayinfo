## 1. Add Frame Rate Cache Member and Constructor Initialisation

- [x] 1.1 In `plugin/DeviceSettings/PlatformImplementation.cpp`, add a private member `Exchange::IDisplayProperties::FrameRateType _cachedFrameRate` to `DisplayInfoImplementation` (spec: "Frame rate is cached on plugin initialisation (DeviceSettings backend)")
- [x] 1.2 In the `DisplayInfoImplementation` constructor, after `device::Manager::Initialize()`, call `FrameRate(_cachedFrameRate)` to populate the cache; tolerate `ERROR_GENERAL`/`FRAMERATE_UNKNOWN` as a valid baseline value (spec: "Frame rate is cached on plugin initialisation (DeviceSettings backend)"; design: D-04)

## 2. Detect and Notify Frame Rate Changes on OnResolutionPostChange

- [x] 2.1 In `OnResolutionPostChange`, after the existing `ResolutionChangeImpl(POST_RESOLUTION_CHANGE)` call, query the current frame rate via `FrameRate(newRate)` (spec: "Frame rate is re-queried and compared on OnResolutionPostChange (DeviceSettings backend)")
- [x] 2.2 Compare `newRate` against `_cachedFrameRate`; if different, update `_cachedFrameRate` and call `ResolutionChangeImpl(FRAMERATE_CHANGE)` (spec: "Frame rate cache is updated on every OnResolutionPostChange"; "FRAMERATE_CHANGE is emitted only when the frame rate changes")
- [x] 2.3 If `newRate` equals `_cachedFrameRate`, update `_cachedFrameRate` to the queried value (no-op value change) and do NOT call `ResolutionChangeImpl(FRAMERATE_CHANGE)` (spec: "Frame rate unchanged — no notification emitted")
- [x] 2.4 Verify `Updated(POST_RESOLUTION_CHANGE)` still fires unconditionally, independent of the frame-rate outcome (spec: "POST_RESOLUTION_CHANGE is still emitted independently")

## 3. External Interface Dependency

- [ ] 3.1 Confirm (or coordinate separately) that `FRAMERATE_CHANGE` is added to `Exchange::IConnectionProperties::INotification::Source` in the `entservices-apis` repository before building this change (spec: "Updated event supports FRAMERATE_CHANGE source"; design: Risks)
    > **Blocked:** Requires a companion change in the separate `entservices-apis` repository, outside this workspace. Code in this repo now references `Source::FRAMERATE_CHANGE` and will not compile until that enum value is added upstream. Tracked as an accepted external dependency in proposal.md and design.md.

## 4. Update Canonical Spec

- [x] 4.1 In `openspec/specs/displayinfo.spec.md`, add `FRAMERATE_CHANGE` to the `updated` event's `Source` enum table (spec: "Updated event supports FRAMERATE_CHANGE source")
- [x] 4.2 Update `## Change History` in `openspec/specs/displayinfo.spec.md` with an entry for this change

## 5. Add L1 Tests

- [x] 5.1 In `Tests/L1Tests/tests/test_DisplayInfo.cpp`, add test `FrameRate_CachedOnConstruction`: verify the cache is populated by an initial `FrameRate()` call at construction (spec: "Frame rate is cached on plugin initialisation")
- [x] 5.2 Add test `FrameRate_Unchanged_NoNotification`: mock `FrameRate()` returning the same value across two `OnResolutionPostChange` calls; assert `Updated(FRAMERATE_CHANGE)` is never invoked (spec: "Frame rate unchanged — no notification emitted")
- [x] 5.3 Add test `FrameRate_Changed_EmitsNotification`: mock `FrameRate()` returning a different value on `OnResolutionPostChange`; assert `Updated(FRAMERATE_CHANGE)` is invoked exactly once and the cache reflects the new value (spec: "Frame rate changed — notification emitted")
- [x] 5.4 Add test `FrameRate_Changed_PostResolutionChangeStillEmitted`: assert `Updated(POST_RESOLUTION_CHANGE)` is invoked on every `OnResolutionPostChange` regardless of frame-rate outcome (spec: "POST_RESOLUTION_CHANGE is still emitted independently")
- [ ] 5.5 Confirm tests build and pass under the `USE_DEVICESETTINGS` build flag
    > **Blocked:** Requires `FRAMERATE_CHANGE` to exist on `Exchange::IConnectionProperties::INotification::Source` in the external `entservices-apis` repository (task 3.1), plus the `entservices-testframework` mocks and full DeviceSettings build environment.

## 6. Final Verification

- [ ] 6.1 Run existing L1 test suite and confirm no regressions
    > **Blocked:** Same build environment/interfaces prerequisite as 5.5.
- [ ] 6.2 Re-run openspec coverage check and confirm the `displayinfo` spec coverage remains intact for the modified `Source` enum and `OnResolutionPostChange`/`FrameRate` methods
