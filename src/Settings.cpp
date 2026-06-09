#include "Settings.h"
#include "Logger.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <shlobj.h>
#include <windows.h>

namespace {

std::wstring appDataDirectory()
{
    wchar_t path[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path))) {
        return std::wstring(path) + L"\\IndexCard";
    }
    return L".\\FocusStrip";
}

std::string readFileUtf8(const std::wstring& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool writeFileUtf8(const std::wstring& path, const std::string& data)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    file << data;
    return static_cast<bool>(file);
}

bool findValue(const std::string& json, const char* key, std::string& value)
{
    const std::string needle = std::string("\"") + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) {
        return false;
    }
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) {
        return false;
    }
    ++pos;
    while (pos < json.size() && isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }
    const size_t start = pos;
    while (pos < json.size() && json[pos] != ',' && json[pos] != '}' && json[pos] != '\n' && json[pos] != '\r') {
        ++pos;
    }
    value = json.substr(start, pos - start);
    value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char c) { return isspace(c); }), value.end());
    return !value.empty();
}

void readInt(const std::string& json, const char* key, int& target)
{
    std::string value;
    if (findValue(json, key, value)) {
        try {
            target = std::stoi(value);
        } catch (...) {
        }
    }
}

void readDouble(const std::string& json, const char* key, double& target)
{
    std::string value;
    if (findValue(json, key, value)) {
        try {
            target = std::stod(value);
        } catch (...) {
        }
    }
}

void readBool(const std::string& json, const char* key, bool& target)
{
    std::string value;
    if (findValue(json, key, value)) {
        if (value == "true") {
            target = true;
        } else if (value == "false") {
            target = false;
        }
    }
}

} // namespace

int Settings::outerWidth() const
{
    return selectionWidth + marginLeft + marginRight;
}

int Settings::outerHeight() const
{
    return selectionHeight + marginTop + marginBottom;
}

void Settings::clamp()
{
    selectionWidth = std::clamp(selectionWidth, 100, 5000);
    selectionHeight = std::clamp(selectionHeight, 10, 2000);
    marginTop = std::clamp(marginTop, 20, 2000);
    marginBottom = std::clamp(marginBottom, 120, 2000);
    marginLeft = std::clamp(marginLeft, 10, 2000);
    marginRight = std::clamp(marginRight, 10, 2000);
    opacity = std::clamp(opacity, 0.15, 1.0);
    borderWidth = std::clamp(borderWidth, 1, 8);
    sticky = false;
}

SettingsStore::SettingsStore()
    : directory_(appDataDirectory())
    , filePath_(directory_ + L"\\settings.json")
{
}

Settings SettingsStore::load() const
{
    Settings settings;
    const std::string json = readFileUtf8(filePath_);
    if (json.empty()) {
        settings.clamp();
        Log::write(L"Settings load: no settings file found");
        return settings;
    }

    settings.loadedFromDisk = true;
    readInt(json, "x", settings.x);
    readInt(json, "y", settings.y);
    readInt(json, "selectionWidth", settings.selectionWidth);
    readInt(json, "selectionHeight", settings.selectionHeight);
    readInt(json, "marginTop", settings.marginTop);
    readInt(json, "marginBottom", settings.marginBottom);
    readInt(json, "marginLeft", settings.marginLeft);
    readInt(json, "marginRight", settings.marginRight);
    readDouble(json, "opacity", settings.opacity);
    readInt(json, "borderWidth", settings.borderWidth);
    readBool(json, "visible", settings.visible);
    readBool(json, "sticky", settings.sticky);
    readBool(json, "selectionConfigured", settings.selectionConfigured);
    settings.clamp();
    std::wostringstream line;
    line << L"Settings load: x=" << settings.x
         << L" y=" << settings.y
         << L" selection=" << settings.selectionWidth << L"x" << settings.selectionHeight
         << L" margins=" << settings.marginTop << L"," << settings.marginRight << L"," << settings.marginBottom << L"," << settings.marginLeft
         << L" opacity=" << settings.opacity
         << L" visible=" << settings.visible
         << L" selectionConfigured=" << settings.selectionConfigured;
    Log::write(line.str());
    return settings;
}

bool SettingsStore::save(const Settings& settings) const
{
    CreateDirectoryW(directory_.c_str(), nullptr);

    std::ostringstream json;
    json << "{\n";
    json << "  \"x\": " << settings.x << ",\n";
    json << "  \"y\": " << settings.y << ",\n";
    json << "  \"selectionWidth\": " << settings.selectionWidth << ",\n";
    json << "  \"selectionHeight\": " << settings.selectionHeight << ",\n";
    json << "  \"marginTop\": " << settings.marginTop << ",\n";
    json << "  \"marginBottom\": " << settings.marginBottom << ",\n";
    json << "  \"marginLeft\": " << settings.marginLeft << ",\n";
    json << "  \"marginRight\": " << settings.marginRight << ",\n";
    json << "  \"opacity\": " << settings.opacity << ",\n";
    json << "  \"borderWidth\": " << settings.borderWidth << ",\n";
    json << "  \"visible\": " << (settings.visible ? "true" : "false") << ",\n";
    json << "  \"sticky\": false,\n";
    json << "  \"selectionConfigured\": " << (settings.selectionConfigured ? "true" : "false") << "\n";
    json << "}\n";
    const bool ok = writeFileUtf8(filePath_, json.str());
    std::wostringstream line;
    line << L"Settings save: " << (ok ? L"ok" : L"failed")
         << L" visible=" << settings.visible
         << L" selectionConfigured=" << settings.selectionConfigured
         << L" path=" << filePath_;
    Log::write(line.str());
    return ok;
}

void SettingsStore::reset(Settings& settings) const
{
    const bool wasLoaded = settings.loadedFromDisk;
    settings = Settings{};
    settings.loadedFromDisk = wasLoaded;
    settings.clamp();
}
