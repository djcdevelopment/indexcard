# ADR-0006: Follow mode — grab-point anchoring, border handle, wheel pass-through; drop arrow-resize

**Status:** Proposed
**Date:** 2026-06-17

## Context

ADR-0004 established follow mode as the overlay's single movement story: click the dimmed area to engage, click again to release. The original tracking implementation (`updateStickyTracking`) repositioned the overlay every timer tick so the cursor sat at the overlay's **bottom-right corner**:

```cpp
settings_.x = cursor.x - settings_.outerWidth();
settings_.y = cursor.y - settings_.outerHeight();
```

Real-world reading use surfaced three problems with this model:

1. **Corner-pinning fought the page underneath.** With the cursor forced to the lower-right of the overlay, the pointer often landed over a scroll region — a nested/inner scroll container especially — and interfered with the reading surface. The user had no control over where the cursor sat relative to the strip.

2. **The bright border wasn't grabbable.** `hitTestClient` returned `HitTarget::None` (click-through) for the entire selection rectangle, so the visible accent outline — the natural thing to reach for — passed clicks straight through. Only the dimmed margin engaged follow.

3. **Arrow keys double-purposed as a resize gesture.** A `processStickyKeys` poll (`GetAsyncKeyState` on the arrow keys, run from the follow timer) resized the strip while attached: up/down changed height, left/right changed width. Because the poll never consumed the keys and the overlay never holds focus, the arrows *also* reached the focused app — so a keypress meant to scroll the page both scrolled it and resized the card.

The user's framing: "if the cursor click sticks to its point it gives me more power as a user to work around any of those issues." And, on the resize gesture: "that resize feature shouldn't even be in the codebase anymore — that's a deprecated feature."

## Decision

Refine the follow-mode interaction model:

1. **Anchor to the grab point.** On engage, record the click position in window-client coordinates (`grabOffsetX_/Y_`); tracking keeps that exact point under the cursor instead of snapping a corner to it. The user chooses where the cursor rides — and can keep it off scroll regions. This also removes the jarring leap the corner-snap produced at the moment of engaging.

2. **Make the accent border a grab handle.** `hitTestClient` now treats a band of `max(borderWidth, 6)px` inside the strip edges as `HitTarget::Follow`, while the clear interior stays `HitTarget::None` (click-through to the app being read). The 6px floor keeps the thin 2px outline actually hittable.

3. **Single-toggle detach.** Because the cursor now stays glued to the overlay, every detach click lands on the overlay — and was being processed twice (the low-level mouse hook posted a release *and* the window's `WM_LBUTTONDOWN` re-grabbed). The hook now suppresses its release for left-clicks that land inside the overlay rect, leaving the window's own toggle as the single detach action. Right/middle clicks still release from anywhere as an escape hatch.

4. **Forward the mouse wheel.** While attached, the overlay is hit-test-opaque and (with Windows' default "scroll inactive window under pointer") swallows the wheel. `WM_MOUSEWHEEL`/`WM_MOUSEHWHEEL` now forward to the window directly beneath the overlay, found by momentarily disabling the overlay so `WindowFromPoint` returns the nested child actually under the cursor. The reading surface scrolls without leaving follow mode.

5. **Remove the arrow-resize feature entirely.** Delete `processStickyKeys`, `resizeSelection`, and the four key-state flags. Plain arrows now pass straight through to the focused app. Strip dimensions are adjusted via `settings.json` (consistent with ADR-0004's "settings live in JSON" stance) or by redrawing the strip.

6. **Rename the recapture action "Redraw."** The `□` pill button and the tray item both call `beginCapture()` (drag a fresh strip). The stale "Resize Selection" label is renamed to "Redraw" everywhere it surfaces (tray menu, README, log string) to match the pill vocabulary and to stop implying the deleted incremental-resize gesture.

## Consequences

- Follow mode is now a true grab-and-drag: the point you click stays under the cursor. The corner-snap behavior is gone.
- The outer ~6px ring of the strip interior no longer clicks through (it's the grab handle). On a 40px-tall strip that's the top/bottom 6px each — a small reduction in click-through area, in exchange for a reachable handle.
- Wheel forwarding is **best-effort**: `PostMessage(WM_MOUSEWHEEL)` to the child under the cursor works for standard Win32 controls and most apps (browsers, PDF viewers, editors), but apps that consume raw input instead of the window message may not respond. Worth a real-surface smoke test.
- Arrow scrolling depends on the underlying app retaining keyboard focus. Since the overlay is `WS_EX_NOACTIVATE`, clicking it doesn't steal focus, so this normally holds.
- Net code: arrow-resize removal (`processStickyKeys`, `resizeSelection`, 4 flags) is offset by the grab-anchor state, wheel-forward path, and border-handle hit test. `OverlayWindow.cpp` is roughly flat in size but simpler in responsibility — the keyboard-polling path is gone.
- `CmdResize` (the internal tray command constant) keeps its name; only user-facing strings changed. A future cleanup could rename it `CmdRedraw`.

## Relationship to ADR-0004

ADR-0004's core decision — the two-button pill and "follow via gesture, not button" — stands. This ADR refines the *behavior* of that follow gesture and renames the `□` action from "redraw/resize selection" to "Redraw." ADR-0004 is annotated with a pointer here.
