#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>

struct SettingsConfigEditRequest {
    std::string scope;
    std::string pageName;
    std::string surfaceName;
    std::map<std::string, std::optional<std::string>> changes;
};

bool EditSettingsConfigSource(const std::string& source, const SettingsConfigEditRequest& request, std::string& result, std::string& errorMessage);
bool WriteSettingsConfigAtomically(const std::filesystem::path& configPath, const std::string& source, std::string& errorMessage);
