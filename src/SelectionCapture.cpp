#include "SelectionCapture.h"
#include "Logger.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <sstream>
#include <utility>
#include <vector>
#include <windowsx.h>

namespace {

const wchar_t* CaptureClassName = L"FocusStripSelectionCapture";
constexpr int ToolbarWidth = 360;
constexpr int ToolbarHeight = 48;
constexpr int ToolbarTop = 18;

int widthOf(const RECT& rect) { return rect.right - rect.left; }
int heightOf(const RECT& rect) { return rect.bottom - rect.top; }

void fillRectArgb(std::vector<unsigned int>& pixels, int stride, const RECT& rect, unsigned char a, unsigned char r, unsigned char g, unsigned char b)
{
    const int height = static_cast<int>(pixels.size() / std::max(1, stride));
    const int left = std::clamp(static_cast<int>(rect.left), 0, stride);
    const int top = std::clamp(static_cast<int>(rect.top), 0, height);
    const int right = std::clamp(static_cast<int>(rect.right), 0, stride);
    const int bottom = std::clamp(static_cast<int>(rect.bottom), 0, height);
    const unsigned char pr = static_cast<unsigned char>((static_cast<int>(r) * a) / 255);
    const unsigned char pg = static_cast<unsigned char>((static_cast<int>(g) * a) / 255);
    const unsigned char pb = static_cast<unsigned char>((static_cast<int>(b) * a) / 255);
    const unsigned int color = (static_cast<unsigned int>(a) << 24) | (static_cast<unsigned int>(pr) << 16) | (static_cast<unsigned int>(pg) << 8) | pb;
    for (int y = top; y < bottom; ++y) {
        unsigned int* row = pixels.data() + y * stride;
        for (int x = left; x < right; ++x) {
            row[x] = color;
        }
    }
}

void clearRect(std::vector<unsigned int>& pixels, int stride, const RECT& rect)
{
    const int height = static_cast<int>(pixels.size() / std::max(1, stride));
    const int left = std::clamp(static_cast<int>(rect.left), 0, stride);
    const int top = std::clamp(static_cast<int>(rect.top), 0, height);
    const int right = std::clamp(static_cast<int>(rect.right), 0, stride);
    const int bottom = std::clamp(static_cast<int>(rect.bottom), 0, height);
    for (int y = top; y < bottom; ++y) {
        unsigned int* row = pixels.data() + y * stride;
        for (int x = left; x < right; ++x) {
            row[x] = 0;
        }
    }
}

void normalizeDibAlpha(void* bits, int stride, const RECT& rect, unsigned char alpha)
{
    auto* pixels = static_cast<unsigned int*>(bits);
    for (int y = std::max(0L, rect.top); y < rect.bottom; ++y) {
        unsigned int* row = pixels + y * stride;
        for (int x = std::max(0L, rect.left); x < rect.right; ++x) {
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

void registerCaptureClass(HINSTANCE instance)
{
    static bool registered = false;
    if (registered) {
        return;
    }

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = SelectionCapture::WndProc;
    wc.lpszClassName = CaptureClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    const ATOM atom = RegisterClassExW(&wc);
    std::wostringstream line;
    line << L"SelectionCapture RegisterClass atom=" << atom << L" lastError=" << GetLastError();
    Log::write(line.str());
    registered = true;
}

} // namespace

SelectionCapture::~SelectionCapture()
{
    cancel();
}

bool SelectionCapture::begin(HINSTANCE instance, CompleteCallback complete, CancelCallback cancelCallback)
{
    if (hwnd_) {
        Log::write(L"SelectionCapture begin ignored: already active");
        return false;
    }

    instance_ = instance;
    complete_ = std::move(complete);
    cancel_ = std::move(cancelCallback);
    registerCaptureClass(instance_);

    RECT vs = virtualScreen();
    std::wostringstream startLine;
    startLine << L"SelectionCapture begin virtualScreen=(" << vs.left << L"," << vs.top << L") "
              << widthOf(vs) << L"x" << heightOf(vs);
    Log::write(startLine.str());

    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        CaptureClassName,
        L"Focus Strip Selection Capture",
        WS_POPUP,
        vs.left,
        vs.top,
        vs.right - vs.left,
        vs.bottom - vs.top,
        nullptr,
        nullptr,
        instance_,
        this);

    if (!hwnd_) {
        std::wostringstream line;
        line << L"SelectionCapture CreateWindowEx failed lastError=" << GetLastError();
        Log::write(line.str());
        return false;
    }

    std::wostringstream createdLine;
    createdLine << L"SelectionCapture hwnd=0x" << std::hex << reinterpret_cast<uintptr_t>(hwnd_);
    Log::write(createdLine.str());
    render();
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    HWND captured = SetCapture(hwnd_);
    const BOOL focusOk = SetForegroundWindow(hwnd_);
    SetFocus(hwnd_);
    std::wostringstream shownLine;
    shownLine << L"SelectionCapture shown setCapturePrevious=0x" << std::hex << reinterpret_cast<uintptr_t>(captured)
              << L" setForeground=" << std::dec << focusOk
              << L" visible=" << IsWindowVisible(hwnd_);
    Log::write(shownLine.str());
    return true;
}

void SelectionCapture::cancel()
{
    if (!hwnd_) {
        return;
    }
    HWND old = hwnd_;
    hwnd_ = nullptr;
    ReleaseCapture();
    DestroyWindow(old);
    Log::write(L"SelectionCapture cancel");
    if (cancel_) {
        cancel_();
    }
}

LRESULT CALLBACK SelectionCapture::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    SelectionCapture* self = reinterpret_cast<SelectionCapture*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<SelectionCapture*>(cs->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self ? self->handleMessage(message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT SelectionCapture::handleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_LBUTTONDOWN:
        Log::write(L"SelectionCapture WM_LBUTTONDOWN");
        drawing_ = true;
        start_.x = GET_X_LPARAM(lParam);
        start_.y = GET_Y_LPARAM(lParam);
        current_ = start_;
        render();
        return 0;

    case WM_MOUSEMOVE:
        if (drawing_) {
            current_.x = GET_X_LPARAM(lParam);
            current_.y = GET_Y_LPARAM(lParam);
            render();
        }
        return 0;

    case WM_LBUTTONUP: {
        if (!drawing_) {
            return 0;
        }
        drawing_ = false;
        current_.x = GET_X_LPARAM(lParam);
        current_.y = GET_Y_LPARAM(lParam);
        RECT selection = normalizedSelection();
        RECT vs = virtualScreen();
        OffsetRect(&selection, vs.left, vs.top);
        std::wostringstream line;
        line << L"SelectionCapture WM_LBUTTONUP selection=("
             << selection.left << L"," << selection.top << L") "
             << (selection.right - selection.left) << L"x" << (selection.bottom - selection.top);
        Log::write(line.str());

        HWND old = hwnd_;
        hwnd_ = nullptr;
        ReleaseCapture();
        DestroyWindow(old);

        if ((selection.right - selection.left) >= 10 && (selection.bottom - selection.top) >= 5) {
            if (complete_) {
                complete_(selection);
            }
        } else if (cancel_) {
            Log::write(L"SelectionCapture selection too small; cancel callback");
            cancel_();
        }
        return 0;
    }

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            Log::write(L"SelectionCapture Escape");
            cancel();
            return 0;
        }
        break;

    case WM_PAINT:
        ValidateRect(hwnd_, nullptr);
        render();
        return 0;

    case WM_DESTROY:
        if (hwnd_ == hwnd_) {
            hwnd_ = nullptr;
        }
        return 0;
    }

    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

void SelectionCapture::render()
{
    if (!hwnd_) {
        return;
    }

    RECT client = {};
    GetClientRect(hwnd_, &client);
    const int width = widthOf(client);
    const int height = heightOf(client);
    if (width <= 0 || height <= 0) {
        Log::write(L"SelectionCapture render skipped: empty client");
        return;
    }

    std::vector<unsigned int> pixels(static_cast<size_t>(width) * static_cast<size_t>(height), 0);
    fillRectArgb(pixels, width, client, 86, 0, 0, 0);

    RECT rect = normalizedSelection();
    if (drawing_ && widthOf(rect) > 0 && heightOf(rect) > 0) {
        clearRect(pixels, width, rect);
    }

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC screen = GetDC(nullptr);
    HDC memory = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap || !bits) {
        std::wostringstream line;
        line << L"SelectionCapture CreateDIBSection failed bitmap=" << bitmap << L" bits=" << bits << L" lastError=" << GetLastError();
        Log::write(line.str());
        if (bitmap) {
            DeleteObject(bitmap);
        }
        DeleteDC(memory);
        ReleaseDC(nullptr, screen);
        return;
    }

    memcpy(bits, pixels.data(), pixels.size() * sizeof(unsigned int));
    HBITMAP oldBitmap = reinterpret_cast<HBITMAP>(SelectObject(memory, bitmap));

    drawToolbar(memory, width);

    if (drawing_ && widthOf(rect) > 0 && heightOf(rect) > 0) {
        HPEN border = CreatePen(PS_DASH, 1, RGB(255, 255, 255));
        HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(memory, border));
        HBRUSH oldBrush = reinterpret_cast<HBRUSH>(SelectObject(memory, GetStockObject(NULL_BRUSH)));
        Rectangle(memory, rect.left, rect.top, rect.right, rect.bottom);
        SelectObject(memory, oldBrush);
        SelectObject(memory, oldPen);
        DeleteObject(border);

        RECT borderBand = rect;
        InflateRect(&borderBand, 3, 3);
        borderBand.left = std::max<LONG>(0, borderBand.left);
        borderBand.top = std::max<LONG>(0, borderBand.top);
        borderBand.right = std::min<LONG>(width, borderBand.right);
        borderBand.bottom = std::min<LONG>(height, borderBand.bottom);

        RECT top = { borderBand.left, borderBand.top, borderBand.right, std::min<LONG>(borderBand.top + 7, borderBand.bottom) };
        RECT bottom = { borderBand.left, std::max<LONG>(borderBand.bottom - 7, borderBand.top), borderBand.right, borderBand.bottom };
        RECT left = { borderBand.left, borderBand.top, std::min<LONG>(borderBand.left + 7, borderBand.right), borderBand.bottom };
        RECT right = { std::max<LONG>(borderBand.right - 7, borderBand.left), borderBand.top, borderBand.right, borderBand.bottom };
        normalizeDibAlpha(bits, width, top, 255);
        normalizeDibAlpha(bits, width, bottom, 255);
        normalizeDibAlpha(bits, width, left, 255);
        normalizeDibAlpha(bits, width, right, 255);
    }

    RECT toolbar = { (width - ToolbarWidth) / 2, ToolbarTop, (width + ToolbarWidth) / 2, ToolbarTop + ToolbarHeight };
    normalizeDibAlpha(bits, width, toolbar, 246);

    POINT src = { 0, 0 };
    RECT vs = virtualScreen();
    POINT dst = { vs.left, vs.top };
    SIZE size = { width, height };
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    const BOOL updated = UpdateLayeredWindow(hwnd_, screen, &dst, &size, memory, &src, 0, &blend, ULW_ALPHA);
    if (!updated) {
        std::wostringstream line;
        line << L"SelectionCapture UpdateLayeredWindow failed lastError=" << GetLastError()
             << L" size=" << width << L"x" << height;
        Log::write(line.str());
    }

    SelectObject(memory, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(nullptr, screen);
}

void SelectionCapture::drawToolbar(HDC dc, int width)
{
    RECT toolbar = { (width - ToolbarWidth) / 2, ToolbarTop, (width + ToolbarWidth) / 2, ToolbarTop + ToolbarHeight };

    HBRUSH fill = CreateSolidBrush(RGB(252, 252, 252));
    HBRUSH oldBrush = reinterpret_cast<HBRUSH>(SelectObject(dc, fill));
    HPEN outline = CreatePen(PS_SOLID, 1, RGB(205, 205, 205));
    HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(dc, outline));
    RoundRect(dc, toolbar.left, toolbar.top, toolbar.right, toolbar.bottom, 8, 8);

    RECT icon = { toolbar.left + 16, toolbar.top + 12, toolbar.left + 40, toolbar.top + 36 };
    HBRUSH selected = CreateSolidBrush(RGB(232, 244, 255));
    FillRect(dc, &icon, selected);
    Rectangle(dc, icon.left, icon.top, icon.right, icon.bottom);

    RECT label = { toolbar.left + 52, toolbar.top + 7, toolbar.right - 16, toolbar.top + 28 };
    RECT help = { toolbar.left + 52, toolbar.top + 27, toolbar.right - 16, toolbar.bottom - 7 };

    HFONT titleFont = CreateFontW(-14, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT smallFont = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(dc, titleFont));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(35, 35, 35));
    DrawTextW(dc, L"Draw reading selection", -1, &label, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(dc, smallFont);
    SetTextColor(dc, RGB(95, 95, 95));
    DrawTextW(dc, L"Drag a line or block. Esc cancels.", -1, &help, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    SelectObject(dc, oldFont);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(titleFont);
    DeleteObject(smallFont);
    DeleteObject(fill);
    DeleteObject(selected);
    DeleteObject(outline);
}

RECT SelectionCapture::virtualScreen() const
{
    RECT rect = {};
    rect.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    rect.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    rect.right = rect.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    rect.bottom = rect.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    return rect;
}

RECT SelectionCapture::normalizedSelection() const
{
    RECT rect = {};
    rect.left = std::min(start_.x, current_.x);
    rect.top = std::min(start_.y, current_.y);
    rect.right = std::max(start_.x, current_.x);
    rect.bottom = std::max(start_.y, current_.y);
    return rect;
}
