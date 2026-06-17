# Retro: Follow mode reworked — grab-point anchoring, single-click detach, arrow-resize removed

*Follow mode became a true grab-and-drag: the strip sticks to the exact point you grab, a single click drops it, and the deprecated arrow-key resize gesture is gone. A wheel/input pass-through capability was explored three ways and fully backed out — it could not be made rock-solid, and the supported model is two clean modes instead.*

*Date: 2026-06-17 · Scope: uncommitted + commits 6cb96f1, 2ee74a3 since 46cbb7b*

---

## What shipped

Three changes, all driven by the operator's own day-to-day reading use:

1. **Grab-point anchoring** (`OverlayWindow.cpp` `updateStickyTracking` + `grabOffsetX_/Y_`). Tracking previously set the overlay so the cursor sat at its bottom-right corner (`cursor - outerWidth()/outerHeight()`). Now it records the click position in window-client coords on engage and keeps that exact point under the cursor. The user picks where the pointer rides; the engage-time leap is gone.
2. **Single-click detach** (`StickyMouseProc`). The low-level mouse hook suppresses its release for left-clicks landing inside the overlay rect, so the on-overlay detach click is processed once (not hook-release-then-window-re-grab). Fixed a hard lock-in where, once the cursor was glued to the overlay, there was no way to release.
3. **Border-as-handle + arrow-resize deleted** (`hitTestClient`; `processStickyKeys`/`resizeSelection`/four key-state flags removed). A `max(borderWidth, 6)px` band inside the strip edges is now a follow handle; the clear interior stays click-through. Plain arrows pass straight to the focused app. Tray/README/log wording "Resize Selection" → "Redraw."

What did **not** ship: **wheel / input pass-through while attached.** Explored, built three ways, and reverted (see the dedicated section below). No pass-through code remains in the tree.

Builds clean (`MSBuild … /p:Configuration=Release`) → `D:\work\reader\build\Release\IndexCard.exe`. ADR-0006 records the shipped decisions and the rejected pass-through alternative; ADR-0004 annotated as refined-by-0006.

---

## Engineering Lead perspective

The shipped core is small and solid. `updateStickyTracking` is a two-line swap (constant corner offset → captured grab offset) with an outsized payoff: snap-to-corner becomes drag-by-point, and the engage-time jump disappears because the captured offset already equals the current cursor relationship. The detach lock-in was a clean example of one fix exposing the next: once anchoring kept the cursor on the overlay, every release click landed on it, making a latent double-handling visible (hook posts `StickyReleaseMessage`; the same click also arrives as `WM_LBUTTONDOWN`). Scoping the hook's release to off-overlay clicks and letting the window own on-overlay detach resolved it. These three changes are the kind of thing that should be in the tree: minimal, traceable, and each tied to a concrete behavior.

The pass-through work is the cautionary half. Three escalating attempts, all reverted: (1) synthetic `PostMessage(WM_MOUSEWHEEL)` to the window beneath — ignored by Chromium/Electron and the WinUI Notepad, which read raw input; (2) returning `HTTRANSPARENT` while attached — a layered window doesn't pass the *wheel* on the hit-test return alone; (3) `WS_EX_TRANSPARENT` full click-through — still didn't scroll reliably in the tested apps, and it changed detach semantics so the drop click also hit the app. None reached "rock-solid," and each added input-routing complexity to a tool whose whole value is being predictable. The right call was to back all of it out and keep the two-mode model: attach to aim, detach to read.

Net code health after the backout is good: the keyboard-polling path is gone, the grab-anchor and detach paths are single-purpose, and no speculative input-forwarding code lingers to imply behavior that doesn't hold.

---

## Project / Program Manager perspective

Scope was emergent and reading-driven — each shipped change traces to a concrete annoyance. The notable event this session is a **scope reversal**: pass-through was pursued across several iterations, then cut entirely when it became clear it couldn't be made reliable across the operator's real apps. That's a healthy outcome, not a failure — the cost was a few build-test cycles, and the alternative (shipping flaky scroll behavior into a tool meant to be solid) would have been worse. The decision and its rationale are now captured in ADR-0006 so the same swamp isn't re-entered.

The deferred release queue from the 2026-06-09 retro is untouched and remains the real backlog: push a `v0.1-test` tag to exercise CI release, SignPath application, clean-machine installer test, first GitHub Release, the deferred `SplashWindow WM_CLOSE` gap, and the custom tray icon. New risk introduced: none that survived (pass-through reverted). New risk retired: follow-mode corner-pinning that interfered with reading.

---

## QA / Verification perspective

Verification was build-clean plus operator smoke. The shipped behaviors — grab anchoring, single-click detach, arrow keys no longer resizing — are mechanical and were exercised by the operator in real reading. The pass-through attempts were each smoke-tested against real targets (Notepad, Claude desktop), and that testing is exactly what killed them: the operator reported "scrolling doesn't work" and later "the cursor was locked directly where I clicked." Those negative results were the decisive evidence — a good reminder that a quick real-app smoke beats reasoning about input routing in the abstract.

Worth a hands-on pass before tagging: confirm the detach toggle is single-click reliable on the strip border, and that the 6px grab band catches clicks without making the clear interior feel sticky. Not covered, and acceptable for a solo tool at this stage: any automated test — the app has no harness, consistent with prior retros.

---

## Operator perspective

The shipped three are exactly what I wanted from reading with the thing: the card sticks where I grab it, one click drops it, and arrows don't fight me anymore. The corner-pinning was a real-use annoyance, not a theoretical one, and handing cursor placement back to me was the fix.

The pass-through detour is the honest lesson of the session. I liked the idea — scroll the page while the card follows — and we chased it hard: synthetic forwarding, then transparency, then full click-through. Each one looked plausible and none of them actually scrolled the apps I read in. At some point I clocked that we'd pivoted into vibe-coding on a piece of the tool I need to be rock-solid, and called it: back out the wheel and all the pass-through, keep only the three changes that are proven. That's the right instinct for this project — it's a small, dependable utility, not a place to carry clever-but-flaky input plumbing.

---

## How we worked together (human ↔ AI)

### What worked well

- **The AI traced each behavior to the exact line before changing it** — corner-pinning to `updateStickyTracking`, the detach lock-in to the hook/`WM_LBUTTONDOWN` double-handling. That made the shipped fixes self-evidently correct.
- **It surfaced the "fixed A exposed B" detach bug proactively** rather than letting it bite, and explained why it manifested only after anchoring glued the cursor to the overlay.
- **It accepted the backout cleanly and completely** — reverted the three pass-through experiments, deleted the dead forward code, and corrected the README/ADR/retro to match reality, including renaming the ADR and retro off the now-false "wheel-passthrough" slug.

### What didn't

- **We over-invested in pass-through before smoke-testing the first approach.** The synthetic-forward limitation (raw-input apps ignore posted wheel messages) was knowable up front and even flagged as "best-effort" when first built — yet we shipped it, then escalated to two more variants, before the real-app test settled it. The cheaper path was: build the simplest version, smoke it in the actual target apps immediately, and let that gate any further effort.
- **"Best-effort" got treated as good enough for a tool that needs to be solid.** The first wheel-forward was committed with a caveat instead of being gated behind a real-surface test. For this project, "best-effort input routing" should have been a stop sign, not a footnote.

### Patterns to repeat

- Trace behavior to the line, explain the mechanism, then change it.
- When the operator says "back this out," revert completely and reconcile the docs in the same pass — no half-reverted state, no stale navigation surfaces.

### Patterns to change

- **Smoke-test the risky mechanism in the real target apps before building variant two.** A negative result on the first attempt should gate the rest.
- **Treat "best-effort" / "works in most apps" as a stop sign on this project**, not a shippable state. Rock-solid or not at all.

---

## Lessons learned

1. **The best fix often removes a decision the tool was making for the user.** Corner-pinning was the app deciding cursor placement; grab-point anchoring hands that choice back — less code, more control.
2. **Fixing a constraint can expose a latent bug downstream.** Anchoring the cursor to the overlay made a pre-existing double-handling reachable on every click. Removing slack from a system surfaces the bugs the slack was hiding.
3. **Real-app smoke beats input-routing theory.** Three plausible pass-through designs all died on contact with the actual apps. Test the mechanism where it has to work, first.
4. **Know when you've pivoted from building to vibe-coding — especially on code that must be solid.** Clever input plumbing that can't be made reliable is worse than not having the feature. Cut it and keep the dependable core.
5. **A backout isn't done until the docs match.** README, ADR, and retro all had to be reconciled so nothing claims the reverted feature shipped.

---

## Next moves

- **Hands-on confirm** the shipped three feel right: grab anchoring, single-click detach on the border, arrows-as-passthrough (no resize). Tune `GrabBand` (6px) if the clear interior feels sticky.
- **Unchanged release queue** (from 2026-06-09 retro): push `v0.1-test` tag to validate CI release; SignPath Foundation application; clean-machine installer test; first GitHub Release; optional winget manifest.
- **Still deferred:** `SplashWindow WM_CLOSE` (Alt-F4 skips `dismissed_()`); custom tray icon (still `IDI_APPLICATION`); optional `CmdResize` → `CmdRedraw` internal rename.

---

## Acceptance gates met

- [x] Follow mode anchors to the grab point instead of snapping the cursor to the corner
- [x] Strip border is a grabbable follow handle; clear interior stays click-through
- [x] Detach is a clean single click; no release-then-regrab lock-in
- [x] Arrow-resize feature removed from the codebase (`processStickyKeys`, `resizeSelection`, key-state flags)
- [x] "Resize Selection" wording renamed to "Redraw" in tray menu, README, and log
- [x] Wheel / input pass-through fully backed out; no pass-through code remains
- [x] Docs reconciled (README, ADR-0006, retro) — nothing claims pass-through shipped
- [x] Release build compiles clean
- [ ] Shipped three smoke-confirmed in a final hands-on pass — pending
- [ ] `GrabBand` (6px) validated against real strip heights — deferred
