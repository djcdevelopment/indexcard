# Retro: Hotkey combo-reset guard removal

*A small but precise follow-up to the packaging session: a buggy guard in the low-level keyboard hook's key-up path was removed, making the Win+Shift+W combo reliably re-firable after the first use.*

*Date: 2026-06-09 · Scope: uncommitted changes since commit 9f1948d*

---

## What shipped

One change, three lines removed, in `src/Hotkeys.cpp:39-41`.

In `LowLevelKeyboardProc`, the key-up handler for combo keys previously guarded the `g_comboDown = false` reset:

```cpp
// old — guarded
if (!isWinShiftW('W')) {
    g_comboDown = false;
}

// new — unconditional
g_comboDown = false;
```

The guard was meant to prevent clearing `g_comboDown` while the combo was still physically held. The implementation was wrong in two ways: (1) it always passed a hardcoded `'W'` as `vkCode` regardless of which key actually triggered the key-up; (2) `GetAsyncKeyState` inside `isWinShiftW` reads key state during the hook callback, before the releasing key has been fully unregistered from the system — the result is timing-dependent and unreliable. In a specific release ordering (e.g., Shift released while Win+W still held), the guard could leave `g_comboDown = true` after the combo fully released, causing the hotkey to silently refuse the next press. Unconditional clear is both simpler and correct: once any combo key goes up, the combo is broken.

---

## Engineering Lead perspective

This is a correctness fix, not a feature or architecture change. The bug lived in the fallback path (`WH_KEYBOARD_LL` hook), which activates only when `RegisterHotKey` fails — for example, when another application has already claimed Win+Shift+W. On most developer machines `RegisterHotKey` succeeds and this code path never runs, which is why the bug wasn't noticed until the hotkey was exercised carefully.

The fix's simplicity is its virtue. The guard was trying to be clever about not clearing combo state during a fast-retype scenario (user releases W and immediately re-presses while Win+Shift still held). But `g_comboDown` with `MOD_NOREPEAT`-equivalent logic already handles that: the key-down path only fires when `!g_comboDown`. Once W is released and `g_comboDown` clears, the combo can fire again — which is the correct behavior. The guard was solving a problem that didn't exist and creating one that did.

No new technical debt introduced. Net: -3 lines, one fewer function call per combo key-up event in the fallback path.

---

## Project / Program Manager perspective

Scope is minimal — one uncommitted fix that closes a corner case in the fallback hotkey path. No deferred work opened or closed. The previous session's "next moves" list (CI workflow test push, clean-machine installer test, SignPath application) remains unchanged and is the real queue.

This fix is worth shipping before the first `v0.1-test` tag push, since the CI release workflow will produce a binary that real users will run. Better to have the hotkey path correct before the first external testers encounter it.

---

## QA / Verification perspective

The fix is verifiable by exercising the fallback path: on a machine where another application has registered Win+Shift+W (or by temporarily modifying the code to skip `RegisterHotKey` and force the hook path), confirm that Win+Shift+W fires reliably on every press/release cycle, including after unusual release orderings (release Shift first, or release W before Win).

The fix was not smoke-tested end-to-end this session — the change is mechanical and the logic is straightforward, but the fallback path requires a specific environment to exercise. This is consistent with the previous retro's QA gaps (installer untested on clean VM, CI workflow untested on real runner); the pattern is small-team pragmatism, not sloppiness.

---

## Operator perspective

I noticed the guard while rereading the hotkey code. `isWinShiftW('W')` inside a key-up handler where the released key might be W, Win, or Shift — and with a hardcoded `'W'` argument — was immediately suspicious. The scenario where it could bite: second Win+Shift+W press silently does nothing after the first fires. The kind of bug that would be annoying to diagnose cold in production. Worth three lines.

No design decision here, just a bug caught by reading the code again with fresh eyes.

---

## How we worked together (human ↔ AI)

### What worked well

- **Reading the diff, not the description.** The change was identified by reading the code directly, not from a report or failing test. This is the right tool for this kind of latent logic bug — the bug leaves no visible trace until a specific event sequence occurs.

- **Previous retro's patterns held.** "When removing a guard, verify what the guard was protecting before deleting it" was applied: the guard's intended behavior was traced before the removal was confirmed correct.

### What didn't

- **No automated test covers this path.** The fallback hook path has zero test coverage. The fix is confident based on code reading, not based on a test that can fail if someone reintroduces the bug. This is acceptable for a solo project at this stage but worth noting.

### Patterns to repeat

- Re-read recently written code before tagging a release — small logic errors surface more easily with a day's distance.

### Patterns to change

- Nothing new this session.

---

## Lessons learned

1. **Hardcoded arguments in a generic handler are a smell.** `isWinShiftW('W')` in a key-up handler that fires for W, Win, and Shift was a sign the function was being used outside its intended call site. If a function's only valid caller passes a specific constant, the constant should be inside the function or the call site should be reconsidered.

2. **`GetAsyncKeyState` in a low-level hook is timing-sensitive.** The async key state reads the kernel's key-state table, which updates during hook processing — the key being released may or may not appear as "up" depending on where in the callback chain you are. Avoid reasoning about "is this key still down" inside a WH_KEYBOARD_LL handler for the key that just went up.

---

## Next moves

*(Unchanged from previous retro — this session added nothing to the queue.)*

- **Push a `v0.1-test` tag** to validate the GitHub Actions release workflow. Verify ISCC path and that the release publishes correctly.
- **Apply to SignPath Foundation** for free OSS Authenticode signing.
- **Run the installer on a clean machine** (no VS installed, fresh Windows 10). Verify no UAC prompt, Start Menu shortcut, app launches.
- **Submit first real GitHub Release** once CI and signing are verified.
- **Optional: winget manifest** — low priority until stable release URL exists.
- **SplashWindow WM_CLOSE** — still deferred. Alt-F4 on splash fires `DefWindowProc` without calling `dismissed_()`.
- **Custom tray icon** — still using `IDI_APPLICATION`.

---

## Acceptance gates met

- [x] `g_comboDown` cleared unconditionally on any combo key-up — guard removed
- [x] Logic traced: unconditional clear is correct, guard was both wrong and unnecessary
- [ ] Fallback path exercised end-to-end on a machine where `RegisterHotKey` fails — deferred
