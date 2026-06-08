#pragma once

#include "Settings.h"

#include <functional>
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
        Move,
        Follow,
        Redraw,
        Tune,
        Hide,
        Slider
    };

    enum class Slider {
        None,
        VerticalMargin,
        HorizontalMargin,
        Opacity,
        SelectionHeight,
        SelectionWidth
    };

    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void render();
    void updateWindowSize();
    void notifySettingsChanged();
    void notifyVisibilityChanged();
    void setSticky(bool enabled);
    void installStickyMouseHook();
    void uninstallStickyMouseHook();
    void updateStickyTracking();
    void processStickyKeys();
    void resizeSelection(int deltaWidth, int deltaHeight);

    RECT selectionRect() const;
    RECT pillRect() const;
    RECT pillButtonRect(int index) const;
    RECT hintLabelRect() const;
    RECT slidersZoneRect() const;
    RECT sliderTrackRect(Slider slider) const;
    HitTarget hitTestClient(POINT pt, Slider* slider = nullptr) const;
    void updateSliderFromPoint(Slider slider, int x);

    HWND hwnd_ = nullptr;
    HINSTANCE instance_ = nullptr;
    Settings settings_;
    bool visible_ = false;
    bool draggingMove_ = false;
    POINT dragStart_ = {};
    POINT dragWindowStart_ = {};
    Slider activeSlider_ = Slider::None;
    bool tuneOpen_ = false;
    bool keyUpDown_ = false;
    bool keyDownDown_ = false;
    bool keyLeftDown_ = false;
    bool keyRightDown_ = false;
    HHOOK stickyMouseHook_ = nullptr;

    SettingsCallback settingsChanged_;
    SettingsCallback visibilityChanged_;
    VoidCallback resizeRequested_;
};
