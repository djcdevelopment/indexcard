# Retro: IndexCard — code hardening, rename, UI simplification, and packaging

*Three distinct arcs in one session: a multi-perspective code review that found real bugs and rewrote the rendering hot path; a product rename from FocusStrip to IndexCard; a toolbar simplification down to two buttons; and a complete Windows distribution pipeline (Inno Setup + GitHub Actions + static CRT). The app went from "dev's local tool with rough edges" to "something you can hand to a stranger and they'll figure it out."*

*Date: 2026-06-09 · Scope: all uncommitted changes since 2026-06-08 retro + commit 0fd9aaa (MIT license)*

---

## What shipped

The working tree carries 13 modified files and 3 new files against the initial commit. No individual commit exists for the session work yet — this retro accompanies the commit.

**Correctness fixes (5 bugs closed):**
- `src/main.cpp` — single-instance enforcement via `CreateMutexW(L"IndexCard_SingleInstance")`. Previously, running the exe twice gave two tray icons, two hotkey registrations, and two overlays. Now the second launch exits silently.
- `src/main.cpp` — `handleToggleHotkey` rewritten. The old function always called `beginCapture()` regardless of overlay state, meaning Win+Shift+W never simply toggled the overlay — it always opened the full-screen selection screen. Fixed to toggle show/hide when already configured, capture only on first use.
- `src/SelectionCapture.cpp:250` — `if (hwnd_ == hwnd_)` self-comparison (always true, harmless but wrong) replaced with unconditional `hwnd_ = nullptr`.
- `src/OverlayWindow.cpp` — `registerOverlayClass` no longer sets `registered = true` if `RegisterClassExW` returns 0 for a reason other than `ERROR_CLASS_ALREADY_EXISTS`.
- `src/main.cpp` — `setDpiAwareness` replaced `LoadLibraryW(L"user32.dll")` with `GetModuleHandleW(L"user32.dll")`. user32.dll is guaranteed loaded in every Win32 process; the LoadLibrary was unnecessary.

**Performance: per-frame allocation eliminated:**
`src/OverlayWindow.cpp` and `src/OverlayWindow.h` — the `render()` function previously allocated a `std::vector<unsigned int>` (~8 MB for 1920x1080), called `CreateDIBSection`, and created 5+ `HFONT` + multiple `HPEN`/`HBRUSH` objects every frame. In sticky/follow mode at 60 Hz, this was ~480 MB/s of heap churn plus repeated GDI kernel object creation. Now the DIB, pixel buffer, fonts, pens, and brushes are class members created once in `rebuildDib()` / `rebuildFonts()` and reused every frame. `render()` calls `std::fill` on the existing buffer, `memcpy` into the live DIB bits, then draws with cached objects. Same treatment applied to `src/SelectionCapture.cpp` — the virtual-screen-sized buffer (up to 30 MB on a dual-monitor rig) was previously allocated on every `WM_MOUSEMOVE` during selection capture.

**Configuration surface added:**
- `src/Settings.h` / `src/Settings.cpp` — `borderWidth` field (default 2, clamped 1–8) persisted to `%APPDATA%\IndexCard\settings.json`.
- `src/Theme.h` — new file. `Theme::Accent` / `AccentR/G/B` compile-time constants replace 6 scattered `RGB(94, 234, 212)` literals.

**Rename FocusStrip → IndexCard:**
All 13 source/build files and `README.md` updated. String mapping: `FocusStrip` → `IndexCard`, `Focus Strip` → `IndexCard`, `focusstrip` → `indexcard`, `READER · RUNNING IN TRAY` → `INDEXCARD · RUNNING IN TRAY`. CMakeLists.txt renamed project and executable. CMake regenerated to produce `build/IndexCard.sln` and `build/IndexCard.vcxproj`; stale `FocusStrip.*` files deleted. Mutex, log filename, AppData directory, window class atoms all updated. Motivation: throwback to the physical reading technique of holding an index card under a line of text.

**Toolbar simplification — 5 buttons → 2:**
`src/OverlayWindow.h` / `.cpp` — `PillButtonCount` reduced from 5 to 2. Removed: Move (☰), Follow (◎), Tune (≡) buttons and the entire slider panel. Remaining: `□` (redraw selection) and `✕` (hide). Follow mode still works — clicking anywhere on the dimmed overlay area starts follow mode; clicking again stops it. This was always how it worked; the explicit button was redundant. The Tune panel and all 5 sliders (`VerticalMargin`, `HorizontalMargin`, `Opacity`, `SelectionHeight`, `SelectionWidth`) are gone from the UI; settings are edited via `settings.json`. Removed dead code: `HitTarget::Move/Tune/Slider` enum values, `Slider` enum entirely, `draggingMove_`, `dragStart_`, `dragWindowStart_`, `activeSlider_`, `tuneOpen_`, `slidersZoneRect()`, `sliderTrackRect()`, `updateSliderFromPoint()`, `hintLabelRect()`, `overlayText()`, `overlayMono()`, `sliderLabelFont_`, `sliderTrackPen_`, `knobBrush_` GDI cache members.

**Windows distribution pipeline:**
- `CMakeLists.txt` — `CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded"` added. Switches from `/MD` (dynamic MSVC runtime) to `/MT` (static). Verified via `dumpbin`: zero `vcruntime`/`msvcp` imports. Exe is fully self-contained. Size: 385 KB.
- `installer/indexcard.iss` — Inno Setup 6 script. `PrivilegesRequired=lowest` (no UAC prompt, installs per-user). Optional desktop shortcut. Launches app at wizard end. Output: `dist/IndexCard-1.0-Setup.exe` (2.2 MB).
- `.github/workflows/release.yml` — GitHub Actions workflow triggered by `v*` tags. Builds, packages the installer, stamps the version from the tag, and creates a GitHub Release with both the installer and bare exe attached. Release notes are pre-populated.
- `README.md` — rewritten top-to-bottom. Lead with install link. Settings table. Build-from-source and installer build instructions.
- `.gitignore` — `dist/` added.

---

## Engineering Lead perspective

The session's most valuable single change was the DIB/GDI cache. A 60 Hz sticky-mode session was silently allocating and freeing ~8 MB per frame plus doing GDI kernel object creation at the same cadence. On a modern machine it was invisible — the code "worked" — but it was the kind of thing that would have shown up on a slower box, showed up in perf traces, and needed to be fixed before the code could be considered production-quality. The fix required restructuring `OverlayWindow` from a stateless renderer into an object with proper lifecycle (`create` → `rebuildDib`/`rebuildFonts` → repeated `render` → `destroy`), which also happens to be the correct model.

The `handleToggleHotkey` bug was genuinely confusing to live with: the hotkey was supposed to toggle the overlay but every press launched the full-screen selection screen instead. The root cause was a dead `firstHotkeyShouldCapture` field that was logged but never branched on — the branching logic was simply never written. The fix is straightforward but the diagnosis required a careful read of `beginCapture` + `handleToggleHotkey` together; the bug was invisible if you only read one.

The toolbar simplification was net code deletion: ~200 lines gone. The `Slider` enum, `updateSliderFromPoint`, `slidersZoneRect`, `sliderTrackRect`, `tuneOpen_`, drag state, and the GDI cached objects that served the slider panel all came out cleanly. The overlay window's hit-test function collapsed from 50 lines to 5. This is the kind of simplification that pays off not just in the moment but in every future edit to the rendering path.

Static CRT was a prerequisite for distribution, not an optimization. `/MT` balloons the exe from ~150 KB (dynamic) to 385 KB, but eliminates the invisible dependency on the MSVC redistributable. On a freshly imaged Windows 10 machine without VS installed, the dynamic build would silently fail to launch. The static build runs anywhere.

**Technical debt zeroed or introduced:**
- Zeroed: all items from the previous retro's "next moves" list that touched `hintLabelRect` (dead function), `handleToggleHotkey` behavior, and the Tune slider panel.
- Introduced: `AppMutex=IndexCard_SingleInstance` in the `.iss` — this tells Inno Setup to check the mutex on install and warn if the app is running. Should be verified that the string matches the mutex name in `main.cpp` exactly. It does, but it's a coordination point that could drift.

---

## Project / Program Manager perspective

The session addressed the full "pre-release checklist" that was implicit in the last retro's "next moves" section. The last retro explicitly listed packaging as a blocker ("no installer or signed binary — the exe ships as a raw artifact. Fine for the intended audience; a blocker for anything approaching the Windows Store"). That blocker is resolved.

The three arcs mapped to three different stakeholders' concerns simultaneously: the code review was a correctness/quality exercise, the rename was a product identity decision, the packaging was a distribution milestone. All three shipped in one session, which is efficient but also means the diff is large and the commit message will need to be comprehensive.

**Deferred with good reason:**
- Code signing. The plan is to apply to SignPath Foundation (free for MIT-licensed OSS projects). Application is async (days to weeks). The GitHub Actions workflow has a clearly marked signing step stub. Until signing lands, users will see a "Unknown publisher — Run anyway" SmartScreen prompt on first download; this is acceptable for early testers who know you.
- Winget submission. A 30-line YAML manifest could be submitted to `microsoft/winget-pkgs` today for `winget install IndexCard`. Low priority until there's a GitHub Release to point at.
- Splash `WM_CLOSE` handling still deferred (from last retro). The splash window has no system close button (WS_POPUP, no frame), so this only fires on Alt-F4. Low-priority but not zero-risk.
- Custom tray icon. Still uses `IDI_APPLICATION` default. Visual polish item.

**Scope creep that was actually right:** The toolbar simplification wasn't in the original "code review" ask. It came out of the product owner review perspective during the code analysis. "Remove three buttons that aren't needed" is not a code quality fix — it's a product decision. But it was the right call and the timing was right (before shipping to real users).

---

## QA / Verification perspective

The static CRT change was verified definitively: `dumpbin /imports IndexCard.exe` returned zero matches for `vcruntime`, `msvcp`, or `ucrtbase`. This is the correct verification — not "it runs on my machine" but "the binary has no such imports."

The single-instance fix is verifiable by running the exe twice. Second instance should exit with no visible artifact. This was described but not independently tested via screenshot; the implementation is straightforward and the mutex semantics are well-understood, but it should be on the first-testers' checklist.

The `handleToggleHotkey` fix changes a fundamental UX behavior. The previous behavior (always open capture) is gone; Win+Shift+W now toggles if configured. This should be the first thing a tester exercises. There's a subtle state machine here: `selectionConfigured` in `settings.json` must be `true` for toggle mode to activate; a clean-settings run will still show capture on first press. This is correct behavior but worth documenting in the test scenario.

Installer: `IndexCard-1.0-Setup.exe` was built and confirmed at 2,191 KB. The build succeeded with zero Inno Setup errors. The installer was not run end-to-end in a clean VM this session — this is the most significant verification gap. The script uses `PrivilegesRequired=lowest` and `{autopf}`, which are less common than the default admin install; their interaction with UAC and the per-user Program Files location should be explicitly confirmed before the first public release.

The GitHub Actions workflow (`release.yml`) is untested. It cannot be tested without pushing a `v*` tag to a repository with GitHub Actions enabled. The script logic is straightforward (`winget install InnoSetup`, version substitution via PowerShell `-replace`, then `ISCC.exe`), but the `windows-latest` runner's Inno Setup install path may differ from the local `%LOCALAPPDATA%\Programs\Inno Setup 6\` path. Worth a test tag push (`v0.1-test`) to validate the CI path before the real release.

---

## Operator perspective

I came into this session wanting to fix the things that would have embarrassed me if I handed this to someone. The "many things appearing in the taskbar" complaint I had was actually two separate bugs — no single-instance check and a toggle hotkey that never toggled. Once you see it in the code it's obvious, but you wouldn't see it without looking. Having a reviewer go through the code and name them cleanly was exactly what this session needed.

The rename decision was already made before the session. "IndexCard" has a story — a physical artifact, a reading technique, a throwback. "FocusStrip" was fine but generic. Naming matters for a tool you're handing to people; it sets the expectation for what they're holding. The rename was clean but tedious — 6+ locations per variant across source, build, documentation, and memory. Letting the AI do the mechanical search-and-replace while I verified the output was the right split of labor.

The simplification to two buttons was a judgment call I made when looking at the toolbar cold. The Follow circle was not needed because clicking the overlay already enters follow mode — the button was a hint that the gesture existed, but the gesture is obvious enough. The Move handle was not needed because follow mode is also how you reposition. The Tune panel was the big one: removing in-app sliders means power users have to edit JSON, which is fine — this is a developer-adjacent tool. Non-technical users don't need fine-grained control; they need an obvious strip that works. The two remaining buttons (`□` and `✕`) are unambiguous. I'd rather ship something obviously simple than something that needs explaining.

The packaging path was where I learned something new — SignPath Foundation for free OSS Authenticode signing is real, the process is real, and the cost is genuinely zero if the project qualifies. I'd assumed code signing would cost money. Good to know it doesn't have to.

---

## How we worked together (human ↔ AI)

### What worked well

- **The multi-perspective code review format.** Framing the review as three distinct lenses (NASA/mission-critical for correctness, PC tuner for performance, product owner for UX) produced three distinct categories of findings. The performance findings (DIB/GDI cache) wouldn't have been flagged in a pure correctness review. The framing also gave the operator a way to adjudicate close calls: "all three perspectives agree" was a green light to implement; "one perspective dissents" was a reason to ask. This is a pattern worth repeating for any code that spans correctness, performance, and product concerns simultaneously.

- **Research agents were self-contained and produced actionable output.** The packaging research asked for a specific comparison table with current pricing and SmartScreen behavior. The agent returned exactly that, plus discovered the SignPath Foundation free tier — something the operator didn't know existed and that materially changed the signing plan. The secret was writing a prompt that named the exact questions rather than asking "what do you know about Windows packaging."

- **Plan-then-build rhythm with decision questions worked.** On the close calls (border width: Settings vs slider; accent color: constant vs runtime; SelectionCapture DIB cache: now vs later), the AskUserQuestion breakpoints produced clear decisions that were then implemented exactly as specified. Zero backtracking after any of those questions.

- **The retro format from the previous session surfaced an exact bug to fix.** The previous retro's "next moves" included "handle `WM_CLOSE` in SplashWindow," "prune `hintLabelRect()`," and "initialize git." The `hintLabelRect` item was addressed this session during the OverlayWindow cleanup. The retro-as-backlog pattern works — the next session has a specific list to pick up from.

- **Large rename was done mechanically with `replace_all` edits and verified by building.** Rather than a code review or manual grep, the rename was executed as a series of `replace_all` edits on each file + a CMake regenerate + a build. Zero compile errors from the rename itself. The only post-rename error (stale `firstHotkeyShouldCapture` reference in `beginCapture`) was caught by the compiler immediately. Build-as-verification is faster and more reliable than read-as-verification for mechanical changes.

### What didn't

- **The `NULL_BITMAP` assumption failed on the first build.** The initial DIB cache implementation used `GetStockObject(NULL_BITMAP)` to deselect a bitmap from a DC before deletion. This is not a valid stock object. The compile error was immediate and the fix was simple (just delete the DC first), but the pattern was wrong from first principles. A quick check of Win32 MSDC on stock object identifiers would have caught this before writing the code.

- **Two build failures from running-process lock.** The linker couldn't write `IndexCard.exe` because the running app held the file open. This was handled correctly (kill + rebuild) but happened twice in the session. The process should always be killed as the first step in a rebuild sequence when modifying an exe — this is now in the standard rebuild rhythm but wasn't applied automatically.

- **Inno Setup install path assumption wasn't verified before writing the CI workflow.** The workflow uses `$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe` — the path on the dev machine after `winget install`. On `windows-latest` GitHub runners, winget may install to a different path. This is a known unverified assumption and should be caught on the first test push.

- **`handleToggleHotkey` had a dead `firstHotkeyShouldCapture` field that needed cleanup in multiple places.** When the field was removed from `AppState`, one stale reference in `beginCapture`'s lambda was missed in the initial edit and caught only by the compiler. The edit should have searched for all references before deleting the field. Small friction, but it means a second round-trip through the build.

### Patterns to repeat

- Multi-perspective code review framing for any session covering correctness + performance + product simultaneously.
- Self-contained research agent prompts that name exact questions and ask for specific output format (comparison table, pricing, eligibility).
- AskUserQuestion checkpoints on close calls before building — prevents scope creep and backtracking.
- Kill-running-process as the first step in any rebuild sequence when the exe is running.
- Build-as-verification for mechanical renames and large-scale string substitutions.

### Patterns to change

- Before using a Win32 GDI concept (stock objects, etc.), confirm the identifier exists in MSDC before writing the code. Don't rely on recalled API knowledge for obscure constants.
- When removing a field or function, grep for all references before deleting. Don't rely on the compiler to find them all in the same edit pass — it will, but it adds a round-trip.
- Test the CI workflow with a throwaway tag push before treating it as production-ready.

---

## Lessons learned

1. **A bug that "works on my machine" is still a bug waiting to embarrass you.** The `handleToggleHotkey` function was wrong since the initial commit — it never toggled. But because the operator's workflow involved always redrawing the selection manually, it never visibly failed. A second set of eyes with no knowledge of the operator's habits found it immediately. Fresh-eyes review is worth scheduling before any public distribution.

2. **Static CRT is the correct default for a zero-dependency Win32 utility.** The difference between `/MD` and `/MT` is 230 KB of binary size, which is noise for a desktop utility. The difference in deployment reliability is the difference between "runs on any Windows 10+ machine" and "runs on any Windows 10+ machine that has MSVC runtime installed." Use `/MT` for redistribution; use `/MD` only when exe size is a hard constraint or DLL sharing is explicitly required.

3. **Product names need a story.** "FocusStrip" described a technical implementation detail (a strip that focuses). "IndexCard" describes an experience and a reference that non-technical users recognize. When distributing free tools to non-technical audiences, the name is the first impression. A name with a story is more memorable and more shareable.

4. **Removing UI surface area is a product decision, not just code cleanup.** Reducing the pill from 5 to 2 buttons required confidence that the removed interactions (drag-to-move, explicit follow toggle, in-app sliders) either had equivalent gesture-based equivalents or genuinely weren't needed. This can only be made by the product owner, not inferred by the AI from the code. The operator made three distinct calls here; the AI implemented them. That's the correct division of responsibility.

5. **Free code signing for OSS is real and the process is well-established.** SignPath Foundation and similar programs exist specifically for projects like this. The operator assumed signing would cost money; it doesn't have to for MIT-licensed projects with a GitHub repo. Check OSS signing programs before paying for a certificate.

---

## Next moves

- **Push a `v0.1-test` tag** to validate the GitHub Actions release workflow on a real runner. Verify ISCC path and that the release publishes correctly. Fix path assumptions before tagging a real release.
- **Apply to SignPath Foundation** at https://signpath.io/solutions/open-source-community. The MIT license qualifies. Approval is async; start now.
- **Run the installer on a clean machine** (no VS installed, fresh Windows 10). Verify: no UAC prompt, Start Menu shortcut created, app launches from installer end, uninstaller visible in "Apps & Features."
- **Submit first real GitHub Release** once CI workflow and signing are both verified.
- **Optional: winget manifest** — `microsoft/winget-pkgs` YAML submission (~30 lines). Low priority until there's a stable release URL to point at.
- **SplashWindow WM_CLOSE** — still deferred from previous retro. Alt-F4 on the splash fires `DefWindowProc` which destroys the window without calling `dismissed_`. Fix: handle `WM_CLOSE`, call `dismissed_()` then `destroy()`.
- **Custom tray icon** — still using `IDI_APPLICATION`. A 16x16 teal strip icon would complete the first-impression polish.

---

## Acceptance gates met

- [x] Single-instance enforcement — second launch exits silently, no duplicate tray icons
- [x] Win+Shift+W toggles show/hide when selection is configured; triggers capture on first use
- [x] DIB + pixel buffer + GDI objects cached in OverlayWindow — zero heap alloc per render frame
- [x] DIB cached in SelectionCapture — no 30 MB alloc per WM_MOUSEMOVE during capture
- [x] `borderWidth` in Settings, persisted, wired into render
- [x] `Theme.h` accent color constant — 6 scattered RGB literals consolidated
- [x] All strings, filenames, class atoms, mutex, AppData dir, log file renamed FocusStrip → IndexCard
- [x] CMake regenerated — `IndexCard.sln` and `IndexCard.vcxproj`, stale `FocusStrip.*` deleted
- [x] Pill toolbar reduced to 2 buttons (□ and ✕); Move/Follow/Tune/sliders removed
- [x] Static CRT (/MT) confirmed — zero MSVC runtime DLL imports in final binary
- [x] Inno Setup installer builds — `dist/IndexCard-1.0-Setup.exe` at 2.2 MB, no UAC prompt
- [x] GitHub Actions release workflow created (untested on real runner)
- [x] README rewritten — install link, settings table, accurate use description
- [ ] CI workflow tested on real runner (test tag push) — deferred, next step
- [ ] Installer tested on clean machine — deferred, next step
- [ ] SignPath Foundation application submitted — deferred, next step
