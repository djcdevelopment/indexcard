# ADR-0001: Cache DIB and GDI objects in OverlayWindow for zero per-frame allocation

**Status:** Accepted
**Date:** 2026-06-09

## Context

`OverlayWindow::render()` is called at up to 60 Hz in sticky/follow mode. The original implementation allocated a `std::vector<unsigned int>` pixel buffer (~8 MB for 1920x1080), called `CreateDIBSection`, and created 5+ `HFONT` plus multiple `HPEN`/`HBRUSH` objects on every call. This amounted to ~480 MB/s of heap churn plus repeated GDI kernel object allocation at 60 Hz.

The same problem existed in `SelectionCapture::render()`, which allocated a virtual-screen-sized pixel buffer (up to 30 MB on multi-monitor setups) on every `WM_MOUSEMOVE` during a capture session.

## Decision

Cache the DIB (`HDC dibDC_`, `HBITMAP dibBitmap_`, `void* dibBits_`), the pixel buffer (`std::vector<unsigned int> pixelBuf_`), and all GDI objects (`HFONT pillIconFont_`, `HPEN dividerPen_`) as class members. Rebuild only when window dimensions change (`rebuildDib`) or on creation (`rebuildFonts`). In `render()`, use `std::fill` to zero the existing buffer, fill it, `memcpy` into the live DIB bits, then draw with cached objects.

Apply the same pattern to `SelectionCapture`: allocate the buffer in `begin()`, release in `cancel()` and on completion.

## Consequences

- Per-frame allocations eliminated. The 60 Hz sticky path does zero heap allocation.
- `OverlayWindow` now has a proper create/destroy lifecycle that must be respected.
- `destroy()` must clean up all cached objects; `rebuildDib` and `rebuildFonts` must handle the rebuild-on-resize case correctly.
- When `WM_DPICHANGED` fires, `updateWindowSize` triggers `rebuildDib` if dimensions change. Font sizes are currently hardcoded and do not DPI-scale dynamically; this is consistent with the previous behavior.
- `DeleteDC(dibDC_)` is called before `DeleteObject(dibBitmap_)` at cleanup. Win32 documentation recommends deselecting objects before deletion, but there is no `NULL_BITMAP` stock object. `DeleteDC` deselects the bitmap as a side effect; `DeleteObject` then succeeds.
