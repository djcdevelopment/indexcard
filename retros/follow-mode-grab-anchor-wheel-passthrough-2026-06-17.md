# Retro: Follow mode reworked — grab-point anchoring, wheel pass-through, arrow-resize removed

*A reading-driven overhaul of follow mode: the strip now sticks to the exact point you grab instead of snapping a corner to the cursor, the mouse wheel passes through to the page underneath, and the deprecated arrow-key resize gesture is deleted. Driven entirely by the operator's own day-to-day reading use.*

*Date: 2026-06-17 · Scope: uncommitted changes since commit 46cbb7b*

---

## What shipped

Follow mode — the overlay's whole movement story per ADR-0004 — was reworked from a corner-pinned tracker into a true grab-and-drag, plus the reading surface underneath is no longer blocked from scrolling.

Four behavioral changes and a wording cleanup, all in the working tree (to be committed as part of this retro):

1. **Grab-point anchoring** (`OverlayWindow.cpp` `updateStickyTracking` + new `grabOffsetX_/Y_`). Tracking previously set the overlay so the cursor sat at its bottom-right corner (`cursor - outerWidth()/outerHeight()`). Now it records the click position in window-client coords on engage and keeps that exact point under the cursor. The user picks where the pointer rides relative to the strip.
2. **Border-as-handle** (`hitTestClient`). A `max(borderWidth, 6)px` band inside the strip edges is now a follow handle; the clear interior stays click-through. The bright outline you instinctively reach for is finally grabbable.
3. **Single-toggle detach** (`StickyMouseProc`). The low-level mouse hook now suppresses its release for left-clicks landing inside the overlay rect, so the detach click isn't processed twice (hook-release then window-re-grab). This fixed a hard lock-in: once the cursor was glued to the overlay, there was no way to release.
4. **Mouse-wheel pass-through** (`WM_MOUSEWHEEL`/`WM_MOUSEHWHEEL` + new `windowBelow`). The overlay forwards the wheel to the nested child directly beneath it (found by momentarily disabling the overlay so `WindowFromPoint` skips it). Scroll the page without leaving follow mode.
5. **Arrow-resize deleted** (`processStickyKeys`, `resizeSelection`, four key-state flags removed). Plain arrows pass straight through to the focused app. Tray/README/log wording "Resize Selection" → "Redraw" to match the `□` pill and stop implying the deleted gesture.

Builds clean (`MSBuild … /p:Configuration=Release`) → `D:\work\reader\build\Release\IndexCard.exe`. ADR-0006 drafted (Proposed) capturing the refined interaction model; ADR-0004 annotated as refined-by-0006.

---

## Engineering Lead perspective

The center of gravity is `updateStickyTracking`, and the change is a two-line swap with an outsized behavioral payoff: replacing the constant corner offset (`outerWidth()/outerHeight()`) with a captured grab offset turns a snap-to-corner tracker into a drag-by-point tracker. It also incidentally removed the engage-time jump — under the old model the strip leapt up-left the instant you clicked, because the corner had to reach the cursor; under the new model the captured offset *is* the current cursor relationship, so nothing moves until you do.

The non-obvious bug was the detach lock-in, and it's a good example of one fix exposing the next. Once grab-point anchoring kept the cursor glued to the overlay, every release click necessarily landed on the overlay — which made a latent double-handling visible: the `WH_MOUSE_LL` hook posts `StickyReleaseMessage` on any down-click, and the same physical click also arrives at the window as `WM_LBUTTONDOWN`. Release-then-regrab netted to "still attached." The fix scopes the hook's release to clicks *outside* the window rect (`GetWindowRect` + `PtInRect`) and lets the window's own toggle own on-overlay detach. Right/middle clicks still post release unconditionally as an escape hatch — cheap insurance.

The wheel pass-through is the one piece I'd flag as architecturally soft. `windowBelow` disables the overlay, calls `WindowFromPoint`, re-enables — a well-worn trick to see through a topmost window and reach the nested child, which is what nested-scroll needs. But the delivery is `PostMessage(WM_MOUSEWHEEL)`, which is best-effort: most Win32 controls and mainstream apps honor it, but anything reading raw input won't. We made a deliberate scope cut here — the operator initially asked about both wheel and arrow pass-through, then said "let's just stay with mousewheel," which kept us out of the swamp of synthesizing focus/key input into arbitrary apps.

Net code health is good: the keyboard-polling path (`processStickyKeys` + `resizeSelection` + four flags) is gone, and what replaced it is single-purpose. `hitTestClient` grew a small border-band branch but stayed readable. No new per-frame allocation; the grab offsets are two ints.

---

## Project / Program Manager perspective

Scope was emergent, not planned — this came straight out of the operator using the tool to read and hitting friction. That's the healthiest kind of scope: each change traces to a concrete annoyance ("forcing it to the lower right interferes with scroll," "no way to click off to release," "let me scroll the page while the card follows"). Nothing speculative shipped.

One mid-stream scope cut is worth recording: arrow-key pass-through was explicitly dropped in favor of wheel-only, and the arrow-resize feature was deleted outright rather than rebound (the operator considered Shift+arrows and chose "drop it entirely — deprecated"). That's a scope *reduction* decision made for the right reason — the feature wasn't earning its complexity, and keeping a half-disabled version would have been worse than removing it.

The deferred queue from the 2026-06-09 retro is untouched and remains the real backlog: push a `v0.1-test` tag to exercise the CI release workflow, SignPath application, clean-machine installer test, first GitHub Release, the deferred `SplashWindow WM_CLOSE` gap, and the custom tray icon. New risk introduced this session: wheel-forward reliability across apps (see QA). New risk retired: the follow-mode corner-pinning that interfered with reading surfaces.

---

## QA / Verification perspective

Verification this session was build-clean plus code reasoning, not end-to-end smoke. Each rebuild compiled (the only build hiccups were `LNK1104` from the running exe holding the binary — resolved by stopping the process before linking, now a known step). The behavioral claims are traced through the code, not observed in a recorded run.

The regression surface that genuinely needs a real-world smoke is **wheel pass-through**, precisely because it's best-effort. The transferable test: attach the card over a few representative reading surfaces — a browser with nested scroll containers (the operator's original pain point), a PDF viewer, a plain text editor — and confirm the wheel scrolls the inner container, not just the outer window, and nothing scrolls the overlay. If an app doesn't respond, that's expected (raw-input apps), and the user still has arrow keys and the app's own scrollbar.

Two more worth a hands-on pass: (1) the **detach toggle** — engage on the border, move, click once to drop; confirm it releases on the first click and doesn't re-grab. (2) The **border handle hittability** — confirm the 6px band actually catches clicks on a thin 2px border without making the clear interior feel "sticky." Not covered, and acceptable for a solo tool at this stage: any automated test. The app has no test harness; this is consistent with prior retros and is a known, accepted gap rather than an oversight.

---

## Operator perspective

This all came out of actually reading with the thing. The corner-pinning bugged me in practice, not in theory — forcing the cursor to the lower-right kept colliding with scroll regions, and nested scrolling made it worse. The insight was that I didn't need the tool to be smarter about avoiding scroll zones; I needed it to *stop deciding for me* where the cursor goes. If the click sticks to its point, I can place the cursor wherever it won't fight the page. That's more power with less machinery.

The detach bug was a classic "fixed one thing, broke the adjacent thing" — once the cursor stuck to where I grabbed, I couldn't click off the overlay to let go, because the overlay was always under my cursor. Right call to make the second on-overlay click mean "drop it here."

On the arrow-resize: I didn't want it rebound to Shift+arrows or anything clever. It's a deprecated feature — it shouldn't be in the codebase at all. Reading is the job; resizing live while attached was a gimmick I never used. Cutting it made arrows free to scroll, which is what I actually want. And I caught myself almost over-scoping with arrow-key pass-through too — pulled back to wheel-only because that's the 90% case and the arrow path opens a can of worms. Keep it simple; the product should be obvious.

---

## How we worked together (human ↔ AI)

### What worked well

- **The AI traced the corner-pinning to the exact two lines before proposing anything.** When I described the lower-right friction, it found `updateStickyTracking`'s `cursor - outerWidth()/outerHeight()` and explained *why* that produced the behavior I disliked — so the fix (swap to a captured grab offset) was obviously correct rather than a guess.
- **It predicted the detach lock-in's root cause precisely** — the hook-release-then-window-regrab double-handling — and explained why it would manifest *now* (cursor glued to overlay → every click lands on it) even though the same latent double-toggle existed before. That's the kind of "fixed A surfaces B" reasoning I want surfaced, not papered over.
- **It asked exactly one well-scoped question at the one real fork** (where to put arrow-resize: Shift+arrows / drop entirely / only free up/down) instead of guessing or asking five things. I picked "drop entirely," it dropped it entirely.
- **It honored the scope cut cleanly.** When I said "let's just stay with mousewheel," it didn't re-litigate or half-build the arrow path — it confirmed the wheel was already done and removed the rest.
- **It flagged best-effort honestly.** The wheel-forward limitation (raw-input apps won't respond) and the focus dependency for arrows were called out as caveats up front, not buried — so I know what to smoke-test.

### What didn't

- **No end-to-end verification happened in-session.** Everything is build-clean + reasoning. For a behavioral change this tactile — grab feel, scroll pass-through across real apps — that's a gap I'm carrying into the next sitting rather than one we closed.
- **The running-exe link failures (`LNK1104`) recurred twice** before "stop the process first" became routine. Minor, but it's the same friction each build; could be a pre-build kill step.
- **The grab-band size (6px) is an unvalidated guess.** It's a reasonable default, but nobody has confirmed it feels right on a 2px border at the strip heights I actually use.

### Patterns to repeat

- Trace the offending behavior to the specific line and explain the mechanism *before* proposing a fix — it makes the fix self-evidently right.
- One sharp question at a genuine fork beats either guessing or a questionnaire.
- Call out best-effort/edge-case limitations at the moment of building, so the smoke-test list writes itself.

### Patterns to change

- Add a real hands-on smoke for tactile changes before the retro, not after. A two-minute "attach, scroll a browser, detach" pass would have converted three "needs verification" notes into "verified."
- Make "stop IndexCard.exe before building" a standing pre-build step to kill the `LNK1104` recurrence.

---

## Lessons learned

1. **The best fix often removes a decision the tool was making for the user.** Corner-pinning was the app being "helpful" about cursor placement; grab-point anchoring just hands that choice back. Less code, more user power.
2. **Fixing a constraint can expose a latent bug downstream.** The detach lock-in existed in latent form before, but anchoring the cursor to the overlay made it reachable on every click. When you remove slack from a system, pre-existing double-handling stops being harmless.
3. **Deleting a deprecated feature beats rebinding it.** Arrow-resize wasn't earning its keyboard real estate or its `GetAsyncKeyState` poll. Cutting it freed the arrows for the thing the user actually wanted (scroll) and shrank the surface.
4. **Know where the swamp is and stop at its edge.** Forwarding a wheel message is tractable; synthesizing arbitrary key/focus input into other apps is not. "Wheel-only" was the right line to draw.
5. **`WindowFromPoint` + temporary self-disable is the canonical see-through-my-overlay move** — and it returns the nested child, which is exactly what nested-scroll forwarding needs.

---

## Next moves

- **Smoke-test wheel pass-through** on a browser (nested scroll), a PDF viewer, and an editor. Confirm inner-container scroll and overlay-doesn't-scroll. (Carries this session's main verification gap.)
- **Confirm the 6px grab band feels right** on the strip heights actually used; tune `GrabBand` if the clear interior feels sticky or the border is hard to catch.
- **Promote ADR-0006 to Accepted** once the above smokes pass. Draft lives at `docs/adr/0006-follow-mode-grab-anchor-wheel-passthrough.md`.
- **Unchanged release queue** (from 2026-06-09 retro): push `v0.1-test` tag to validate CI release workflow; SignPath Foundation application; clean-machine installer test; first GitHub Release; optional winget manifest.
- **Still deferred:** `SplashWindow WM_CLOSE` (Alt-F4 skips `dismissed_()`); custom tray icon (still `IDI_APPLICATION`); optional `CmdResize` → `CmdRedraw` internal rename.

---

## Acceptance gates met

- [x] Follow mode anchors to the grab point instead of snapping the cursor to the corner
- [x] Strip border is a grabbable follow handle; clear interior stays click-through
- [x] Detach is a clean single click; no release-then-regrab lock-in
- [x] Mouse wheel passes through to the reading surface (incl. nested child) while attached
- [x] Arrow-resize feature removed from the codebase (`processStickyKeys`, `resizeSelection`, key-state flags)
- [x] "Resize Selection" wording renamed to "Redraw" in tray menu, README, and log
- [x] Release build compiles clean
- [x] ADR-0006 drafted (Proposed); ADR-0004 annotated
- [ ] Wheel pass-through smoke-tested on real reading surfaces — deferred to next sitting
- [ ] `GrabBand` (6px) validated against real strip heights — deferred
