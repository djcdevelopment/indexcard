#include "SplashWindow.h"
#include <windowsx.h>

namespace {

const wchar_t* SplashClassName = L"FocusStripSplash";
bool g_splashRegistered = false;

void registerSplashClass(HINSTANCE instance)
{
    if (g_splashRegistered) {
        return;
    }
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = SplashWindow::WndProc;
    wc.lpszClassName = SplashClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);
    g_splashRegistered = true;
}

} // namespace

SplashWindow::~SplashWindow()
{
    destroy();
}

bool SplashWindow::create(HINSTANCE instance)
{
    instance_ = instance;
    registerSplashClass(instance_);

    // Query system DPI for physical pixel scaling
    HDC screen = GetDC(nullptr);
    dpi_ = GetDeviceCaps(screen, LOGPIXELSX);
    ReleaseDC(nullptr, screen);

    const int W = s(kW);
    const int H = s(kH);

    RECT work = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int wx = work.left + (work.right - work.left - W) / 2;
    const int wy = work.top + (work.bottom - work.top - H) / 2;

    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        SplashClassName,
        L"Reader",
        WS_POPUP,
        wx, wy, W, H,
        nullptr, nullptr, instance_, this);
    if (!hwnd_) {
        return false;
    }

    HRGN rgn = CreateRoundRectRgn(0, 0, W + 1, H + 1, s(20), s(20));
    SetWindowRgn(hwnd_, rgn, FALSE);

    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    UpdateWindow(hwnd_);
    return true;
}

void SplashWindow::destroy()
{
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

LRESULT CALLBACK SplashWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    SplashWindow* self = reinterpret_cast<SplashWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = reinterpret_cast<SplashWindow*>(cs->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self ? self->handleMessage(msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT SplashWindow::handleMessage(UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_ERASEBKGND:
        return TRUE;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd_, &ps);
        const int W = s(kW);
        const int H = s(kH);
        HDC mem = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, W, H);
        HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(mem, bmp));
        SetBkMode(mem, TRANSPARENT);
        paint(mem);
        BitBlt(hdc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
        SelectObject(mem, oldBmp);
        DeleteObject(bmp);
        DeleteDC(mem);
        EndPaint(hwnd_, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        const RECT dr = drawButtonRect();
        const RECT lr = laterButtonRect();
        if (PtInRect(&dr, pt)) {
            if (drawRequested_) {
                drawRequested_();
            }
            destroy();
        } else if (PtInRect(&lr, pt)) {
            if (dismissed_) {
                dismissed_();
            }
            destroy();
        }
        return 0;
    }

    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            if (dismissed_) {
                dismissed_();
            }
            destroy();
        }
        return 0;

    case WM_DESTROY:
        hwnd_ = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

// ---------------------------------------------------------------------------
// Internal drawing helpers (all coords in logical pixels, scaled via s())
// ---------------------------------------------------------------------------

namespace {

void splashRR(HDC dc, int x, int y, int w, int h, int rx,
              COLORREF fill, COLORREF border = 0, int bw = 0)
{
    HBRUSH br = CreateSolidBrush(fill);
    HPEN pen = bw > 0 ? CreatePen(PS_SOLID, bw, border)
                      : static_cast<HPEN>(GetStockObject(NULL_PEN));
    HBRUSH oldBr   = static_cast<HBRUSH>(SelectObject(dc, br));
    HPEN   oldPen  = static_cast<HPEN>(SelectObject(dc, pen));
    RoundRect(dc, x, y, x + w, y + h, rx, rx);
    SelectObject(dc, oldBr);
    SelectObject(dc, oldPen);
    DeleteObject(br);
    if (bw > 0) {
        DeleteObject(pen);
    }
}

void splashText(HDC dc, int x, int y, int w, int h,
                const wchar_t* text, int physPts, bool bold, COLORREF col, UINT fmt,
                const wchar_t* face = L"Segoe UI")
{
    HFONT f = CreateFontW(-physPts, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, face);
    HFONT old = static_cast<HFONT>(SelectObject(dc, f));
    SetTextColor(dc, col);
    RECT r = {x, y, x + w, y + h};
    DrawTextW(dc, text, -1, &r, fmt);
    SelectObject(dc, old);
    DeleteObject(f);
}

void splashKey(HDC dc, int x, int y, int w, int h, const wchar_t* label, int physPts)
{
    splashRR(dc, x, y, w, h, h / 3, RGB(42, 47, 56), RGB(62, 70, 84), 1);
    splashText(dc, x, y, w, h, label, physPts, true, RGB(195, 205, 218),
               DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

} // namespace

void SplashWindow::paint(HDC dc)
{
    const int W   = s(kW);
    const int H   = s(kH);
    const int pad = s(40);
    const int cw  = W - pad * 2;  // content width

    // Background
    {
        RECT full = {0, 0, W, H};
        HBRUSH bg = CreateSolidBrush(RGB(20, 24, 32));
        FillRect(dc, &full, bg);
        DeleteObject(bg);
    }

    // App icon (72x72 logical, centered)
    {
        const int iw = s(72);
        const int ih = s(72);
        const int ix = (W - iw) / 2;
        const int iy = s(28);
        splashRR(dc, ix, iy, iw, ih, s(18), RGB(16, 38, 36), RGB(94, 234, 212), s(2));
        // strip inside
        splashRR(dc, ix + s(14), iy + s(29), s(44), s(14), s(4), RGB(94, 234, 212));
    }

    // "READER · RUNNING IN TRAY"
    splashText(dc, pad, s(116), cw, s(18),
               L"READER  ·  RUNNING IN TRAY",
               s(9), false, RGB(88, 100, 118),
               DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Headline
    splashText(dc, pad, s(140), cw, s(70),
               L"A reading light for your screen.",
               s(22), true, RGB(228, 234, 244),
               DT_LEFT | DT_WORDBREAK);

    // Description
    splashText(dc, pad, s(216), cw, s(72),
               L"Dim everything but the line you’re on. Draw a strip over any "
               L"terminal, editor, or log — the band stays clear while the rest fades back.",
               s(12), false, RGB(108, 120, 138),
               DT_LEFT | DT_WORDBREAK);

    // Hint box
    {
        const int hbx = pad;
        const int hby = s(294);
        const int hbw = cw;
        const int hbh = s(56);
        splashRR(dc, hbx, hby, hbw, hbh, s(10), RGB(26, 30, 40), RGB(42, 48, 62), 1);

        const int kh  = s(22);
        const int ky  = hby + (hbh - kh) / 2;
        const int fps = s(9);   // font for keys / surrounding text

        // "Press" label
        splashText(dc, hbx + s(16), ky, s(36), kh,
                   L"Press", fps, false, RGB(108, 120, 138),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        int cx = hbx + s(56);
        // [Win]
        splashKey(dc, cx, ky, s(34), kh, L"Win", fps);  cx += s(34) + s(4);
        splashText(dc, cx, ky, s(12), kh, L"+", fps, false, RGB(80, 92, 108),
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);                  cx += s(12) + s(4);
        // [Shift]
        splashKey(dc, cx, ky, s(42), kh, L"Shift", fps);  cx += s(42) + s(4);
        splashText(dc, cx, ky, s(12), kh, L"+", fps, false, RGB(80, 92, 108),
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);                  cx += s(12) + s(4);
        // [W]
        splashKey(dc, cx, ky, s(24), kh, L"W", fps);  cx += s(24) + s(8);
        // trailing text
        splashText(dc, cx, ky, hbx + hbw - cx - s(16), kh,
                   L"to draw your first strip", fps, false, RGB(108, 120, 138),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    // Buttons
    {
        const RECT dr = drawButtonRect();
        const RECT lr = laterButtonRect();
        const int  bfps = s(12);

        splashRR(dc, dr.left, dr.top, dr.right - dr.left, dr.bottom - dr.top,
                 s(10), RGB(94, 234, 212));
        splashText(dc, dr.left, dr.top, dr.right - dr.left, dr.bottom - dr.top,
                   L"Draw a reading strip", bfps, true, RGB(12, 18, 26),
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        splashRR(dc, lr.left, lr.top, lr.right - lr.left, lr.bottom - lr.top,
                 s(10), RGB(34, 39, 50), RGB(52, 58, 72), 1);
        splashText(dc, lr.left, lr.top, lr.right - lr.left, lr.bottom - lr.top,
                   L"Maybe later", bfps, false, RGB(140, 152, 170),
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // Footer
    splashText(dc, pad, s(428), cw, s(20),
               L"it stays out of the way — click-through, no focus stealing",
               s(9), false, RGB(58, 66, 82),
               DT_CENTER | DT_VCENTER | DT_SINGLELINE, L"Consolas");
}

RECT SplashWindow::drawButtonRect() const
{
    return {s(40), s(362), s(310), s(410)};
}

RECT SplashWindow::laterButtonRect() const
{
    return {s(320), s(362), s(460), s(410)};
}
