#pragma once

#include <functional>
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

    HWND hwnd_ = nullptr;
    HINSTANCE instance_ = nullptr;
    CompleteCallback complete_;
    CancelCallback cancel_;
    POINT start_ = {};
    POINT current_ = {};
    bool drawing_ = false;
};
