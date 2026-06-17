# ADR-0006: Follow mode — grab-point anchoring, border handle, single-click detach; drop arrow-resize

**Status:** Accepted
**Date:** 2026-06-17

## Context

ADR-0004 established follow mode as the overlay's single movement story: click the dimmed area to engage, click again to release. Real reading use surfaced three problems with the original implementation:

1. **Corner-pinning.** `updateStickyTracking` repositioned the overlay every tick so the cursor sat at its **bottom-right corner** (`cursor - outerWidth()/outerHeight()`). The user had no control over where the cursor landed relative to the strip, and engaging produced a jarring leap as the corner snapped to the cursor.

2. **The bright border wasn't grabbable.** `hitTestClient` returned `HitTarget::None` (click-through) for the entire selection rectangle, so the visible accent outline — the natural thing to reach for — passed clicks straight through. Only the dimmed margin engaged follow.

3. **Arrow keys double-purposed as a resize gesture.** A `processStickyKeys` poll (`GetAsyncKeyState` on the arrows, run from the follow timer) resized the strip while attached. Because the poll never consumed the keys and the overlay never holds focus, an arrow press meant for the app both reached it and resized the card. The feature was unused in practice; the user's call: "that resize feature shouldn't even be in the codebase — that's a deprecated feature."

## Decision

1. **Anchor to the grab point.** On engage, record the click position in window-client coords (`grabOffsetX_/Y_`); tracking keeps that exact point under the cursor instead of snapping a corner to it. The user chooses where the cursor rides, and the engage-time leap is gone (the captured offset already matches the current cursor relationship).

2. **Make the accent border a grab handle.** `hitTestClient` treats a band of `max(borderWidth, 6)px` inside the strip edges as `HitTarget::Follow`; the clear interior stays `HitTarget::None` (click-through to the app being read). The 6px floor keeps the thin 2px outline hittable.

3. **Single-click detach.** Because the cursor stays glued to the overlay while attached, every detach click lands on the overlay — and was being processed twice (the low-level mouse hook posted a release *and* the window's `WM_LBUTTONDOWN` re-grabbed). The hook now suppresses its release for left-clicks inside the overlay rect, leaving the window's own toggle as the single detach action. Right/middle clicks still release from anywhere as an escape hatch.

4. **Remove the arrow-resize feature entirely.** Delete `processStickyKeys`, `resizeSelection`, and the four key-state flags. Strip dimensions are adjusted via `settings.json` (consistent with ADR-0004) or by redrawing the strip.

5. **Rename the recapture action "Redraw."** The `□` pill button and the tray item both call `beginCapture()` (drag a fresh strip). The stale "Resize Selection" label is renamed to "Redraw" in the tray menu, README, and log to match the pill vocabulary.

The overlay remains hit-test-opaque (`HitTarget::Follow`) while attached, so it owns its own clicks for the detach toggle.

## Rejected alternative — wheel / input pass-through while attached

We explored letting the mouse wheel scroll the reading surface *while the card is attached*, so the user could move the card and scroll without detaching. Three approaches were built and backed out:

1. **Synthetic forward** — intercept `WM_MOUSEWHEEL` and `PostMessage` it to the window beneath (found via a temporary `EnableWindow(FALSE)` + `WindowFromPoint`). Failed: modern targets (Chromium/Electron apps, the WinUI Notepad) ignore posted wheel messages; they read raw pointer input.
2. **`HTTRANSPARENT` while attached** — return `HitTarget::None` so hit-testing falls through. Failed: a layered window does not pass the *wheel* through on the hit-test return value alone.
3. **`WS_EX_TRANSPARENT` while attached** — the full click-through extended style. Also did not produce reliable scrolling in the tested apps, and it changed detach semantics so the drop click also landed on the app.

**Decision: do not pursue pass-through.** It could not be made rock-solid across the apps the user actually reads in, and each variant added input-routing complexity to a tool whose value is being simple and predictable. The supported workflow is two clean modes: **attach to aim** (mouse moves the card), **detach to read** (the card parks; scroll the app natively with the wheel or keyboard through the clear strip / off the overlay). No pass-through code remains in the tree.

## Consequences

- Follow mode is a true grab-and-drag: the point you click stays under the cursor; the corner-snap and engage-leap are gone.
- The outer ~6px ring of the strip interior no longer clicks through (it's the grab handle) — a small reduction in click-through area in exchange for a reachable handle.
- Detach is a single, reliable click; no release-then-regrab lock-in.
- Arrow keys now pass straight to the focused app (the overlay never holds focus). Live strip resizing while attached is gone; resize is via `settings.json` or redraw.
- `CmdResize` (the internal tray command constant) keeps its name; only user-facing strings changed.

## Relationship to ADR-0004

ADR-0004's core decision — the two-button pill and "follow via gesture, not button" — stands. This ADR refines the *behavior* of that follow gesture and renames the `□` action. ADR-0004 is annotated with a pointer here.
