# Focus Strip

Focus Strip is a small Windows 10 native C++ tray utility for keeping a persistent reading-guide overlay above terminals, editors, and log viewers. The overlay behaves like a quiet white index card with a clear focus band for the current line or block.

## Build

Requirements:

- Windows 10 or newer
- Visual Studio 2022 with the Desktop C++ workload
- CMake 3.20 or newer

From this directory:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

If CMake is not on `PATH`, Visual Studio's bundled CMake is typically available at:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' -S . -B build
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build --config Release
```

Run:

```powershell
.\build\Release\FocusStrip.exe
```

## Use

- `Win+Shift+W`: opens snip-style selection capture. Draw the focused reading selection; Focus Strip builds the white margin area around it.
- Tray menu:
  - Show/Hide
  - Resize Selection
  - Reset Defaults
  - Quit
- Upper-right handle: drag to move the overlay.
- Click the white card area or the upper-right sticky button to toggle mouse-follow mode.
- Sticky mode:
  - Overlay follows the mouse with the lower-right card corner anchored to the pointer.
  - Click the card once to release sticky mode.
  - Up/Down changes selection height by 10 px.
  - Left/Right changes selection width by 50 px.
- Upper-left resize button: enter snip-style selection capture again.
- Bottom controls:
  - X hides the overlay.
  - Sliders adjust vertical margin, horizontal margin, opacity, selection height, and selection width.

Settings are stored at:

```text
%APPDATA%\FocusStrip\settings.json
```

Debug logs are written to:

```text
%APPDATA%\FocusStrip\focusstrip.log
```

`Win+Shift+W` always opens the snip-style capture surface. Re-selecting the strip is the primary way to change its size and placement quickly.

## Architecture

The app is split into small Win32 modules:

- `main.cpp`: owns the hidden app window, message loop, DPI setup, and coordination between the tray, hotkey, overlay, capture window, and settings store.
- `OverlayWindow`: creates a `WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE` popup. It renders the white margin surface into a 32-bit DIB and publishes it with `UpdateLayeredWindow`. The focused reading band is a low-alpha cutout area inside the higher-opacity white strip.
- `Hotkeys`: registers global `Win+Shift+W` through `RegisterHotKey`.
- `TrayIcon`: owns the system tray icon and menu through `Shell_NotifyIcon`.
- `SelectionCapture`: creates a temporary full virtual-screen topmost capture window for snip-style rectangle drawing across multi-monitor layouts.
- `Settings`: persists a small JSON file without external dependencies.

Click-through behavior is handled through `WM_NCHITTEST`: only the move handle, sticky button, resize button, close button, and bottom sliders return `HTCLIENT`; the rest returns `HTTRANSPARENT` so the terminal/editor below remains usable. Sticky mode temporarily treats the overlay as interactive so a single click can release it.

DPI and multi-monitor behavior:

- The process requests per-monitor DPI awareness v2 when available, falling back to process DPI awareness on older systems.
- Selection capture uses the full virtual desktop bounds from `SM_XVIRTUALSCREEN`, `SM_YVIRTUALSCREEN`, `SM_CXVIRTUALSCREEN`, and `SM_CYVIRTUALSCREEN`, so negative monitor coordinates are supported.
- Overlay geometry is stored in screen pixels and restored on startup.
