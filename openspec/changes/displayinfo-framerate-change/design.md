## Context

`DisplayInfoImplementation` (DeviceSettings backend, `plugin/DeviceSettings/PlatformImplementation.cpp`) registers with `device::Host` as an `IVideoOutputPortEvents` sink and receives `OnResolutionPreChange`/`OnResolutionPostChange` callbacks whenever the connected display's resolution changes. Today, `OnResolutionPostChange` only forwards a `POST_RESOLUTION_CHANGE` notification to `IConnectionProperties::INotification` observers via `ResolutionChangeImpl`. It does not consider frame rate at all, even though a resolution change frequently also changes the frame rate (e.g. 3840x2160@60 → 3840x2160@50).

`Exchange::IDisplayProperties::FrameRate(FrameRateType&)` is already implemented in this backend: it reads the current `device::VideoResolution` from the active `device::VideoOutputPort` and maps `device::FrameRate` to `Exchange::IDisplayProperties::FrameRateType`. It returns `Core::ERROR_GENERAL` if a `device::Exception` is thrown, and `FRAMERATE_UNKNOWN` (with `ERROR_NONE`) if the display is not connected.

The `Source` enum consumed by `Updated()` (`PRE_RESOLUTION_CHANGE`, `POST_RESOLUTION_CHANGE`, `HDMI_CHANGE`, `HDCP_CHANGE`) is declared in `Exchange::IConnectionProperties::INotification::Source`, which lives in the external `entservices-apis` repository (`apis/DisplayInfo/IDisplayInfo.h`), not in this repository. This design assumes a `FRAMERATE_CHANGE` value is added to that enum in a companion change to `entservices-apis`; only the `entservices-displayinfo` side is designed and implemented here.

Only the DeviceSettings backend is in scope: the Linux backend has no `OnResolutionPostChange`/`OnResolutionPreChange` handling at all, and the RPI backend's `FrameRate()` is an unimplemented stub (`ERROR_UNAVAILABLE`).

## Goals / Non-Goals

**Goals:**
- Cache the current frame rate in `DisplayInfoImplementation` (DeviceSettings backend) at construction time.
- Re-query the frame rate every time `OnResolutionPostChange` fires, compare it to the cached value, and update the cache.
- Emit `Updated(FRAMERATE_CHANGE)` to all registered observers when the queried frame rate differs from the cached value, in addition to the existing `Updated(POST_RESOLUTION_CHANGE)` notification.
- Keep the change scoped to the DeviceSettings backend only.

**Non-Goals:**
- Changing the behaviour or return value of the `FrameRate()` getter itself (it continues to query live hardware on every call; the cache is used only for change detection, not as a read-through cache for the JSON-RPC property).
- Implementing frame-rate caching/notification for the Linux or RPI backends.
- Adding `FRAMERATE_CHANGE` handling to `OnResolutionPreChange` (frame rate is not yet known/stable before the change completes).
- Modifying the `entservices-apis` interfaces repository (tracked as an external dependency, not part of this change's implementation).

## Decisions

### D-01: Cache stored as a plain member variable, updated from the same callback that already handles resolution changes

Add a private member `Exchange::IDisplayProperties::FrameRateType _cachedFrameRate` to `DisplayInfoImplementation`, initialised in the constructor by calling the existing `FrameRate()` method once `device::Manager::Initialize()` has completed. `OnResolutionPostChange` queries `FrameRate()` again, compares the result to `_cachedFrameRate`, updates the member, and — only if the value changed — walks the observer list a second time to call `Updated(FRAMERATE_CHANGE)`.

**Alternative considered:** Compute frame rate lazily inside `ResolutionChangeImpl` for every `Source` value. Rejected — `ResolutionChangeImpl` is also invoked from `OnResolutionPreChange` (`PRE_RESOLUTION_CHANGE`), where the frame rate is not yet meaningful; keeping the frame-rate check local to `OnResolutionPostChange` avoids spurious queries on the pre-change path.

### D-02: No additional locking beyond the existing `_adminLock` around the observer list

`_cachedFrameRate` is only ever written from `OnResolutionPostChange`/the constructor, both of which execute serially on the `device::Host` event-delivery path (the existing code applies the same assumption to `OnResolutionPreChange`/`OnResolutionPostChange` today, with no locking around resolution state). The observer-list traversal that fires `Updated(FRAMERATE_CHANGE)` reuses the existing `_adminLock`-protected traversal pattern already used in `ResolutionChangeImpl`.

**Alternative considered:** Guard `_cachedFrameRate` with a dedicated `Core::CriticalSection`. Rejected as unnecessary — no other thread reads or writes this member, so it would add complexity without closing a real race.

### D-03: Reuse `ResolutionChangeImpl` for both notifications

`OnResolutionPostChange` calls `ResolutionChangeImpl(POST_RESOLUTION_CHANGE)` (unchanged), then performs the frame-rate query/compare, and — if changed — calls `ResolutionChangeImpl(FRAMERATE_CHANGE)` as a second, independent notification. This keeps the observer-notification code path single-sourced rather than duplicating the observer-list walk.

**Alternative considered:** Emit a single combined notification carrying both source values. Rejected — `Updated()` takes one `Source` value per call by contract; two independent calls is consistent with how `PRE_RESOLUTION_CHANGE`/`POST_RESOLUTION_CHANGE` are already delivered as separate events.

### D-04: Constructor-time cache population tolerates hardware unavailability

If `FrameRate()` returns `Core::ERROR_GENERAL` (e.g. `device::Exception` during construction, before a display is connected), `_cachedFrameRate` is still set to the returned value (`FRAMERATE_UNKNOWN` by contract of the existing implementation). This ensures the first real `OnResolutionPostChange` after a display connects is compared against a well-defined baseline (`FRAMERATE_UNKNOWN`) rather than an uninitialised value, and will correctly detect the transition to the real frame rate as a change.

## Risks / Trade-offs

| Risk | Mitigation |
|------|-----------|
| `entservices-apis` does not yet define `FRAMERATE_CHANGE` on `INotification::Source` | Out-of-repo dependency called out explicitly in proposal and spec; this change's code will not compile until that enum value exists upstream |
| Extra `FrameRate()` call on every `OnResolutionPostChange` adds a DeviceSettings library round-trip | Acceptable — `OnResolutionPostChange` is a low-frequency event (display resolution changes), not a hot path |
| Frame rate genuinely unavailable (`ERROR_GENERAL`) immediately after `OnResolutionPostChange` | Cache is still updated to `FRAMERATE_UNKNOWN`; no `FRAMERATE_CHANGE` is emitted only if the value is unchanged from the previous cache, consistent with existing error handling in `FrameRate()` |

## Open Questions

- None — scope is confirmed limited to the DeviceSettings backend, and the `entservices-apis` enum extension is treated as an accepted external dependency.
