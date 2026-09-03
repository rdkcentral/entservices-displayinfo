## Why

The `DisplayInfo` plugin's `updated` event reports resolution and connection-state changes (`PRE_RESOLUTION_CHANGE`, `POST_RESOLUTION_CHANGE`, `HDMI_CHANGE`, `HDCP_CHANGE`), but does not report when the connected display's frame rate changes as a result of a resolution change. Clients that need to react to frame-rate transitions (e.g. media pipelines re-syncing playback) currently have no signal to subscribe to and must poll `Exchange::IDisplayProperties::FrameRate()` after every `POST_RESOLUTION_CHANGE`, which is wasteful and error-prone.

## What Changes

- Extend the `Exchange::IConnectionProperties::INotification::Updated` notification with a second parameter, `bool isFrameRateChanged`, alongside the existing `Source event` parameter. **BREAKING**: this method is declared in the external `entservices-apis` interfaces repository (and its generated JSON-RPC binding `JConnectionProperties::Event::Updated`) and must be extended there before this signature can be used here; this change assumes that parameter is available and updates every local caller/overrider to match.
- On `DisplayInfoImplementation` construction (DeviceSettings backend), query `Exchange::IDisplayProperties::FrameRate()` once and cache the result in a new private member variable.
- On every `OnResolutionPostChange` callback, re-query `FrameRate()` via a new `IsFrameRateChanged()` helper, compare the result against the cached value, update the cache, and pass the comparison result as `isFrameRateChanged` on the existing `Updated(POST_RESOLUTION_CHANGE, isFrameRateChanged)` call — no separate notification is emitted.
- All other call sites of `Updated()` (DeviceSettings `PRE_RESOLUTION_CHANGE`, Linux and RPI `HDMI_CHANGE`) pass `isFrameRateChanged = false`, since frame rate is not relevant to those events.
- Scope of the frame-rate caching/comparison logic is limited to the DeviceSettings backend (`plugin/DeviceSettings/PlatformImplementation.cpp`) — this is the only backend that implements `OnResolutionPostChange` and a working `FrameRate()`. The Linux and RPI backends only need the signature update to stay compilable against the extended interface.

## Capabilities

### Modified Capabilities

- `displayinfo`: The `updated` event's underlying `Updated()` notification gains a second `isFrameRateChanged` parameter, and the DeviceSettings backend gains frame-rate caching and change-detection behaviour triggered by `OnResolutionPostChange`.

## Impact

- **JSON-RPC surface:** The `updated` event payload gains an `isFrameRateChanged` boolean field (via the extended `Updated()` notification). No changes to the existing `Source` enum values.
- **Cross-repo dependency:** `Exchange::IConnectionProperties::INotification::Updated` and its JSON-RPC binding `JConnectionProperties::Event::Updated` are defined in `entservices-apis` (`apis/DisplayInfo/IDisplayInfo.h`) and its generated `interfaces/json/JConnectionProperties.h`, outside this repository. Extending the signature requires a corresponding change in that repository; this proposal covers only the `entservices-displayinfo` side.
- **Code:** `plugin/DeviceSettings/PlatformImplementation.cpp` — new cached frame-rate member variable, initialization in the constructor, `IsFrameRateChanged()` helper, and updated `ResolutionChangeImpl`/`OnResolutionPostChange`/`OnResolutionPreChange`. `plugin/DisplayInfo.h`, `plugin/Linux/PlatformImplementation.cpp`, `plugin/RPI/PlatformImplementation.cpp` — updated `Updated()` override/call sites to match the new two-parameter signature.
- **Tests:** New L1 test cases covering: initial cache population on construction, `isFrameRateChanged == false` when frame rate is unchanged after `OnResolutionPostChange`, and `isFrameRateChanged == true` when frame rate changes.
- **No breaking changes** to existing `displayinfo` JSON-RPC properties, the `Source` enum, or the `PRE_RESOLUTION_CHANGE`/`POST_RESOLUTION_CHANGE`/`HDMI_CHANGE`/`HDCP_CHANGE` event triggers themselves.

