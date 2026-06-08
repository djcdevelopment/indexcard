# Retro: UI polish — splash screen and toolbar redesign

*First public-release pass on FocusStrip: a new first-run splash screen and a complete overlay toolbar redesign, replacing ad-hoc corner buttons with a dark pill toolbar and a teal visual language.*
*Date: 2026-06-08 · Scope: full session (no git history; repo is pre-init)*

---

## What shipped

Six source files changed or created, one new `retros/` directory initialized.

**New files:**
- `D:\work\reader\src\SplashWindow.h` / `SplashWindow.cpp` — first-run splash screen. 500×480 logical pixels, DPI-aware (queries `LOGPIXELSX` at runtime and scales all coordinates and font sizes by `dpi/96`). Dark card with rounded corners via `SetWindowRgn`, double-buffered `WM_PAINT`. Teal app icon, bold headline, description, `[Win]+[Shift]+[W]` key badge row, teal primary CTA button, dark secondary. Fires when `settings.selectionConfigured == false`.

**Modified files:**
- `src/OverlayWindow.h` — `HitTarget` enum replaced (Sticky→Follow, Resize→Redraw, Close→Hide, added Tune); `tuneOpen_` bool added; layout helpers replaced (`moveRect`/`stickyRect`/`resizeRect`/`closeRect`/`bottomControlsRect`/`sliderRect` → `pillRect`/`pillButtonRect`/`hintLabelRect`/`slidersZoneRect`/`sliderTrackRect`).
- `src/OverlayWindow.cpp` — `render()` fully rewritten: dark rounded-pill toolbar (5 icon-only buttons at 16pt: ☰ ◎ ↖ ≡ ✕), teal 2px selection border drawn directly in the pixel buffer (not via GDI, so `UpdateLayeredWindow` premultiplied-alpha is correct), two new helper families (`fillRoundRectArgb` + `normalizeDibAlphaRound` for rounded-corner regions, `insideRoundRect` for per-pixel ellipse check). Hint label removed after user feedback. `hitTestClient` and `handleMessage` updated for new enum. Tune button toggles `tuneOpen_`; sliders render above the pill only when open.
- `src/SelectionCapture.cpp` — cursor changed from `IDC_CROSS` to `IDC_ARROW`.
- `src/Settings.h` — default `marginBottom` 150→160.
- `src/Settings.cpp` — `marginBottom` clamp min 50→120 (ensures room for pill + sliders zone).
- `CMakeLists.txt` — added `SplashWindow.cpp/.h`; added `/utf-8` compile flag (fixes `·`, `—`, `'` rendering in GDI string literals).

---

## Engineering Lead perspective

The load-bearing new subsystem is `SplashWindow`. The design constraint — ultra-lightweight, zero non-Win32 dependencies — was preserved: it's a plain `WS_POPUP` window using `SetWindowRgn` for rounded corners and standard `WM_PAINT` + `BitBlt` double buffering. No GDI+, no Direct2D, no embedded assets.

The trickiest part of the overlay redesign was the `UpdateLayeredWindow` alpha path for rounded-corner UI elements. GDI operations on a 32bpp DIB write RGB but zero the alpha channel. The existing `normalizeDibAlpha` worked fine for rectangles, but the new pill shape required a pixel-level ellipse test. The solution — `insideRoundRect` (clamped-center ellipse formula) used in both `fillRoundRectArgb` and `normalizeDibAlphaRound` — is general and could serve any future rounded-corner element in the overlay.

The teal selection border was similarly constrained: drawing it via GDI would leave alpha=0 in those pixels (invisible through `UpdateLayeredWindow`). Drawing it directly in the pixel vector with `fillRectArgb` before `memcpy` sidesteps the GDI alpha problem entirely.

DPI handling for the splash required a runtime query rather than compile-time constants. Using `GetDeviceCaps(screen, LOGPIXELSX)` before window creation gives the correct physical pixel count; every coordinate and font height in `paint()` then flows through `s(int)` which is `v * dpi_ / 96`. At 175% DPI (168 logical pixels per inch) the window is created at 875×840 physical pixels. The pattern is verbose but unambiguous and survives per-monitor DPI changes on next launch.

One genuine technical debt item: `hintLabelRect()` still exists in `OverlayWindow.h` and `.cpp` even though the hint label was removed from `render()`. The dead function isn't harmful but should be pruned before the next pass.

---

## Project / Program Manager perspective

The stated goal was "a layer of polish so we can release this publicly." The session delivered the two headline items from the design mockup: a first-run splash screen and a redesigned toolbar. Both are complete and verified running.

Deferred items (not in scope for this session, worth tracking):
- The splash does not yet handle the case where the user closes it via the window's system menu or Alt-F4 (no OS close button, but `WM_CLOSE` is not explicitly handled — currently falls through to `DefWindowProc` which destroys the window without calling `dismissed_`). Low risk for now.
- The toolbar icons are Unicode characters rendered in Segoe UI; on systems where Segoe UI doesn't include a given glyph, fallback rendering is unpredictable. Acceptable for a personal tool; worth revisiting before a broader release.
- No installer or signed binary — the exe ships as a raw artifact. Fine for the intended audience; a blocker for anything approaching the Windows Store.
- Tray menu still references old UI concepts ("Resize Selection," "Reset Defaults") — these work, but the language doesn't match the new toolbar vocabulary.

The session also produced this retro and initialized the `retros/` directory, establishing the documentation pattern for future sessions.

---

## QA / Verification perspective

Verification was done by running the binary directly between each change cluster and observing behavior:

- **Splash first-run gate**: confirmed by deleting `%APPDATA%\FocusStrip\settings.json` before each test launch. Splash appeared; "Draw a reading strip" triggered `beginCapture()`; "Maybe later" dismissed cleanly.
- **DPI scaling**: user confirmed splash was still too small at 175% DPI after the first build, then confirmed correct size after the `LOGPIXELSX` + `s()` scaling pass.
- **Encoding fix**: `·` and `—` were visibly garbled in the first splash render (UTF-8 bytes misread as ANSI). Confirmed clean after `/utf-8` flag added.
- **Toolbar**: user confirmed pill visible and functional after drawing a strip.
- **Hint bar removal**: user flagged the top hint label as unwanted; confirmed removed on next launch.
- **Arrow cursor**: user confirmed cursor change during selection capture. (No screenshot of this; described as "nice.")

Gaps not covered:
- Multi-monitor layout not tested; the splash centers on the system work area, which may not be the active monitor on dual-monitor setups.
- Tune slider panel not explicitly user-tested this session — it builds and the logic is wired, but the user didn't exercise it post-redesign.
- Splash `WM_CLOSE` / Alt-F4 path not tested.

---

## Operator perspective

I wanted this tool to be shareable without it looking like a dev's personal script. The mock that design built gave a clear target — the dark card, the teal accent, the pill toolbar — and the goal was to match the feel without importing the mock's dependencies or framework.

The iteration cycle was fast: build, observe, adjust. The two fixes I had to call out explicitly (the tiny splash at 175% DPI, the hint bar I didn't want) were caught immediately on first run, which is the right time to catch them. That's the workflow working correctly.

The decision to go icon-only on the toolbar buttons was a judgment call I made mid-session after seeing the first render. Labels competing with icon glyphs at that scale read as clutter; removing them and sizing up the icons to 16pt gave the pill a cleaner, more intentional look. That was the right call.

The arrow cursor fix was a one-line observation: crosshair implies precision drawing; the strip isn't that. Arrow is correct.

---

## How we worked together (human ↔ AI)

### What worked well

- **Screenshot-driven iteration.** The user attached screenshots of both the target mock and each intermediate build result. This let the AI match visual intent without needing the actual HTML source — the screenshots were the spec. When the source HTML was 1.2 MB of bundled React, reading it would have been noise; the screenshots were signal.

- **The "remove X" micro-requests landed cleanly.** "Remove the top black bar," "arrow cursor makes more sense," "icon only" — each was a one-line change that the AI found immediately without re-reading context. The codebase was small enough that grep + read of the right function was sufficient; no search agent needed.

- **DPI failure was caught on the first real run.** The user launched the splash, saw it was tiny, reported it immediately with a screenshot showing 175% DPI in Windows settings. The root cause (physical vs logical pixels in a per-monitor-DPI-aware process) and the fix (runtime `LOGPIXELSX` query + uniform `s()` scaling) were identified in one step. The AI didn't need to guess — the evidence was in the screenshot.

- **The pixel-buffer alpha strategy was reasoned from first principles correctly.** The `UpdateLayeredWindow` alpha constraint (GDI zeros the alpha channel, so GDI-drawn teal borders become invisible) was identified before writing the code, not discovered as a bug. The fix — draw borders in the pixel vector before `memcpy` — was applied preemptively.

- **Build errors were minimal and fast.** One `std::clamp` type ambiguity (`RECT` fields are `LONG`, not `int`), fixed with explicit casts on the first compiler error. One linker error because the old process was still running, fixed by killing it. Total unplanned debug time: under two minutes.

### What didn't

- **The hint label shipped and then was immediately removed.** The AI added it based on the mock reference without checking whether the user actually wanted it. A one-line "do you want the hint label from the mock?" before implementing would have saved a build-launch-remove cycle.

- **The first run of the splash required manual settings.json deletion.** The AI's initial "start it up" comment said "splash should be visible now" without first checking whether the existing settings file had `selectionConfigured=1`. The log check should have come before the confident prediction.

- **Splash layout was hardcoded in logical units without establishing the DPI strategy first.** The scaling was retrofitted in a second pass. A two-sentence upfront discussion ("this is a per-monitor-DPI-aware process, so font sizes and positions need to scale — here's the approach") would have produced a cleaner first implementation.

### Patterns to repeat

- Attach screenshots of running app to feedback messages — this is the highest-bandwidth feedback channel for UI work.
- For `UpdateLayeredWindow` + GDI work: always distinguish pixel-buffer writes (correct alpha) from GDI writes (alpha=0) explicitly before coding. Write the alpha strategy in a comment at the top of `render()`.
- Kill the running process before build when iterating on a Win32 exe — add it to the standard rebuild sequence automatically.

### Patterns to change

- Check for an existing saved state that would suppress a first-run feature before confidently predicting that feature will appear on next launch.
- Ask before implementing any "informational" UI element (banners, labels, hints) that exists in a reference mock but isn't explicitly requested — these are easy to add and easy to remove, but unnecessary remove cycles add noise.

---

## Lessons learned

1. **`UpdateLayeredWindow` and GDI do not compose on alpha.** GDI draws RGB but zeroes alpha in 32bpp DIBs. Any element you need to be visible through `UpdateLayeredWindow` must either be drawn in the pixel vector (before `memcpy`) or have its alpha restored by a post-GDI `normalizeDibAlpha` pass. The `fillRoundRectArgb` + `normalizeDibAlphaRound` pair solves this for rounded shapes without any GDI involvement in shape geometry.

2. **DPI-aware Win32 code must scale before `CreateWindowExW`, not after.** Query the DPI (via `GetDeviceCaps(screen, LOGPIXELSX)` or `GetDpiForSystem()`), compute physical dimensions, create the window at physical size. Retrofitting DPI scaling to a window that was created at logical size requires destroying and recreating the window; avoid that.

3. **`std::clamp` on RECT fields requires explicit casts.** `RECT.left/right/top/bottom` are `LONG`; `std::clamp` can't deduce `_Ty` when one argument is `LONG` and another is `int`. Cast with `static_cast<int>()` at the call site.

4. **MSVC treats source files as ANSI by default.** Any non-ASCII character in a wide string literal (`L"..."`) in a UTF-8 source file will be misinterpreted unless `/utf-8` is in the compile flags. Add it at project creation; don't rediscover it the first time a `·` renders as `Ã`.

5. **Small Win32 apps benefit from describing their alpha strategy explicitly.** `UpdateLayeredWindow` with premultiplied alpha is non-obvious; the rendering pipeline (pixel vector → memcpy → GDI text → normalizeDibAlpha) has a specific order for a specific reason. A two-line comment at the top of `render()` would save the next person (or next session) from having to reconstruct it from first principles.

---

## Next moves

- **Prune `hintLabelRect()`** from `OverlayWindow.h/.cpp` — the function is dead code since the hint bar was removed.
- **Handle `WM_CLOSE` in `SplashWindow`** — currently falls through to `DefWindowProc`, which destroys the window without firing `dismissed_`. Should call `dismissed_()` then `destroy()`.
- **Update tray menu language** — "Resize Selection" → "Redraw", consistent with pill toolbar vocabulary.
- **Initialize git** — `cd D:\work\reader && git init && git add . && git commit -m "initial: ui-polish pass, splash + pill toolbar"` to establish history before next session.
- **Tune slider UX** — user hasn't exercised the Tune panel post-redesign; worth a deliberate test pass before marking polish complete.
- **Consider tray icon update** — currently uses the default `IDI_APPLICATION` icon. A custom icon matching the teal strip visual language would complete the first-run impression.

---

## Acceptance gates met

- [x] Splash screen appears on first run (no `settings.json` or `selectionConfigured == false`)
- [x] Splash is DPI-aware — readable at 175% scaling
- [x] Splash special characters render correctly (`·`, `—`, `'`)
- [x] "Draw a reading strip" triggers selection capture and closes splash
- [x] "Maybe later" closes splash, app continues in tray
- [x] Overlay toolbar replaced with dark pill — 5 icon-only buttons
- [x] Selection strip has 2px teal border
- [x] Top hint label removed
- [x] Selection capture cursor changed to arrow
- [x] Zero build warnings, zero build errors
- [ ] Tune panel user-tested post-redesign — deferred (not exercised this session)
- [ ] Multi-monitor splash centering — deferred (not tested)
- [ ] `WM_CLOSE` handling in SplashWindow — deferred (low risk, logged above)
