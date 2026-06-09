# ADR-0003: Inno Setup for Windows packaging

**Status:** Accepted
**Date:** 2026-06-09

## Context

IndexCard needs a distribution artifact that non-technical Windows users can run without reading documentation. Options evaluated:

- **MSIX** — requires sideload enablement for unsigned packages; heavy for a 385 KB exe; blocked by SmartScreen unless signed or distributed via the Store.
- **WiX v4** — produces `.msi`; requires UAC elevation by default; XML authoring overhead is high relative to the app's size; `.msi` SmartScreen behavior is at least as aggressive as `.exe`.
- **Raw `.exe`** — zero installer friction, but no Start Menu shortcut, no uninstaller, stays in Downloads folder, SmartScreen warns on every new-user download.
- **NSIS** — viable; slightly less actively maintained than Inno Setup.
- **Inno Setup** — single-file `.exe` output, active development (v6.7.3), simple Pascal-like script, `PrivilegesRequired=lowest` for no-UAC per-user install, well-understood SmartScreen behavior.

## Decision

Use Inno Setup 6. The installer script lives at `installer/indexcard.iss`. Key choices in the script:

- `PrivilegesRequired=lowest` — installs to per-user `%AppData%\Local\Programs` without a UAC prompt.
- `{autopf}` / `{autoprograms}` — uses the correct per-user Program Files and Start Menu paths.
- Desktop shortcut is opt-in (task checkbox), not default.
- `AppMutex=IndexCard_SingleInstance` — Inno Setup checks this mutex and warns if the app is running during install. Must stay synchronized with the mutex name in `src/main.cpp`.
- Output to `dist/` (gitignored).

## Consequences

- Installer adds ~1.8 MB overhead over the bare exe (Inno Setup stub + LZMA compressed payload).
- `dist/` is gitignored; the installer is produced as a build artifact only.
- The GitHub Actions workflow (`release.yml`) installs Inno Setup on the runner via winget and builds the installer. The install path on `windows-latest` runners may differ from the local dev path (`%LOCALAPPDATA%\Programs\Inno Setup 6\`) and needs verification on a real runner push.
- Code signing is not yet wired in. Once SignPath Foundation approval comes through, a signing step will be added to the workflow between the Inno Setup build and the GitHub Release upload.
