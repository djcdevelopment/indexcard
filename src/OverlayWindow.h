#pragma once

#include "Settings.h"

#include <functional>
#include <vector>
#include <windows.h>

class OverlayWindow {
public:
    using VoidCallback = std::function<void()>;
    using SettingsCallback = std::function<void(const Settings&)>;

    OverlayWindow() = default;
    ~OverlayWindow();

    bool create(HINSTANCE instance, Settings settings);
    void destroy();

    void show();
    void hide();
    void toggle();
    bool visible() const { return visible_; }

    void reset(const Settings& settings);
    void applySelectionRect(const RECT& selection);
    const Settings& settings() const { return settings_; }

    void onSettingsChanged(SettingsCallback callback) { settingsChanged_ = std::move(callback); }
    void onVisibilityChanged(SettingsCallback callback) { visibilityChanged_ = std::move(callback); }
    void onResizeRequested(VoidCallback callback) { resizeRequested_ = std::move(callback); }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
    enum class HitTarget {
        None,
        Follow,
        Redraw,
        Hide,
    };

    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void render();
    void updateWindowSize();
    void rebuildDib(int w, int h);
    void rebuildFonts();
    void notifySettingsChanged();
    void notifyVisibilityChanged();
    void setSticky(bool enabled);
    void installStickyMouseHook();
    void uninstallStickyMouseHook();
    void updateStickyTracking();

    RECT selectionRect() const;
    RECT pillRect() const;
    RECT pillButtonRect(int index) const;
    HitTarget hitTestClient(POINT pt) const;

    HWND hwnd_ = nullptr;
    HINSTANCE instance_ = nullptr;
    Settings settings_;
    bool visible_ = false;
    HHOOK stickyMouseHook_ = nullptr;

    // Where the cursor sat, in window-client coords, when follow mode was
    // engaged. Tracking keeps this exact point under the cursor instead of
    // snapping the overlay's corner to it.
    int grabOffsetX_ = 0;
    int grabOffsetY_ = 0;

    // DIB cache — rebuilt only when window dimensions change
    std::vector<unsigned int> pixelBuf_;
    HDC     dibDC_      = nullptr;
    HBITMAP dibBitmap_  = nullptr;
    void*   dibBits_    = nullptr;
    int     dibCachedW_ = 0;
    int     dibCachedH_ = 0;

    // GDI object cache — created once, reused every frame
    HFONT pillIconFont_ = nullptr;
    HPEN  dividerPen_   = nullptr;

    SettingsCallback settingsChanged_;
    SettingsCallback visibilityChanged_;
    VoidCallback resizeRequested_;
};
