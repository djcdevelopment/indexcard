# IndexCard

A reading light for your screen. Dims everything except the line you're on — a digital version of the classic index-card-under-the-line reading technique.

## Install

Download the latest **IndexCard-x.x-Setup.exe** from [Releases](https://github.com/dciula/reader/releases) and run it. No admin rights required.

Or download the bare **IndexCard.exe** and double-click — no installer, no dependencies.

## Use

- `Win+Shift+W` — toggle the overlay on/off. First press opens a selection screen: drag to draw your reading strip.
- Click the **dimmed overlay area** to enter follow mode (overlay tracks your cursor). Click again to stop.
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
| `opacity` | 0.90 | Dimming strength (0.15–1.0) |
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
