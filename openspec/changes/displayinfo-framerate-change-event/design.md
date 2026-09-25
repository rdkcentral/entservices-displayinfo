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

### D-02: Cache is populated by a dedicated `InitializeFrameRate()` interface method, called during `DisplayInfo::Initialize`

Rather than lazily populating the cache on the first `OnResolutionPostChange`, the cache
is deliberately seeded during plugin initialization via
`_connectionProperties->InitializeFrameRate()`. This guarantees:
- The cache is never compared against its default-constructed value
  (`FRAMERATE_UNKNOWN`) on the first real resolution-change event, which would
  otherwise spuriously fire `FRAMERATE_CHANGE` on the very first callback after startup.
- The cache is ready before any observer registers, since `Initialize()` calls
  `InitializeFrameRate()` before `_connectionProperties->Register(&_notification)`.

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

### D-05: Linux and RPI backends stub `InitializeFrameRate()` with distinct error codes

- Linux/DRM: `Core::ERROR_NOT_SUPPORTED` — the backend has no frame-rate concept wired
  up at all.
- BCM/RPI: `Core::ERROR_UNAVAILABLE` — consistent with the existing pattern for other
  stubbed `IDisplayProperties`/`IConnectionProperties` methods on that backend.

`DisplayInfo::Initialize` ignores the return value of `InitializeFrameRate()` (best
effort, matching the existing pattern for other non-fatal interface calls in
`Initialize`) so plugin activation is never blocked by a backend that doesn't support
frame-rate caching.

## Risks / Trade-offs

| Risk | Mitigation |
|------|-----------|
| `InitializeFrameRate()` runs during `Initialize()`, before test/mock backends may be wired up (L1 fixture ordering) | L1 fixture reordered so `VideoOutputPort`/`VideoResolution`/other device mocks are installed via `setImpl` before `dispatcher->Activate()`/`plugin->Initialize()` |
| `FrameRate()` can throw/return an error if the DS library call fails | `IsFrameRateChanged()` wraps the call in the same try/catch as `FrameRate()`; on failure `newRate` stays `FRAMERATE_UNKNOWN`, which is compared and cached like any other value |
| False-positive `FRAMERATE_CHANGE` on repeated `OnResolutionPostChange` calls with an unchanged rate | Comparison against `_cachedFrameRate` under `_frameRateLock` prevents duplicate notifications |

## Open Questions

- **OQ-01:** Should `InitializeFrameRate()`'s return value be surfaced to
  `DisplayInfo::Initialize`'s `message` (i.e. treated as a soft failure worth logging),
  or remain silently ignored as it is today? Currently ignored, consistent with other
  best-effort calls in `Initialize`.
- **OQ-02:** Should Linux/DRM and BCM/RPI backends eventually implement real frame-rate
  change detection (via DRM mode-set callbacks / `vc_dispmanx` respectively)? Tracked as
  a future change; out of scope here.
