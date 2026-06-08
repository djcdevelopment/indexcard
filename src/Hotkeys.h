#pragma once

#include <windows.h>

class Hotkeys {
public:
    static constexpr int ToggleOverlayId = 1;

    Hotkeys() = default;
    ~Hotkeys();

    bool registerAll(HWND hwnd);
    void unregisterAll();

private:
    HWND hwnd_ = nullptr;
    bool registered_ = false;
    HHOOK hook_ = nullptr;
};
