#include "Hotkeys.h"
#include "Logger.h"

#include <sstream>

namespace {

HWND g_hookHwnd = nullptr;
bool g_comboDown = false;

bool isKeyDown(int vk)
{
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

bool isWinShiftW(DWORD vkCode)
{
    const bool winDown = isKeyDown(VK_LWIN) || isKeyDown(VK_RWIN);
    const bool shiftDown = isKeyDown(VK_LSHIFT) || isKeyDown(VK_RSHIFT) || isKeyDown(VK_SHIFT);
    return vkCode == 'W' && winDown && shiftDown;
}

LRESULT CALLBACK LowLevelKeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION && g_hookHwnd) {
        const auto* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        const bool keyDown = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
        const bool keyUp = wParam == WM_KEYUP || wParam == WM_SYSKEYUP;

        if (keyDown && isWinShiftW(info->vkCode)) {
            if (!g_comboDown) {
                g_comboDown = true;
                Log::write(L"Keyboard hook detected Win+Shift+W");
                PostMessageW(g_hookHwnd, WM_HOTKEY, Hotkeys::ToggleOverlayId, 0);
            }
            return 1;
        }

        if (keyUp && (info->vkCode == 'W' || info->vkCode == VK_LWIN || info->vkCode == VK_RWIN || info->vkCode == VK_SHIFT || info->vkCode == VK_LSHIFT || info->vkCode == VK_RSHIFT)) {
            if (!isWinShiftW('W')) {
                g_comboDown = false;
            }
        }
    }

    return CallNextHookEx(nullptr, code, wParam, lParam);
}

} // namespace

Hotkeys::~Hotkeys()
{
    unregisterAll();
}

bool Hotkeys::registerAll(HWND hwnd)
{
    unregisterAll();
    hwnd_ = hwnd;
    registered_ = RegisterHotKey(hwnd_, ToggleOverlayId, MOD_WIN | MOD_SHIFT | MOD_NOREPEAT, 'W') != FALSE;
    std::wostringstream line;
    line << L"RegisterHotKey Win+Shift+W hwnd=0x" << std::hex << reinterpret_cast<uintptr_t>(hwnd_)
         << L" result=" << (registered_ ? L"ok" : L"failed")
         << L" lastError=" << std::dec << GetLastError();
    Log::write(line.str());

    if (!registered_) {
        g_hookHwnd = hwnd_;
        g_comboDown = false;
        hook_ = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandleW(nullptr), 0);
        std::wostringstream hookLine;
        hookLine << L"SetWindowsHookEx fallback for Win+Shift+W result=" << (hook_ ? L"ok" : L"failed")
                 << L" hook=0x" << std::hex << reinterpret_cast<uintptr_t>(hook_)
                 << L" lastError=" << std::dec << GetLastError();
        Log::write(hookLine.str());
    }

    return registered_ || hook_ != nullptr;
}

void Hotkeys::unregisterAll()
{
    if (registered_ && hwnd_) {
        Log::write(L"UnregisterHotKey Win+Shift+W");
        UnregisterHotKey(hwnd_, ToggleOverlayId);
    }
    if (hook_) {
        Log::write(L"UnhookWindowsHookEx Win+Shift+W fallback");
        UnhookWindowsHookEx(hook_);
    }
    hook_ = nullptr;
    g_hookHwnd = nullptr;
    g_comboDown = false;
    hwnd_ = nullptr;
    registered_ = false;
}
