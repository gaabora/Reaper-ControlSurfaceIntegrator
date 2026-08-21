#pragma once

#include <filesystem>

namespace ProductLog {
void Initialize();
std::filesystem::path ActiveFile();
std::filesystem::path SessionDirectory();
void Write(const char* message);
bool OpenActiveFile();
bool OpenSessionDirectory();
}
