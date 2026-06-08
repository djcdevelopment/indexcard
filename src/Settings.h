#pragma once

#include <string>

struct Settings {
    int x = 300;
    int y = 220;
    int selectionWidth = 900;
    int selectionHeight = 40;
    int marginTop = 150;
    int marginBottom = 160;
    int marginLeft = 75;
    int marginRight = 75;
    double opacity = 0.90;
    bool visible = false;
    bool sticky = false;
    bool selectionConfigured = false;

    bool loadedFromDisk = false;

    int outerWidth() const;
    int outerHeight() const;
    void clamp();
};

class SettingsStore {
public:
    SettingsStore();

    Settings load() const;
    bool save(const Settings& settings) const;
    void reset(Settings& settings) const;

    const std::wstring& filePath() const { return filePath_; }

private:
    std::wstring directory_;
    std::wstring filePath_;
};
