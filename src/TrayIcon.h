#pragma once

#include <windows.h>
#include <shellapi.h>

class TrayIcon {
public:
    static constexpr UINT TrayMessage = WM_APP + 10;
    static constexpr int CmdShowHide = 1001;
    static constexpr int CmdResize = 1002;
    static constexpr int CmdReset = 1003;
    static constexpr int CmdQuit = 1004;

    TrayIcon() = default;
    ~TrayIcon();

    bool create(HWND hwnd, HINSTANCE instance);
    void destroy();
    void setVisibleState(bool visible);
    void showMenu();

private:
    HWND hwnd_ = nullptr;
    NOTIFYICONDATAW nid_ = {};
    bool added_ = false;
    bool visible_ = false;
};
