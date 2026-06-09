#include "TrayIcon.h"

TrayIcon::~TrayIcon()
{
    destroy();
}

bool TrayIcon::create(HWND hwnd, HINSTANCE instance)
{
    destroy();
    hwnd_ = hwnd;

    nid_ = {};
    nid_.cbSize = sizeof(nid_);
    nid_.hWnd = hwnd_;
    nid_.uID = 1;
    nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid_.uCallbackMessage = TrayMessage;
    nid_.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(nid_.szTip, L"IndexCard");

    added_ = Shell_NotifyIconW(NIM_ADD, &nid_) != FALSE;
    if (added_) {
        nid_.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &nid_);
    }
    return added_;
}

void TrayIcon::destroy()
{
    if (added_) {
        Shell_NotifyIconW(NIM_DELETE, &nid_);
    }
    added_ = false;
    hwnd_ = nullptr;
}

void TrayIcon::setVisibleState(bool visible)
{
    visible_ = visible;
    if (!added_) {
        return;
    }
    nid_.uFlags = NIF_TIP;
    wcscpy_s(nid_.szTip, visible ? L"IndexCard - visible" : L"IndexCard - hidden");
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
}

void TrayIcon::showMenu()
{
    if (!hwnd_) {
        return;
    }

    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, CmdShowHide, visible_ ? L"Hide" : L"Show");
    AppendMenuW(menu, MF_STRING, CmdResize, L"Resize Selection");
    AppendMenuW(menu, MF_STRING, CmdReset, L"Reset Defaults");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, CmdQuit, L"Quit");

    POINT pt = {};
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd_);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);
}
