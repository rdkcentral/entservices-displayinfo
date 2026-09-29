## Context

`DisplayInfoImplementation` (DeviceSettings backend) already implements
`OnResolutionPreChange` / `OnResolutionPostChange` as IARM/DS callbacks that fire
`ResolutionChangeImpl(PRE_RESOLUTION_CHANGE)` / `ResolutionChangeImpl(POST_RESOLUTION_CHANGE)`.
`FrameRate(FrameRateType&)` already exists as a read-only getter used by the
`DisplayInfo.framerate` JSON-RPC property; it queries
`device::Host::getInstance().getVideoOutputPort(...).getResolution().getFrameRate()`
and maps the DS enum to `FrameRateType`.

There was previously no mechanism to detect a frame-rate change independent of a full
resolution poll, and no cached "last known" frame rate to diff against.

## Goals / Non-Goals

**Goals:**
- Cache the frame rate once at plugin initialization so the first `OnResolutionPostChange`
  callback has a valid baseline to diff against.
- Detect frame-rate changes as a side effect of the existing resolution-change callback,
  without adding a new polling thread or timer.
- Emit `Updated(FRAMERATE_CHANGE)` only when the frame rate actually differs from the
  cached value — avoid event spam on resolution changes that keep the same frame rate.
- Keep the change backend-scoped: only DeviceSettings implements real frame-rate change
  detection; Linux/RPI stub the new interface method.

**Non-Goals:**
- Implementing frame-rate change detection for the Linux/DRM or BCM/RPI backends.
- Adding a dedicated polling/timer mechanism — detection remains piggy-backed on the
  existing resolution-change callback.
- Changing the `DisplayInfo.framerate` JSON-RPC property's read behavior or return codes.

## Decisions

### D-01: Reuse the existing `FrameRate()` getter for change detection

`IsFrameRateChanged()` calls the same private `FrameRate(FrameRateType&)` used by the
JSON-RPC `framerate` property, rather than duplicating the DS query/mapping logic. This
keeps a single source of truth for frame-rate resolution and mapping.

**Alternative considered:** Read `resolution.getFrameRate()` directly in
`IsFrameRateChanged()`. Rejected — would duplicate the try/catch and enum-mapping logic
already in `FrameRate()`.

### D-02: Cache is populated asynchronously by `CacheInitialFrameRateAsync()`, spawned from the `DisplayInfoImplementation` constructor

Rather than lazily populating the cache on the first `OnResolutionPostChange`, the
DeviceSettings backend spawns a background thread from its constructor
(`_frameRateCacheThread = std::thread(&DisplayInfoImplementation::CacheInitialFrameRateAsync, this)`)
that queries `FrameRate()` and seeds `_cachedFrameRate`, retrying once after a fixed
delay if the first attempt fails. The thread is kept joinable (not detached): the
destructor joins it before any other teardown proceeds, so it can never touch `this`
after destruction has started.
This guarantees:
- The cache is populated best-effort even though `device::Manager::Initialize()` can
  return before the underlying HAL is fully ready — the retry absorbs that race
  without blocking plugin `Initialize()`.
- No public interface method is required, since the caching is purely an
  implementation detail of the DeviceSettings backend and does not need to be invoked
  from `DisplayInfo::Initialize` or exposed on `Exchange::IConnectionProperties`.

**Alternative considered (originally implemented):** A dedicated
`Exchange::IConnectionProperties::InitializeFrameRate()` interface method, called
synchronously by `DisplayInfo::Initialize` before registering notification observers.
Superseded — it required an interface change in `entservices-apis` and stub overrides
on every backend, and still raced the HAL if `device::Manager::Initialize()` hadn't
fully come up by the time `Initialize()` ran. The async, backend-internal approach
removes both problems without widening the public interface.

**Alternative considered:** Lazily initialize the cache on first use inside
`IsFrameRateChanged()` (treat `FRAMERATE_UNKNOWN` cache as "no baseline, skip compare").
Rejected — adds an extra branch/flag to every callback for a case fully solved once at
startup.

### D-03: `FRAMERATE_CHANGE` is emitted before `POST_RESOLUTION_CHANGE`

`OnResolutionPostChange` calls `ResolutionChangeImpl(FRAMERATE_CHANGE)` first (only if
changed), then unconditionally calls `ResolutionChangeImpl(POST_RESOLUTION_CHANGE)`.
This lets clients that only care about frame rate react before the more general
resolution-change notification, and matches the existing pattern of firing multiple
`Updated` events for a single hardware callback (see `HDMI_CHANGE`/`HDCP_CHANGE`
combinations elsewhere in the implementation).

### D-04: Frame-rate cache and lock are separate from the existing `_adminLock`/`_observers` state

A dedicated `_frameRateLock` (`Core::CriticalSection`) guards `_cachedFrameRate`,
distinct from `_adminLock` which guards the observer list. This avoids holding the
observer lock while making a (potentially blocking) DeviceSettings library call inside
`FrameRate()`.

### D-05: Linux and RPI backends do not implement frame-rate caching

Only the DeviceSettings backend caches and diffs the frame rate; there is no
interface method for Linux/RPI to stub, so no changes are required in those backends
for this feature.

## Risks / Trade-offs

| Risk | Mitigation |
|------|-----------|
| `CacheInitialFrameRateAsync()` runs on a background thread started from the constructor, before test/mock backends may be wired up (L1 fixture ordering) | L1 fixture reordered so `VideoOutputPort`/`VideoResolution`/other device mocks are installed via `setImpl` before `dispatcher->Activate()`/`plugin->Initialize()` |
| `FrameRate()` can throw/return an error if the DS library call fails | `IsFrameRateChanged()` wraps the call in the same try/catch as `FrameRate()`; on failure `newRate` stays `FRAMERATE_UNKNOWN`, which is compared and cached like any other value; `CacheInitialFrameRateAsync()` retries once after a fixed delay before giving up |
| False-positive `FRAMERATE_CHANGE` on repeated `OnResolutionPostChange` calls with an unchanged rate | Comparison against `_cachedFrameRate` under `_frameRateLock` prevents duplicate notifications |
| **(Found via L1 testing)** A detached background thread can outlive the `DisplayInfoImplementation` object that spawned it (e.g. if the object is destroyed while the thread is still sleeping/querying), causing a use-after-free when the thread later touches `this` or backend singletons the object depended on | `_frameRateCacheThread` is kept joinable (not detached); the destructor joins it before any other teardown proceeds, so the thread can never run after the object starts being destroyed. This can block destruction for up to ~2s if torn down while the thread is still sleeping/retrying — accepted, since this window is only realistically hit in fast construct/destroy test cycles, not real device operation |

## Open Questions

- **OQ-01:** Should `CacheInitialFrameRateAsync()`'s failure be surfaced anywhere
  beyond a log line (e.g. a diagnostic property), or remain silently best-effort as it
  is today? Currently silent/best-effort.
- **OQ-02:** Should Linux/DRM and BCM/RPI backends eventually implement real frame-rate
  change detection (via DRM mode-set callbacks / `vc_dispmanx` respectively)? Tracked as
  a future change; out of scope here.
