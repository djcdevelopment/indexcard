cmake_minimum_required(VERSION 3.20)

project(IndexCard LANGUAGES CXX)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(IndexCard WIN32
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
    src/Theme.h
    src/TrayIcon.cpp
    src/TrayIcon.h
)

target_compile_features(IndexCard PRIVATE cxx_std_17)
target_compile_options(IndexCard PRIVATE /utf-8)
target_compile_definitions(IndexCard PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0A00
)

target_link_libraries(IndexCard PRIVATE
    user32
    gdi32
    shell32
    ole32
)
