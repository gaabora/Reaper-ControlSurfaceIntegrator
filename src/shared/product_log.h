#pragma once

#include <filesystem>

namespace ProductLog {
void Initialize();
void Refresh();
void SetOutputs(bool writeFile, bool showConsole);
std::filesystem::path ActiveFile();
std::filesystem::path ActiveDirectory();
void Write(const char* message);
bool OpenActiveFile();
bool OpenActiveDirectory();
}
