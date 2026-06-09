# ADR-0005: Product rename from FocusStrip to IndexCard

**Status:** Accepted
**Date:** 2026-06-09

## Context

The app was originally named FocusStrip, describing the implementation (a strip that focuses). Before public distribution, the product owner decided to rename to IndexCard — a reference to the classic physical reading technique of holding an index card under a line of text to keep your place and reduce visual distraction.

Rationale: a name with a story is more memorable and more shareable than a name that describes a technical artifact. Non-technical users recognize "index card" as a concrete object with a known purpose. The name sets the expectation for what the tool does before the user interacts with it.

## Decision

Rename all occurrences:

| Old | New |
|-----|-----|
| `FocusStrip` (code, class atoms, mutex) | `IndexCard` |
| `Focus Strip` (user-visible display) | `IndexCard` |
| `focusstrip` (lowercase filesystem) | `indexcard` |
| `READER · RUNNING IN TRAY` (splash) | `INDEXCARD · RUNNING IN TRAY` |
| `Reader` (splash window title) | `IndexCard` |

Renamed: CMakeLists.txt project and executable, all source string literals (window class atoms, mutex name, window titles, tray tooltips, log prefix, AppData directory, log filename), README. CMake regenerated: `build/IndexCard.sln` and `build/IndexCard.vcxproj` created, stale `FocusStrip.sln` and `FocusStrip.vcxproj` deleted.

Display name is one word: `IndexCard`. Not `Index Card`. Consistent with a product/app identifier.

## Consequences

- Existing users (currently just the author) will lose their `%APPDATA%\FocusStrip\settings.json` and `focusstrip.log`. No migration code. Pre-release, this is acceptable.
- The single-instance mutex name changed from `FocusStrip_SingleInstance` to `IndexCard_SingleInstance`. Any running FocusStrip instance will not block an IndexCard launch. On the transition machine, kill the old process before launching the new one.
- The window class atom names changed. If there is a stale `FocusStrip*` class atom registered in the same process (from a previously loaded module), `RegisterClassExW` will succeed with the new names without conflict.
- Files under `retros/` are excluded from the rename per operator instruction. The retro from 2026-06-08 refers to "FocusStrip" throughout; that is intentional historical record.
