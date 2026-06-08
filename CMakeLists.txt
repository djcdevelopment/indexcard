cmake_minimum_required(VERSION 3.20)

project(FocusStrip LANGUAGES CXX)

add_executable(FocusStrip WIN32
    src/Hotkeys.cpp
    src/Hotkeys.h
    src/Logger.cpp
    src/Logger.h
    src/main.cpp
    src/OverlayWindow.cpp
    src/OverlayWindow.h
    src/SelectionCapture.cpp
    src/SelectionCapture.h
    src/Settings.cpp
    src/Settings.h
    src/SplashWindow.cpp
    src/SplashWindow.h
    src/TrayIcon.cpp
    src/TrayIcon.h
)

target_compile_features(FocusStrip PRIVATE cxx_std_17)
target_compile_options(FocusStrip PRIVATE /utf-8)
target_compile_definitions(FocusStrip PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0A00
)

target_link_libraries(FocusStrip PRIVATE
    user32
    gdi32
    shell32
    ole32
)
