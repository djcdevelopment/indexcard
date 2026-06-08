#pragma once
#include <functional>
#include <windows.h>

class SplashWindow {
public:
    using VoidCallback = std::function<void()>;

    SplashWindow() = default;
    ~SplashWindow();

    bool create(HINSTANCE instance);
    void destroy();

    void onDrawRequested(VoidCallback cb) { drawRequested_ = std::move(cb); }
    void onDismissed(VoidCallback cb) { dismissed_ = std::move(cb); }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

private:
    LRESULT handleMessage(UINT msg, WPARAM wp, LPARAM lp);
    void paint(HDC dc);
    RECT drawButtonRect() const;
    RECT laterButtonRect() const;

    int s(int v) const { return v * dpi_ / 96; }  // logical → physical pixels

    HWND hwnd_ = nullptr;
    HINSTANCE instance_ = nullptr;
    int dpi_ = 96;
    static constexpr int kW = 500;
    static constexpr int kH = 480;

    VoidCallback drawRequested_;
    VoidCallback dismissed_;
};
