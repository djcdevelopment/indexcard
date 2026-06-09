#include "Logger.h"

#include <fstream>
#include <mutex>
#include <shlobj.h>
#include <sstream>
#include <windows.h>

namespace {

std::mutex g_mutex;
std::wstring g_directory;
std::wstring g_path;

std::wstring appDataDirectory()
{
    wchar_t path[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path))) {
        return std::wstring(path) + L"\\IndexCard";
    }
    return L".\\FocusStrip";
}

std::string narrowAscii(const std::wstring& value)
{
    std::string result;
    result.reserve(value.size());
    for (wchar_t ch : value) {
        result.push_back(ch >= 0 && ch <= 127 ? static_cast<char>(ch) : '?');
    }
    return result;
}

std::wstring timestamp()
{
    SYSTEMTIME st = {};
    GetLocalTime(&st);
    wchar_t buffer[64] = {};
    swprintf_s(
        buffer,
        L"%04u-%02u-%02u %02u:%02u:%02u.%03u",
        st.wYear,
        st.wMonth,
        st.wDay,
        st.wHour,
        st.wMinute,
        st.wSecond,
        st.wMilliseconds);
    return buffer;
}

} // namespace

namespace Log {

void init()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_directory = appDataDirectory();
    g_path = g_directory + L"\\indexcard.log";
    CreateDirectoryW(g_directory.c_str(), nullptr);

    std::ofstream file(g_path, std::ios::binary | std::ios::app);
    file << "\n--- IndexCard log start " << narrowAscii(timestamp()) << " ---\n";
}

void write(const std::wstring& message)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_path.empty()) {
        g_directory = appDataDirectory();
        g_path = g_directory + L"\\indexcard.log";
        CreateDirectoryW(g_directory.c_str(), nullptr);
    }

    std::wostringstream debug;
    debug << L"[IndexCard] " << message << L"\n";
    OutputDebugStringW(debug.str().c_str());

    std::ofstream file(g_path, std::ios::binary | std::ios::app);
    file << narrowAscii(timestamp()) << " " << narrowAscii(message) << "\n";
}

void write(const wchar_t* message)
{
    write(std::wstring(message));
}

std::wstring path()
{
    if (g_path.empty()) {
        init();
    }
    return g_path;
}

}
