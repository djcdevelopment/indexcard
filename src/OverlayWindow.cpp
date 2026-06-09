#include "OverlayWindow.h"
#include "Logger.h"
#include "Theme.h"

#include <algorithm>
#include <sstream>
#include <cstring>
#include <cmath>
#include <memory>
#include <string>
#include <vector>
#include <windowsx.h>

namespace {

const wchar_t* OverlayClassName = L"IndexCardOverlayWindow";
constexpr UINT StickyTimerId = 1;
constexpr UINT StickyReleaseMessage = WM_APP + 30;
constexpr int StickyTimerMs = 16;
constexpr int PillHeight = 44;
constexpr int PillBottomPad = 10;
constexpr int PillButtonCount = 2;

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
    const DWORD err = GetLastError();
    std::wostringstream line;
    line << L"Overlay RegisterClass atom=" << atom << L" lastError=" << err;
    Log::write(line.str());
    if (atom != 0 || err == ERROR_CLASS_ALREADY_EXISTS) {
        registered = true;
    }
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
        L"IndexCard",
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
    rebuildFonts();
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
    if (dibDC_) { DeleteDC(dibDC_); dibDC_ = nullptr; }
    if (dibBitmap_) { DeleteObject(dibBitmap_); dibBitmap_ = nullptr; }
    dibBits_ = nullptr;
    dibCachedW_ = dibCachedH_ = 0;
    pixelBuf_.clear();

    if (pillIconFont_) { DeleteObject(pillIconFont_); pillIconFont_ = nullptr; }
    if (dividerPen_)   { DeleteObject(dividerPen_);   dividerPen_   = nullptr; }
}

void OverlayWindow::rebuildDib(int w, int h)
{
    if (w <= 0 || h <= 0) {
        return;
    }
    if (dibDC_) { DeleteDC(dibDC_); dibDC_ = nullptr; }
    if (dibBitmap_) { DeleteObject(dibBitmap_); dibBitmap_ = nullptr; }
    dibBits_ = nullptr;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = -h;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC screen = GetDC(nullptr);
    dibDC_     = CreateCompatibleDC(screen);
    dibBitmap_ = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &dibBits_, nullptr, 0);
    ReleaseDC(nullptr, screen);

    if (!dibDC_ || !dibBitmap_ || !dibBits_) {
        if (dibBitmap_) { DeleteObject(dibBitmap_); dibBitmap_ = nullptr; }
        if (dibDC_)     { DeleteDC(dibDC_);         dibDC_     = nullptr; }
        dibBits_ = nullptr;
        Log::write(L"rebuildDib: CreateDIBSection failed");
        return;
    }

    SelectObject(dibDC_, dibBitmap_);
    SetBkMode(dibDC_, TRANSPARENT);
    pixelBuf_.assign(static_cast<size_t>(w) * h, 0u);
    dibCachedW_ = w;
    dibCachedH_ = h;
}

void OverlayWindow::rebuildFonts()
{
    if (pillIconFont_) { DeleteObject(pillIconFont_); }
    if (dividerPen_)   { DeleteObject(dividerPen_);   }

    pillIconFont_ = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    dividerPen_ = CreatePen(PS_SOLID, 1, RGB(38, 44, 56));
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
        const HitTarget target = hitTestClient(pt);
        if (target == HitTarget::Follow) {
            setSticky(!settings_.sticky);
            notifySettingsChanged();
        } else if (target == HitTarget::Redraw) {
            if (resizeRequested_) {
                resizeRequested_();
            }
        } else if (target == HitTarget::Hide) {
            hide();
        }
        return 0;
    }

    case WM_LBUTTONUP:
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
    if (!hwnd_ || !dibBits_) {
        return;
    }

    const int width  = dibCachedW_;
    const int height = dibCachedH_;

    std::fill(pixelBuf_.begin(), pixelBuf_.end(), 0u);

    // Dimming overlay
    const unsigned char wa = static_cast<unsigned char>(std::clamp(settings_.opacity, 0.15, 1.0) * 255.0);
    RECT full = {0, 0, width, height};
    fillRectArgb(pixelBuf_, width, full, wa, 255, 255, 255);

    // Clear selection strip
    RECT sel = selectionRect();
    fillRectArgb(pixelBuf_, width, sel, 18, 255, 255, 255);

    // Selection border
    {
        const int bw = settings_.borderWidth;
        fillRectArgb(pixelBuf_, width, {sel.left, sel.top, sel.right, sel.top + bw},      220, Theme::AccentR, Theme::AccentG, Theme::AccentB);
        fillRectArgb(pixelBuf_, width, {sel.left, sel.bottom - bw, sel.right, sel.bottom}, 220, Theme::AccentR, Theme::AccentG, Theme::AccentB);
        fillRectArgb(pixelBuf_, width, {sel.left, sel.top, sel.left + bw, sel.bottom},    220, Theme::AccentR, Theme::AccentG, Theme::AccentB);
        fillRectArgb(pixelBuf_, width, {sel.right - bw, sel.top, sel.right, sel.bottom},  220, Theme::AccentR, Theme::AccentG, Theme::AccentB);
    }

    // Pill toolbar background
    const RECT pill = pillRect();
    fillRoundRectArgb(pixelBuf_, width, pill, PillHeight / 2, PillHeight / 2, 238, 20, 24, 32);

    memcpy(dibBits_, pixelBuf_.data(), pixelBuf_.size() * sizeof(unsigned int));

    // Pill: divider between the two buttons
    {
        HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(dibDC_, dividerPen_));
        const int x = pill.left + (pill.right - pill.left) / PillButtonCount;
        MoveToEx(dibDC_, x, pill.top + 8, nullptr);
        LineTo(dibDC_, x, pill.bottom - 8);
        SelectObject(dibDC_, oldPen);
    }

    // Pill: \u25A1 (redraw) and \u2715 (hide)
    {
        const wchar_t* icons[PillButtonCount] = {L"\u25A1", L"\u2715"};
        HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(dibDC_, pillIconFont_));
        SetTextColor(dibDC_, RGB(148, 160, 178));
        for (int i = 0; i < PillButtonCount; ++i) {
            const RECT br = pillButtonRect(i);
            DrawTextW(dibDC_, icons[i], -1, const_cast<RECT*>(&br), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        SelectObject(dibDC_, oldFont);
    }

    normalizeDibAlphaRound(dibBits_, width, pill, PillHeight / 2, PillHeight / 2, 238);

    POINT src = {0, 0};
    POINT dst = {settings_.x, settings_.y};
    SIZE  size = {width, height};
    BLENDFUNCTION blend = {};
    blend.BlendOp             = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat         = AC_SRC_ALPHA;
    HDC screen = GetDC(nullptr);
    UpdateLayeredWindow(hwnd_, screen, &dst, &size, dibDC_, &src, 0, &blend, ULW_ALPHA);
    ReleaseDC(nullptr, screen);
}

void OverlayWindow::updateWindowSize()
{
    if (!hwnd_) {
        return;
    }
    settings_.clamp();
    const int w = settings_.outerWidth();
    const int h = settings_.outerHeight();
    if (w != dibCachedW_ || h != dibCachedH_) {
        rebuildDib(w, h);
    }
    SetWindowPos(hwnd_, HWND_TOPMOST, settings_.x, settings_.y, w, h, SWP_NOACTIVATE | (visible_ ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
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

OverlayWindow::HitTarget OverlayWindow::hitTestClient(POINT pt) const
{
    if (settings_.sticky) {
        return HitTarget::Follow;
    }

    const RECT pill = pillRect();
    if (contains(pill, pt)) {
        const int bw = (pill.right - pill.left) / PillButtonCount;
        const int idx = std::clamp(static_cast<int>(pt.x - pill.left) / std::max(1, bw), 0, PillButtonCount - 1);
        return idx == 0 ? HitTarget::Redraw : HitTarget::Hide;
    }

    return contains(selectionRect(), pt) ? HitTarget::None : HitTarget::Follow;
}
