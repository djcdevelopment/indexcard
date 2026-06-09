# IndexCard

A reading light for your screen. Dims everything except the line you're on — a digital version of the classic index-card-under-the-line reading technique.

**Platform support:** Windows 10+ (current). macOS support is planned.

## Install

Download the latest **IndexCard-1.0-Setup.exe** from [Releases](https://github.com/djcdevelopment/indexcard/releases) and run it. No admin rights required.

Or download the bare **IndexCard.exe** and double-click — no installer, no dependencies.

> **Windows security prompt:** Because IndexCard is not yet code-signed, Windows SmartScreen will block it on first run. Click **More info → Run anyway** to proceed. This is expected until a signing certificate is in place.

## Use

- `Win+Shift+W` — toggle the overlay on/off. First press opens a selection screen: drag to draw your reading strip.
- Click the **dimmed overlay area** to enter follow mode (strip tracks your cursor vertically as you read). Click again to exit follow mode.
- Bottom-right pill:
  - `□` — redraw / resize the strip
  - `✕` — hide the overlay
- Tray icon (right-click):
  - Show/Hide, Resize Selection, Reset Defaults, Quit

### Settings

Edit `%APPDATA%\IndexCard\settings.json` to adjust colors, margins, opacity, and border width. Changes take effect on next launch.

| Key | Default | Description |
|-----|---------|-------------|
| `selectionWidth` | 900 | Strip width in pixels |
| `selectionHeight` | 40 | Strip height in pixels |
| `marginTop` | 150 | Space above the strip |
| `marginBottom` | 160 | Space below (room for the pill toolbar) |
| `marginLeft` | 75 | Space left of the strip |
| `marginRight` | 75 | Space right of the strip |
| `opacity` | 0.90 | How opaque the dim overlay is (0.15–1.0); higher = more dimmed |
| `borderWidth` | 2 | Strip border thickness in pixels (1–8) |

---

## Build from source

**Requirements:** Windows 10+, Visual Studio 2022 (Desktop C++ workload), CMake 3.20+

```powershell
cmake -S . -B build
cmake --build build --config Release
# output: build\Release\IndexCard.exe
```

**Build the installer** (requires [Inno Setup 6](https://jrsoftware.org/isinfo.php)):

```powershell
# Adjust the path to match your Inno Setup 6 installation
& "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe" installer\indexcard.iss
# output: dist\IndexCard-1.0-Setup.exe
```

---

## Architecture

Pure Win32 C++, no dependencies beyond the Windows API. Single-process, single-thread message loop.

| Module | Role |
|--------|------|
| `main.cpp` | App window, message loop, DPI setup, coordination |
| `OverlayWindow` | `WS_EX_LAYERED` popup, DIB pixel rendering, `UpdateLayeredWindow` |
| `SelectionCapture` | Full virtual-screen snip window for drawing the strip |
| `Hotkeys` | Global `Win+Shift+W` via `RegisterHotKey` + LL keyboard hook fallback |
| `TrayIcon` | System tray icon and context menu |
| `Settings` | JSON persistence in `%APPDATA%\IndexCard\` |
| `Theme.h` | Compile-time accent color (`#5eead4`) |

Click-through: `WM_NCHITTEST` returns `HTTRANSPARENT` over the strip so the content underneath stays fully interactive. The pill toolbar returns `HTCLIENT`.

Multi-monitor: selection capture uses `SM_XVIRTUALSCREEN` / `SM_CXVIRTUALSCREEN` so negative-coordinate monitors work correctly.

Logs: `%APPDATA%\IndexCard\indexcard.log`
