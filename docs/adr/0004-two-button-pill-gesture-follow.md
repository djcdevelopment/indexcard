# ADR-0004: Two-button pill toolbar; follow mode via gesture, not button

**Status:** Accepted (follow-gesture behavior and `□` label refined by ADR-0006, 2026-06-17)
**Date:** 2026-06-09

## Context

The overlay toolbar originally had 5 buttons: Move (☰), Follow (◎), Redraw (↖), Tune (≡), and Hide (✕). Three concerns:

1. **Move button** — drag-to-reposition via a handle. But follow mode already repositions the overlay by tracking the cursor. Having both was redundant.
2. **Follow button** — toggled follow mode explicitly. But clicking anywhere on the dimmed overlay area was already wired to `setSticky(true)` via `hitTestClient` returning `HitTarget::Follow` for the dimmed region. The button was a visible hint to a gesture that was discoverable on its own.
3. **Tune button + slider panel** — 5 in-app sliders for vertical margin, horizontal margin, opacity, selection height, and selection width. Complex to render (required separate `slidersZoneRect`, `sliderTrackRect`, per-frame knob position calculation), and the target audience is developer-adjacent users comfortable editing a JSON settings file.

The product owner's judgment: "this product should be so obvious you don't need words."

## Decision

Reduce the pill to 2 buttons: `□` (redraw/resize selection) and `✕` (hide). Remove the Move button and all drag-to-move code (`draggingMove_`, `dragStart_`, `dragWindowStart_`, `SetCapture` for drag). Remove the Follow button — follow mode remains accessible via click-on-overlay gesture. Remove the Tune button and the entire slider panel.

Follow mode interaction: click anywhere on the dimmed overlay area → overlay enters follow mode. Click anywhere → exits follow mode. This is the complete movement story.

Settings adjustment: edit `%APPDATA%\IndexCard\settings.json` directly and restart.

## Consequences

- ~200 lines of dead code removed: `Slider` enum, `HitTarget::Move/Tune/Slider`, `updateSliderFromPoint`, `slidersZoneRect`, `sliderTrackRect`, `hintLabelRect`, `overlayText`, `overlayMono`, `tuneOpen_`, drag state, 3 cached GDI objects.
- `hitTestClient` collapsed from 50 lines to 5.
- Non-technical users have no in-app way to adjust margins or opacity. This is an explicit tradeoff: simplicity for the common case, `settings.json` for power users.
- Follow mode discoverability relies on users trying to click the overlay. The splash screen copy ("Drag a reading strip") and the lack of other obvious interaction points make this reasonably discoverable.
- The `□` icon renders from Segoe UI's Unicode block. On systems where Segoe UI lacks U+25A1, fallback rendering applies. Acceptable for the target platform (Windows 10+, where Segoe UI coverage is broad).
