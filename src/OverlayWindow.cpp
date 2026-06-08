#include "OverlayWindow.h"
#include "Logger.h"

#include <algorithm>
#include <sstream>
#include <cstring>
#include <cmath>
#include <memory>
#include <string>
#include <vector>
#include <windowsx.h>

namespace {

const wchar_t* OverlayClassName = L"FocusStripOverlayWindow";
constexpr UINT StickyTimerId = 1;
constexpr UINT StickyReleaseMessage = WM_APP + 30;
constexpr int StickyTimerMs = 16;
constexpr int PillHeight = 44;
constexpr int PillBottomPad = 10;
constexpr int PillButtonCount = 5;
constexpr int SlidersZoneHeight = 48;
constexpr int HintHeight = 22;

HWND g_stickyMouseHwnd = nullptr;

LRESULT CALLBACK StickyMouseProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION && g_stickyMouseHwnd) {
        switch (wParam) {
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_XBUTTONDOWN:
            PostMessageW(g_stickyMouseHwnd, StickyReleaseMessage, 0, 0);
            break;
        default:
            break;
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

int widthOf(const RECT& rect) { return rect.right - rect.left; }
int heightOf(const RECT& rect) { return rect.bottom - rect.top; }

bool contains(const RECT& rect, POINT pt)
{
    return pt.x >= rect.left && pt.x < rect.right && pt.y >= rect.top && pt.y < rect.bottom;
}

void registerOverlayClass(HINSTANCE instance)
{
    static bool registered = false;
    if (registered) {
        return;
    }

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = OverlayWindow::WndProc;
    wc.lpszClassName = OverlayClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    const ATOM atom = RegisterClassExW(&wc);
    std::wostringstream line;
    line << L"Overlay RegisterClass atom=" << atom << L" lastError=" << GetLastError();
    Log::write(line.str());
    registered = true;
}

bool insideRoundRect(int x, int y, const RECT& r, int rx, int ry)
{
    if (x < r.left || x >= r.right || y < r.top || y >= r.bottom) {
        return false;
    }
    const int cx = std::clamp(x, static_cast<int>(r.left) + rx, static_cast<int>(r.right) - rx);
    const int cy = std::clamp(y, static_cast<int>(r.top)  + ry, static_cast<int>(r.bottom) - ry);
    const double dx = x - cx;
    const double dy = y - cy;
    if (rx > 0 && ry > 0 && (dx * dx / (rx * rx) + dy * dy / (ry * ry) > 1.0)) {
        return false;
    }
    return true;
}

void fillRectArgb(std::vector<unsigned int>& pixels, int stride, const RECT& rect,
                  unsigned char a, unsigned char r, unsigned char g, unsigned char b)
{
    if (a == 0) {
        return;
    }
    const unsigned char pr = static_cast<unsigned char>((static_cast<int>(r) * a) / 255);
    const unsigned char pg = static_cast<unsigned char>((static_cast<int>(g) * a) / 255);
    const unsigned char pb = static_cast<unsigned char>((static_cast<int>(b) * a) / 255);
    const unsigned int color = (static_cast<unsigned int>(a) << 24) | (static_cast<unsigned int>(pr) << 16) | (static_cast<unsigned int>(pg) << 8) | pb;
    for (int y = rect.top; y < rect.bottom; ++y) {
        unsigned int* row = pixels.data() + y * stride;
        for (int x = rect.left; x < rect.right; ++x) {
            row[x] = color;
        }
    }
}

void fillRoundRectArgb(std::vector<unsigned int>& pixels, int stride, const RECT& rect,
                       int rx, int ry, unsigned char a, unsigned char r, unsigned char g, unsigned char b)
{
    if (a == 0) {
        return;
    }
    const unsigned char pr = static_cast<unsigned char>((static_cast<int>(r) * a) / 255);
    const unsigned char pg = static_cast<unsigned char>((static_cast<int>(g) * a) / 255);
    const unsigned char pb = static_cast<unsigned char>((static_cast<int>(b) * a) / 255);
    const unsigned int color = (static_cast<unsigned int>(a) << 24) | (static_cast<unsigned int>(pr) << 16) | (static_cast<unsigned int>(pg) << 8) | pb;
    for (int y = rect.top; y < rect.bottom; ++y) {
        unsigned int* row = pixels.data() + y * stride;
        for (int x = rect.left; x < rect.right; ++x) {
            if (insideRoundRect(x, y, rect, rx, ry)) {
                row[x] = color;
            }
        }
    }
}

void normalizeDibAlpha(void* bits, int stride, const RECT& rect, unsigned char alpha)
{
    auto* pixels = static_cast<unsigned int*>(bits);
    for (int y = rect.top; y < rect.bottom; ++y) {
        unsigned int* row = pixels + y * stride;
        for (int x = rect.left; x < rect.right; ++x) {
            const unsigned int pixel = row[x];
            const unsigned char b = static_cast<unsigned char>(pixel & 0xFF);
            const unsigned char g = static_cast<unsigned char>((pixel >> 8) & 0xFF);
            const unsigned char r = static_cast<unsigned char>((pixel >> 16) & 0xFF);
            const unsigned char pr = static_cast<unsigned char>((static_cast<int>(r) * alpha) / 255);
            const unsigned char pg = static_cast<unsigned char>((static_cast<int>(g) * alpha) / 255);
            const unsigned char pb = static_cast<unsigned char>((static_cast<int>(b) * alpha) / 255);
            row[x] = (static_cast<unsigned int>(alpha) << 24) | (static_cast<unsigned int>(pr) << 16) | (static_cast<unsigned int>(pg) << 8) | pb;
        }
    }
}

void normalizeDibAlphaRound(void* bits, int stride, const RECT& rect, int rx, int ry, unsigned char alpha)
{
    auto* pixels = static_cast<unsigned int*>(bits);
    for (int y = rect.top; y < rect.bottom; ++y) {
        unsigned int* row = pixels + y * stride;
        for (int x = rect.left; x < rect.right; ++x) {
            if (!insideRoundRect(x, y, rect, rx, ry)) {
                continue;
            }
            const unsigned int pixel = row[x];
            const unsigned char b = static_cast<unsigned char>(pixel & 0xFF);
            const unsigned char g = static_cast<unsigned char>((pixel >> 8) & 0xFF);
            const unsigned char r = static_cast<unsigned char>((pixel >> 16) & 0xFF);
            const unsigned char pr = static_cast<unsigned char>((static_cast<int>(r) * alpha) / 255);
            const unsigned char pg = static_cast<unsigned char>((static_cast<int>(g) * alpha) / 255);
            const unsigned char pb = static_cast<unsigned char>((static_cast<int>(b) * alpha) / 255);
            row[x] = (static_cast<unsigned int>(alpha) << 24) | (static_cast<unsigned int>(pr) << 16) | (static_cast<unsigned int>(pg) << 8) | pb;
        }
    }
}

void overlayText(HDC dc, const RECT& r, const wchar_t* text, int pts, bool bold, COLORREF col, UINT fmt)
{
    HFONT f = CreateFontW(-pts, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT old = reinterpret_cast<HFONT>(SelectObject(dc, f));
    SetTextColor(dc, col);
    DrawTextW(dc, text, -1, const_cast<RECT*>(&r), fmt);
    SelectObject(dc, old);
    DeleteObject(f);
}

void overlayMono(HDC dc, const RECT& r, const wchar_t* text, int pts, COLORREF col)
{
    HFONT f = CreateFontW(-pts, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_MODERN, L"Consolas");
    HFONT old = reinterpret_cast<HFONT>(SelectObject(dc, f));
    SetTextColor(dc, col);
    DrawTextW(dc, text, -1, const_cast<RECT*>(&r), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, old);
    DeleteObject(f);
}

} // namespace

OverlayWindow::~OverlayWindow()
{
    destroy();
}

bool OverlayWindow::create(HINSTANCE instance, Settings settings)
{
    instance_ = instance;
    settings_ = settings;
    settings_.clamp();
    std::wostringstream startLine;
    startLine << L"Overlay create start x=" << settings_.x
              << L" y=" << settings_.y
              << L" size=" << settings_.outerWidth() << L"x" << settings_.outerHeight();
    Log::write(startLine.str());
    registerOverlayClass(instance_);

    hwnd_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        OverlayClassName,
        L"Focus Strip",
        WS_POPUP,
        settings_.x,
        settings_.y,
        settings_.outerWidth(),
        settings_.outerHeight(),
        nullptr,
        nullptr,
        instance_,
        this);

    if (!hwnd_) {
        std::wostringstream line;
        line << L"Overlay CreateWindowEx failed lastError=" << GetLastError();
        Log::write(line.str());
        return false;
    }

    std::wostringstream createdLine;
    createdLine << L"Overlay hwnd=0x" << std::hex << reinterpret_cast<uintptr_t>(hwnd_);
    Log::write(createdLine.str());
    updateWindowSize();
    render();
    if (settings_.visible) {
        show();
    }
    Log::write(L"Overlay create complete");
    return true;
}

void OverlayWindow::destroy()
{
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

void OverlayWindow::show()
{
    if (!hwnd_) {
        return;
    }
    visible_ = true;
    settings_.visible = true;
    settings_.sticky = false;
    updateWindowSize();
    render();
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    SetWindowPos(hwnd_, HWND_TOPMOST, settings_.x, settings_.y, settings_.outerWidth(), settings_.outerHeight(), SWP_NOACTIVATE | SWP_SHOWWINDOW);
    notifyVisibilityChanged();
}

void OverlayWindow::hide()
{
    if (!hwnd_) {
        return;
    }
    setSticky(false);
    visible_ = false;
    settings_.visible = false;
    ShowWindow(hwnd_, SW_HIDE);
    notifyVisibilityChanged();
}

void OverlayWindow::toggle()
{
    visible_ ? hide() : show();
}

void OverlayWindow::reset(const Settings& settings)
{
    settings_ = settings;
    settings_.clamp();
    setSticky(false);
    updateWindowSize();
    render();
    if (visible_) {
        SetWindowPos(hwnd_, HWND_TOPMOST, settings_.x, settings_.y, settings_.outerWidth(), settings_.outerHeight(), SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
    notifySettingsChanged();
}

void OverlayWindow::applySelectionRect(const RECT& selection)
{
    settings_.selectionWidth = std::max(100, static_cast<int>(selection.right - selection.left));
    settings_.selectionHeight = std::max(10, static_cast<int>(selection.bottom - selection.top));
    settings_.x = selection.left - settings_.marginLeft;
    settings_.y = selection.top - settings_.marginTop;
    settings_.visible = true;
    settings_.selectionConfigured = true;
    settings_.clamp();
    updateWindowSize();
    render();
    show();
    notifySettingsChanged();
}

LRESULT CALLBACK OverlayWindow::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    OverlayWindow* self = reinterpret_cast<OverlayWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<OverlayWindow*>(cs->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self ? self->handleMessage(message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT OverlayWindow::handleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_NCHITTEST: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hwnd_, &pt);
        return hitTestClient(pt) == HitTarget::None ? HTTRANSPARENT : HTCLIENT;
    }

    case WM_LBUTTONDOWN: {
        POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        Slider slider = Slider::None;
        const HitTarget target = hitTestClient(pt, &slider);
        if (settings_.sticky && target == HitTarget::None) {
            setSticky(false);
            notifySettingsChanged();
            return 0;
        }
        if (target == HitTarget::Move) {
            draggingMove_ = true;
            dragStart_ = pt;
            dragWindowStart_.x = settings_.x;
            dragWindowStart_.y = settings_.y;
            SetCapture(hwnd_);
        } else if (target == HitTarget::Follow) {
            setSticky(!settings_.sticky);
            notifySettingsChanged();
        } else if (target == HitTarget::Redraw) {
            if (resizeRequested_) {
                resizeRequested_();
            }
        } else if (target == HitTarget::Tune) {
            tuneOpen_ = !tuneOpen_;
            render();
        } else if (target == HitTarget::Hide) {
            hide();
        } else if (target == HitTarget::Slider && slider != Slider::None) {
            activeSlider_ = slider;
            updateSliderFromPoint(activeSlider_, pt.x);
            SetCapture(hwnd_);
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (draggingMove_) {
            settings_.x = dragWindowStart_.x + (pt.x - dragStart_.x);
            settings_.y = dragWindowStart_.y + (pt.y - dragStart_.y);
            SetWindowPos(hwnd_, HWND_TOPMOST, settings_.x, settings_.y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
        } else if (activeSlider_ != Slider::None) {
            updateSliderFromPoint(activeSlider_, pt.x);
        }
        return 0;
    }

    case WM_LBUTTONUP:
        if (settings_.sticky && !draggingMove_ && activeSlider_ == Slider::None) {
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            if (hitTestClient(pt) == HitTarget::None) {
                setSticky(false);
            }
        }
        if (draggingMove_ || activeSlider_ != Slider::None) {
            draggingMove_ = false;
            activeSlider_ = Slider::None;
            ReleaseCapture();
            notifySettingsChanged();
        }
        return 0;

    case WM_TIMER:
        if (wParam == StickyTimerId) {
            updateStickyTracking();
            processStickyKeys();
            return 0;
        }
        break;

    case StickyReleaseMessage:
        if (settings_.sticky) {
            Log::write(L"Sticky released by mouse click");
            setSticky(false);
            notifySettingsChanged();
        }
        return 0;

    case WM_DISPLAYCHANGE:
    case WM_DPICHANGED:
        updateWindowSize();
        render();
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd_, StickyTimerId);
        uninstallStickyMouseHook();
        hwnd_ = nullptr;
        return 0;
    }

    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

void OverlayWindow::render()
{
    if (!hwnd_) {
        return;
    }

    const int width = settings_.outerWidth();
    const int height = settings_.outerHeight();
    if (width <= 0 || height <= 0) {
        return;
    }

    // --- Pixel buffer (premultiplied ARGB) ---
    std::vector<unsigned int> pixels(static_cast<size_t>(width) * static_cast<size_t>(height), 0);

    // Dimming overlay (white with opacity)
    const unsigned char wa = static_cast<unsigned char>(std::clamp(settings_.opacity, 0.15, 1.0) * 255.0);
    RECT full = {0, 0, width, height};
    fillRectArgb(pixels, width, full, wa, 255, 255, 255);

    // Clear selection strip
    RECT sel = selectionRect();
    fillRectArgb(pixels, width, sel, 18, 255, 255, 255);

    // Teal selection border (2px, drawn directly in pixel buffer for correct alpha)
    {
        const int bw = 2;
        fillRectArgb(pixels, width, {sel.left, sel.top, sel.right, sel.top + bw}, 220, 94, 234, 212);
        fillRectArgb(pixels, width, {sel.left, sel.bottom - bw, sel.right, sel.bottom}, 220, 94, 234, 212);
        fillRectArgb(pixels, width, {sel.left, sel.top, sel.left + bw, sel.bottom}, 220, 94, 234, 212);
        fillRectArgb(pixels, width, {sel.right - bw, sel.top, sel.right, sel.bottom}, 220, 94, 234, 212);
    }

    const RECT hint = {}; // hint label removed

    // Pill toolbar background
    const RECT pill = pillRect();
    fillRoundRectArgb(pixels, width, pill, PillHeight / 2, PillHeight / 2, 238, 20, 24, 32);

    // Sliders zone background (when tuning)
    const RECT sz = tuneOpen_ ? slidersZoneRect() : RECT{};
    if (tuneOpen_) {
        fillRoundRectArgb(pixels, width, sz, 8, 8, 232, 22, 27, 36);
    }

    // --- GDI drawing into DIB ---
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap || !bits) {
        if (bitmap) {
            DeleteObject(bitmap);
        }
        DeleteDC(mem);
        ReleaseDC(nullptr, screen);
        return;
    }
    memcpy(bits, pixels.data(), pixels.size() * sizeof(unsigned int));
    HBITMAP oldBitmap = reinterpret_cast<HBITMAP>(SelectObject(mem, bitmap));
    SetBkMode(mem, TRANSPARENT);

    // Pill: button dividers
    {
        HPEN divPen = CreatePen(PS_SOLID, 1, RGB(38, 44, 56));
        HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(mem, divPen));
        const int bw = (pill.right - pill.left) / PillButtonCount;
        for (int i = 1; i < PillButtonCount; ++i) {
            const int x = pill.left + i * bw;
            MoveToEx(mem, x, pill.top + 8, nullptr);
            LineTo(mem, x, pill.bottom - 8);
        }
        SelectObject(mem, oldPen);
        DeleteObject(divPen);
    }

    // Pill: icon-only buttons (larger icons \u2014 no label text competing)
    {
        struct BtnDef {
            const wchar_t* icon;
            bool active;
        };
        const BtnDef btns[PillButtonCount] = {
            {L"\u2630", false},            // \u2630  Move
            {L"\u25CE", settings_.sticky}, // \u25CE  Follow
            {L"\u2196", false},            // \u2196  Redraw
            {L"\u2261", tuneOpen_},        // \u2261  Tune
            {L"\u2715", false},            // \u2715  Hide
        };
        for (int i = 0; i < PillButtonCount; ++i) {
            const RECT br = pillButtonRect(i);
            const COLORREF col = btns[i].active ? RGB(94, 234, 212) : RGB(148, 160, 178);
            overlayText(mem, br, btns[i].icon, 16, false, col,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }

    // Sliders (when tuning)
    if (tuneOpen_) {
        const struct {
            Slider id;
            const wchar_t* label;
        } sliderDefs[] = {
            {Slider::VerticalMargin,   L"V-Margin"},
            {Slider::HorizontalMargin, L"H-Margin"},
            {Slider::Opacity,          L"Opacity"},
            {Slider::SelectionHeight,  L"Height"},
            {Slider::SelectionWidth,   L"Width"},
        };
        for (const auto& sd : sliderDefs) {
            RECT track = sliderTrackRect(sd.id);
            RECT labelR = {track.left, sz.top + 4, track.right, sz.top + 16};
            overlayText(mem, labelR, sd.label, 7, false, RGB(80, 92, 108),
                        DT_CENTER | DT_SINGLELINE);

            // Track line
            HPEN trackPen = CreatePen(PS_SOLID, 2, RGB(44, 52, 66));
            HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(mem, trackPen));
            const int midY = track.top + heightOf(track) / 2;
            MoveToEx(mem, track.left, midY, nullptr);
            LineTo(mem, track.right, midY);
            SelectObject(mem, oldPen);
            DeleteObject(trackPen);

            // Knob
            double t = 0.0;
            switch (sd.id) {
            case Slider::VerticalMargin:   t = (settings_.marginTop - 20)       / 480.0; break;
            case Slider::HorizontalMargin: t = (settings_.marginLeft - 10)      / 290.0; break;
            case Slider::Opacity:          t = (settings_.opacity - 0.15)       / 0.85;  break;
            case Slider::SelectionHeight:  t = (settings_.selectionHeight - 10) / 190.0; break;
            case Slider::SelectionWidth:   t = (settings_.selectionWidth - 100) / 1900.0; break;
            default: break;
            }
            t = std::clamp(t, 0.0, 1.0);
            const int kx = track.left + static_cast<int>(std::round(t * widthOf(track)));
            RECT knob = {kx - 4, track.top, kx + 4, track.bottom};
            HBRUSH knobBr = CreateSolidBrush(RGB(94, 234, 212));
            FillRect(mem, &knob, knobBr);
            DeleteObject(knobBr);
        }
    }

    // Fix alpha for rounded-corner regions
    normalizeDibAlphaRound(bits, width, pill, PillHeight / 2, PillHeight / 2, 238);
    if (tuneOpen_) {
        normalizeDibAlphaRound(bits, width, sz, 8, 8, 232);
    }

    POINT src = {0, 0};
    POINT dst = {settings_.x, settings_.y};
    SIZE size = {width, height};
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(hwnd_, screen, &dst, &size, mem, &src, 0, &blend, ULW_ALPHA);

    SelectObject(mem, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
}

void OverlayWindow::updateWindowSize()
{
    if (!hwnd_) {
        return;
    }
    settings_.clamp();
    SetWindowPos(hwnd_, HWND_TOPMOST, settings_.x, settings_.y, settings_.outerWidth(), settings_.outerHeight(), SWP_NOACTIVATE | (visible_ ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
}

void OverlayWindow::notifySettingsChanged()
{
    if (settingsChanged_) {
        settingsChanged_(settings_);
    }
}

void OverlayWindow::notifyVisibilityChanged()
{
    if (visibilityChanged_) {
        visibilityChanged_(settings_);
    }
}

void OverlayWindow::setSticky(bool enabled)
{
    settings_.sticky = enabled;
    if (!hwnd_) {
        return;
    }
    if (enabled) {
        SetTimer(hwnd_, StickyTimerId, StickyTimerMs, nullptr);
        installStickyMouseHook();
        updateStickyTracking();
    } else {
        KillTimer(hwnd_, StickyTimerId);
        uninstallStickyMouseHook();
        keyUpDown_ = keyDownDown_ = keyLeftDown_ = keyRightDown_ = false;
    }
    render();
}

void OverlayWindow::installStickyMouseHook()
{
    if (stickyMouseHook_) {
        return;
    }
    g_stickyMouseHwnd = hwnd_;
    stickyMouseHook_ = SetWindowsHookExW(WH_MOUSE_LL, StickyMouseProc, GetModuleHandleW(nullptr), 0);
    std::wostringstream line;
    line << L"Sticky mouse hook install result=" << (stickyMouseHook_ ? L"ok" : L"failed")
         << L" hook=0x" << std::hex << reinterpret_cast<uintptr_t>(stickyMouseHook_)
         << L" lastError=" << std::dec << GetLastError();
    Log::write(line.str());
}

void OverlayWindow::uninstallStickyMouseHook()
{
    if (!stickyMouseHook_) {
        return;
    }
    Log::write(L"Sticky mouse hook uninstall");
    UnhookWindowsHookEx(stickyMouseHook_);
    stickyMouseHook_ = nullptr;
    if (g_stickyMouseHwnd == hwnd_) {
        g_stickyMouseHwnd = nullptr;
    }
}

void OverlayWindow::updateStickyTracking()
{
    if (!settings_.sticky || !visible_) {
        return;
    }
    POINT cursor = {};
    GetCursorPos(&cursor);
    settings_.x = cursor.x - settings_.outerWidth();
    settings_.y = cursor.y - settings_.outerHeight();
    SetWindowPos(hwnd_, HWND_TOPMOST, settings_.x, settings_.y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
}

void OverlayWindow::processStickyKeys()
{
    if (!settings_.sticky) {
        return;
    }

    const bool up = (GetAsyncKeyState(VK_UP) & 0x8000) != 0;
    const bool down = (GetAsyncKeyState(VK_DOWN) & 0x8000) != 0;
    const bool left = (GetAsyncKeyState(VK_LEFT) & 0x8000) != 0;
    const bool right = (GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0;

    if (up && !keyUpDown_) {
        resizeSelection(0, -10);
    }
    if (down && !keyDownDown_) {
        resizeSelection(0, 10);
    }
    if (left && !keyLeftDown_) {
        resizeSelection(-50, 0);
    }
    if (right && !keyRightDown_) {
        resizeSelection(50, 0);
    }

    keyUpDown_ = up;
    keyDownDown_ = down;
    keyLeftDown_ = left;
    keyRightDown_ = right;
}

void OverlayWindow::resizeSelection(int deltaWidth, int deltaHeight)
{
    settings_.selectionWidth = std::max(100, settings_.selectionWidth + deltaWidth);
    settings_.selectionHeight = std::max(10, settings_.selectionHeight + deltaHeight);
    updateWindowSize();
    render();
    notifySettingsChanged();
}

RECT OverlayWindow::selectionRect() const
{
    return {
        settings_.marginLeft,
        settings_.marginTop,
        settings_.marginLeft + settings_.selectionWidth,
        settings_.marginTop + settings_.selectionHeight
    };
}

RECT OverlayWindow::pillRect() const
{
    const int w = settings_.outerWidth();
    const int h = settings_.outerHeight();
    const int pw = std::clamp(w - 16, 220, 400);
    return {w - pw - 8, h - PillBottomPad - PillHeight, w - 8, h - PillBottomPad};
}

RECT OverlayWindow::pillButtonRect(int index) const
{
    const RECT p = pillRect();
    const int bw = (p.right - p.left) / PillButtonCount;
    return {p.left + index * bw, p.top, p.left + (index + 1) * bw, p.bottom};
}

RECT OverlayWindow::hintLabelRect() const
{
    const int w = settings_.outerWidth();
    const int hw = std::min(420, w - 32);
    return {(w - hw) / 2, 6, (w - hw) / 2 + hw, 6 + HintHeight};
}

RECT OverlayWindow::slidersZoneRect() const
{
    const RECT p = pillRect();
    return {p.left, p.top - 6 - SlidersZoneHeight, p.right, p.top - 6};
}

RECT OverlayWindow::sliderTrackRect(Slider slider) const
{
    const RECT zone = slidersZoneRect();
    const int gap = 10;
    const int count = 5;
    const int tw = std::max(40, (widthOf(zone) - (count - 1) * gap) / count);
    int index = 0;
    switch (slider) {
    case Slider::VerticalMargin:   index = 0; break;
    case Slider::HorizontalMargin: index = 1; break;
    case Slider::Opacity:          index = 2; break;
    case Slider::SelectionHeight:  index = 3; break;
    case Slider::SelectionWidth:   index = 4; break;
    default: return {};
    }
    const int x = zone.left + index * (tw + gap);
    const int y = zone.top + 18;
    return {x, y, x + tw, y + 12};
}

OverlayWindow::HitTarget OverlayWindow::hitTestClient(POINT pt, Slider* slider) const
{
    if (slider) {
        *slider = Slider::None;
    }

    if (settings_.sticky) {
        return HitTarget::Follow;
    }

    // Check pill buttons
    const RECT pill = pillRect();
    if (contains(pill, pt)) {
        const int bw = (pill.right - pill.left) / PillButtonCount;
        const int idx = (pt.x - pill.left) / std::max(1, bw);
        switch (std::clamp(idx, 0, PillButtonCount - 1)) {
        case 0: return HitTarget::Move;
        case 1: return HitTarget::Follow;
        case 2: return HitTarget::Redraw;
        case 3: return HitTarget::Tune;
        case 4: return HitTarget::Hide;
        }
    }

    // Check sliders (only when tune panel is open)
    if (tuneOpen_) {
        const Slider sliders[] = {
            Slider::VerticalMargin,
            Slider::HorizontalMargin,
            Slider::Opacity,
            Slider::SelectionHeight,
            Slider::SelectionWidth,
        };
        for (Slider item : sliders) {
            RECT r = sliderTrackRect(item);
            InflateRect(&r, 8, 12);
            if (contains(r, pt)) {
                if (slider) {
                    *slider = item;
                }
                return HitTarget::Slider;
            }
        }
    }

    if (!contains(selectionRect(), pt)) {
        return HitTarget::Follow;
    }
    return HitTarget::None;
}

void OverlayWindow::updateSliderFromPoint(Slider slider, int x)
{
    RECT track = sliderTrackRect(slider);
    const double t = std::clamp((x - track.left) / static_cast<double>(std::max(1, widthOf(track))), 0.0, 1.0);
    switch (slider) {
    case Slider::VerticalMargin:
        settings_.marginTop = settings_.marginBottom = 20 + static_cast<int>(std::round(t * 480.0));
        break;
    case Slider::HorizontalMargin:
        settings_.marginLeft = settings_.marginRight = 10 + static_cast<int>(std::round(t * 290.0));
        break;
    case Slider::Opacity:
        settings_.opacity = 0.15 + t * 0.85;
        break;
    case Slider::SelectionHeight:
        settings_.selectionHeight = 10 + static_cast<int>(std::round(t * 190.0));
        break;
    case Slider::SelectionWidth:
        settings_.selectionWidth = 100 + static_cast<int>(std::round(t * 1900.0));
        break;
    default:
        break;
    }
    settings_.clamp();
    updateWindowSize();
    render();
    notifySettingsChanged();
}
