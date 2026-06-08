#pragma once

#include <string>

namespace Log {

void init();
void write(const std::wstring& message);
void write(const wchar_t* message);
std::wstring path();

}
