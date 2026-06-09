#pragma once

#include <functional>
#include <vector>
#include <windows.h>

class SelectionCapture {
public:
    using CompleteCallback = std::function<void(const RECT&)>;
    using CancelCallback = std::function<void()>;

    SelectionCapture() = default;
    ~SelectionCapture();

    bool begin(HINSTANCE instance, CompleteCallback complete, CancelCallback cancel);
    void cancel();
    bool active() const { return hwnd_ != nullptr; }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void render();
    void drawToolbar(HDC dc, int width);
    RECT virtualScreen() const;
    RECT normalizedSelection() const;
    void rebuildDib(int w, int h);
    void releaseDib();

    HWND hwnd_ = nullptr;
    HINSTANCE instance_ = nullptr;
    CompleteCallback complete_;
    CancelCallback cancel_;
    POINT start_ = {};
    POINT current_ = {};
    bool drawing_ = false;

    // DIB cache — alive for the duration of one capture session
    std::vector<unsigned int> pixelBuf_;
    HDC     dibDC_     = nullptr;
    HBITMAP dibBitmap_ = nullptr;
    void*   dibBits_   = nullptr;

    // GDI object cache for drawToolbar
    HBRUSH toolbarFillBrush_     = nullptr;
    HBRUSH toolbarSelectedBrush_ = nullptr;
    HPEN   toolbarOutlinePen_    = nullptr;
    HFONT  toolbarTitleFont_     = nullptr;
    HFONT  toolbarSmallFont_     = nullptr;
    HPEN   selectionBorderPen_   = nullptr;
};
