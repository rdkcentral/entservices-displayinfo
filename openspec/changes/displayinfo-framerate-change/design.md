## Context

`DisplayInfoImplementation` (DeviceSettings backend, `plugin/DeviceSettings/PlatformImplementation.cpp`) registers with `device::Host` as an `IVideoOutputPortEvents` sink and receives `OnResolutionPreChange`/`OnResolutionPostChange` callbacks whenever the connected display's resolution changes. Today, `OnResolutionPostChange` only forwards a `POST_RESOLUTION_CHANGE` notification to `IConnectionProperties::INotification` observers via `ResolutionChangeImpl`. It does not consider frame rate at all, even though a resolution change frequently also changes the frame rate (e.g. 3840x2160@60 → 3840x2160@50).

`Exchange::IDisplayProperties::FrameRate(FrameRateType&)` is already implemented in this backend: it reads the current `device::VideoResolution` from the active `device::VideoOutputPort` and maps `device::FrameRate` to `Exchange::IDisplayProperties::FrameRateType`. It returns `Core::ERROR_GENERAL` if a `device::Exception` is thrown, and `FRAMERATE_UNKNOWN` (with `ERROR_NONE`) if the display is not connected.

`Exchange::IConnectionProperties::INotification::Updated(const Source event)` is declared in the external `entservices-apis` repository (`apis/DisplayInfo/IDisplayInfo.h`), along with its generated JSON-RPC binding `Exchange::JConnectionProperties::Event::Updated`. Rather than adding a new `Source` enum value (which was the initial approach considered), this design extends the `Updated()` signature itself with a second parameter, `bool isFrameRateChanged`, so existing `Source` values (in particular `POST_RESOLUTION_CHANGE`) can carry the frame-rate-changed signal without introducing a new enum member. This still requires `entservices-apis` and its generated `JConnectionProperties.h` binding to be updated to the two-parameter signature before this repository's code compiles against them; only the `entservices-displayinfo` side is designed and implemented here.

Only the frame-rate caching/comparison logic is scoped to the DeviceSettings backend: the Linux backend has no `OnResolutionPostChange`/`OnResolutionPreChange` handling at all, and the RPI backend's `FrameRate()` is an unimplemented stub (`ERROR_UNAVAILABLE`). The Linux and RPI backends' existing `Updated()` call sites (`HDMI_CHANGE`) and `DisplayInfo.h`'s `Notification::Updated` override still need to be updated to the new two-parameter signature to remain compilable, passing `isFrameRateChanged = false`.

## Goals / Non-Goals

**Goals:**
- Extend `Exchange::IConnectionProperties::INotification::Updated` with a second `bool isFrameRateChanged` parameter, without introducing a new `Source` enum value.
- Cache the current frame rate in `DisplayInfoImplementation` (DeviceSettings backend) at construction time.
- Re-query the frame rate every time `OnResolutionPostChange` fires (via an `IsFrameRateChanged()` helper), compare it to the cached value, and update the cache.
- Pass `isFrameRateChanged = true` on the existing `Updated(POST_RESOLUTION_CHANGE, ...)` call when the queried frame rate differs from the cached value; pass `false` otherwise.
- Update every other `Updated()` call site (DeviceSettings `PRE_RESOLUTION_CHANGE`, Linux and RPI `HDMI_CHANGE`) and the `Notification::Updated` override in `DisplayInfo.h` to match the new signature, passing `isFrameRateChanged = false`.
- Keep the frame-rate caching/comparison logic scoped to the DeviceSettings backend only.

**Non-Goals:**
- Adding a new `Source` enum value (e.g. `FRAMERATE_CHANGE`) — superseded by the boolean-parameter approach.
- Changing the behaviour or return value of the `FrameRate()` getter itself (it continues to query live hardware on every call; the cache is used only for change detection, not as a read-through cache for the JSON-RPC property).
- Implementing frame-rate caching/notification for the Linux or RPI backends.
- Reporting `isFrameRateChanged` on `PRE_RESOLUTION_CHANGE` (frame rate is not yet known/stable before the change completes) — always `false` there.
- Modifying the `entservices-apis` interfaces repository or its generated `JConnectionProperties.h` binding (tracked as an external dependency, not part of this change's implementation).

## Decisions

### D-01: Extend `Updated()` with a boolean parameter instead of adding a `Source` enum value

`Exchange::IConnectionProperties::INotification::Updated` becomes `Updated(const Source event, const bool isFrameRateChanged)`. `DisplayInfo.h`'s `Notification::Updated` override forwards both parameters to `Exchange::JConnectionProperties::Event::Updated(_parent, event, isFrameRateChanged)`.

**Alternative considered (superseded):** Add a new `FRAMERATE_CHANGE` value to the `Source` enum and emit it as an independent notification. Rejected in favour of the boolean-parameter approach — frame rate changes are only meaningful as a qualifier on `POST_RESOLUTION_CHANGE` (they never occur independently of a resolution change on this backend), so carrying it as a flag on the existing event avoids a second observer-list traversal and keeps `Source` semantics (connection/resolution/HDCP state machine) separate from the frame-rate qualifier.

### D-02: Cache stored as a plain member variable, checked via a dedicated helper

Add a private member `Exchange::IDisplayProperties::FrameRateType _cachedFrameRate` to `DisplayInfoImplementation`, initialised in the constructor by calling the existing `FrameRate()` method once `device::Manager::Initialize()` has completed. A new `IsFrameRateChanged()` helper re-queries `FrameRate()`, compares it against `_cachedFrameRate`, updates the cache when different, and returns whether a change occurred. `OnResolutionPostChange` calls this helper once and passes its result straight through to `ResolutionChangeImpl(POST_RESOLUTION_CHANGE, isFrameRateChanged)`.

**Alternative considered:** Inline the frame-rate query/compare directly in `OnResolutionPostChange`. Rejected — extracting `IsFrameRateChanged()` keeps the callback body focused on event delivery and makes the comparison independently testable.

### D-03: No additional locking beyond the existing `_adminLock` around the observer list

`_cachedFrameRate` is only ever written from `IsFrameRateChanged()` (called from `OnResolutionPostChange`) and the constructor, both of which execute serially on the `device::Host` event-delivery path (the existing code applies the same assumption to `OnResolutionPreChange`/`OnResolutionPostChange` today, with no locking around resolution state). The observer-list traversal in `ResolutionChangeImpl` is unchanged and reuses the existing `_adminLock`-protected pattern, now simply passing `isFrameRateChanged` through to each `Updated()` call.

**Alternative considered:** Guard `_cachedFrameRate` with a dedicated `Core::CriticalSection`. Rejected as unnecessary — no other thread reads or writes this member, so it would add complexity without closing a real race.

### D-04: Constructor-time cache population tolerates hardware unavailability

If `FrameRate()` returns `Core::ERROR_GENERAL` (e.g. `device::Exception` during construction, before a display is connected), `_cachedFrameRate` is still set to the returned value (`FRAMERATE_UNKNOWN` by contract of the existing implementation). This ensures the first real `OnResolutionPostChange` after a display connects is compared against a well-defined baseline (`FRAMERATE_UNKNOWN`) rather than an uninitialised value, and will correctly report `isFrameRateChanged = true` for the transition to the real frame rate.

## Risks / Trade-offs

| Risk | Mitigation |
|------|-----------|
| `entservices-apis` does not yet declare the two-parameter `Updated(Source, bool)` on `INotification`, nor does its generated `JConnectionProperties::Event::Updated` accept the extra argument | Out-of-repo dependency called out explicitly in proposal.md and this design; this repository's code (`DisplayInfo.h`, all `PlatformImplementation.cpp` variants, L1 tests) will not compile until that signature exists upstream |
| Every `Updated()` call site across all backends must be updated in lockstep with the interface change, or the build breaks everywhere (not just DeviceSettings) | All call sites (`plugin/DisplayInfo.h`, `plugin/DeviceSettings/PlatformImplementation.cpp`, `plugin/Linux/PlatformImplementation.cpp`, `plugin/RPI/PlatformImplementation.cpp`) were updated together in this change |
| Extra `FrameRate()` call on every `OnResolutionPostChange` adds a DeviceSettings library round-trip | Acceptable — `OnResolutionPostChange` is a low-frequency event (display resolution changes), not a hot path |
| Frame rate genuinely unavailable (`ERROR_GENERAL`) immediately after `OnResolutionPostChange` | Cache is still updated to `FRAMERATE_UNKNOWN`; `isFrameRateChanged` is only `true` if the value differs from the previous cache, consistent with existing error handling in `FrameRate()` |

## Open Questions

- None — scope is confirmed limited to the DeviceSettings backend for the caching/comparison logic, and the `entservices-apis` signature extension (plus regenerating `JConnectionProperties.h`) is treated as an accepted external dependency.
