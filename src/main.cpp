#include "Hotkeys.h"
#include "Logger.h"
#include "OverlayWindow.h"
#include "SelectionCapture.h"
#include "Settings.h"
#include "SplashWindow.h"
#include "TrayIcon.h"

#include <memory>
#include <sstream>
#include <windows.h>

namespace {

const wchar_t* AppWindowClassName = L"FocusStripAppWindow";

struct AppState {
    HINSTANCE instance = nullptr;
    HWND hwnd = nullptr;
    SettingsStore store;
    Settings settings;
    OverlayWindow overlay;
    SelectionCapture capture;
    TrayIcon tray;
    Hotkeys hotkeys;
    SplashWindow splash;
    bool firstHotkeyShouldCapture = false;
};

std::unique_ptr<AppState> g_app;

void saveAndUpdateTray(const Settings& settings)
{
    if (!g_app) {
        return;
    }
    g_app->store.save(settings);
    g_app->tray.setVisibleState(settings.visible);
}

void beginCapture()
{
    if (!g_app || g_app->capture.active()) {
        Log::write(L"beginCapture ignored: no app or already active");
        return;
    }

    Log::write(L"beginCapture");
    g_app->overlay.hide();
    const bool started = g_app->capture.begin(
        g_app->instance,
        [](const RECT& selection) {
            Log::write(L"beginCapture complete callback");
            g_app->firstHotkeyShouldCapture = false;
            g_app->overlay.applySelectionRect(selection);
            saveAndUpdateTray(g_app->overlay.settings());
        },
        []() {
            Log::write(L"beginCapture cancel callback");
            saveAndUpdateTray(g_app->overlay.settings());
        });
    std::wostringstream line;
    line << L"beginCapture result=" << started;
    Log::write(line.str());
}

void handleToggleHotkey()
{
    if (!g_app) {
        return;
    }
    std::wostringstream line;
    line << L"handleHotkey begin capture firstCapture=" << g_app->firstHotkeyShouldCapture
         << L" overlayVisible=" << g_app->overlay.visible();
    Log::write(line.str());
    beginCapture();
}

void resetDefaults()
{
    if (!g_app) {
        return;
    }
    Settings reset;
    reset.visible = g_app->overlay.visible();
    reset.loadedFromDisk = true;
    reset.selectionConfigured = false;
    g_app->firstHotkeyShouldCapture = true;
    g_app->overlay.reset(reset);
    saveAndUpdateTray(g_app->overlay.settings());
}

void setDpiAwareness()
{
    HMODULE user32 = LoadLibraryW(L"user32.dll");
    if (user32) {
        using SetDpiAwarenessContextFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
        auto setContext = reinterpret_cast<SetDpiAwarenessContextFn>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (setContext && setContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
            FreeLibrary(user32);
            return;
        }
        FreeLibrary(user32);
    }
    SetProcessDPIAware();
}

LRESULT CALLBACK AppWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_HOTKEY:
        Log::write(L"WM_HOTKEY received");
        if (wParam == Hotkeys::ToggleOverlayId) {
            handleToggleHotkey();
            return 0;
        }
        break;

    case TrayIcon::TrayMessage:
        Log::write(L"Tray message received");
        if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_CONTEXTMENU) {
            g_app->tray.showMenu();
            return 0;
        }
        if (LOWORD(lParam) == WM_LBUTTONDBLCLK) {
            handleToggleHotkey();
            return 0;
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case TrayIcon::CmdShowHide:
            Log::write(L"Tray command Show/Hide");
            handleToggleHotkey();
            return 0;
        case TrayIcon::CmdResize:
            Log::write(L"Tray command Resize Selection");
            beginCapture();
            return 0;
        case TrayIcon::CmdReset:
            Log::write(L"Tray command Reset Defaults");
            resetDefaults();
            return 0;
        case TrayIcon::CmdQuit:
            Log::write(L"Tray command Quit");
            DestroyWindow(hwnd);
            return 0;
        default:
            break;
        }
        break;

    case WM_DESTROY:
        Log::write(L"App WM_DESTROY");
        if (g_app) {
            Settings settings = g_app->overlay.settings();
            settings.visible = g_app->overlay.visible();
            settings.sticky = false;
            g_app->store.save(settings);
            g_app->hotkeys.unregisterAll();
            g_app->tray.destroy();
            g_app->overlay.destroy();
        }
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool registerAppClass(HINSTANCE instance)
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = AppWndProc;
    wc.lpszClassName = AppWindowClassName;
    return RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    Log::init();
    Log::write(L"App startup");
    setDpiAwareness();

    g_app = std::make_unique<AppState>();
    g_app->instance = instance;
    g_app->settings = g_app->store.load();
    g_app->firstHotkeyShouldCapture = !g_app->settings.selectionConfigured;
    std::wostringstream initialLine;
    initialLine << L"Initial state firstHotkeyShouldCapture=" << g_app->firstHotkeyShouldCapture;
    Log::write(initialLine.str());

    if (!registerAppClass(instance)) {
        std::wostringstream line;
        line << L"registerAppClass failed lastError=" << GetLastError();
        Log::write(line.str());
        return 1;
    }

    g_app->hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        AppWindowClassName,
        L"Focus Strip",
        WS_OVERLAPPED,
        0,
        0,
        0,
        0,
        nullptr,
        nullptr,
        instance,
        nullptr);
    if (!g_app->hwnd) {
        std::wostringstream line;
        line << L"App hidden CreateWindowEx failed lastError=" << GetLastError();
        Log::write(line.str());
        return 1;
    }
    {
        std::wostringstream line;
        line << L"App hidden hwnd=0x" << std::hex << reinterpret_cast<uintptr_t>(g_app->hwnd);
        Log::write(line.str());
    }

    if (!g_app->overlay.create(instance, g_app->settings)) {
        Log::write(L"Overlay create failed");
        return 1;
    }
    Log::write(L"Overlay create ok");

    g_app->overlay.onSettingsChanged([](const Settings& settings) {
        saveAndUpdateTray(settings);
    });
    g_app->overlay.onVisibilityChanged([](const Settings& settings) {
        saveAndUpdateTray(settings);
    });
    g_app->overlay.onResizeRequested([]() {
        beginCapture();
    });

    const bool trayOk = g_app->tray.create(g_app->hwnd, instance);
    {
        std::wostringstream line;
        line << L"Tray create result=" << trayOk;
        Log::write(line.str());
    }
    g_app->tray.setVisibleState(g_app->overlay.visible());
    const bool hotkeysOk = g_app->hotkeys.registerAll(g_app->hwnd);
    {
        std::wostringstream line;
        line << L"Hotkeys registerAll result=" << hotkeysOk;
        Log::write(line.str());
    }

    if (!g_app->settings.selectionConfigured) {
        g_app->splash.onDrawRequested([]() {
            Log::write(L"Splash: draw requested");
            beginCapture();
        });
        g_app->splash.onDismissed([]() {
            Log::write(L"Splash: dismissed");
        });
        const bool splashOk = g_app->splash.create(instance);
        std::wostringstream splashLine;
        splashLine << L"Splash create result=" << splashOk;
        Log::write(splashLine.str());
    }

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    Log::write(L"App exit");
    g_app.reset();
    return static_cast<int>(msg.wParam);
}
