#pragma once

#include "settings_schema.h"

#include <map>
#include <string>
#include <vector>

struct SettingOverrides {
    std::map<std::string, std::string> values;
    std::map<std::string, int> lineNumbers;
    bool valid = true;
    int firstLineNumber = 0;
};

struct SettingValidationIssue {
    std::string settingName;
    std::string message;
};

const Settings::Definition* FindSettingDefinition(const std::string& settingName);
bool SettingAllowsScope(const Settings::Definition& definition, const std::string& scope);

class SettingsValues
{
private:
    std::map<std::string, std::string> values_;

public:
    SettingsValues();

    bool TryApply(const SettingOverrides& overrides, const std::string& scope, SettingsValues& result, std::vector<SettingValidationIssue>& issues) const;
    const std::map<std::string, std::string>& GetValues() const;
    bool GetBoolean(const std::string& settingName) const;
    const std::string& GetString(const std::string& settingName) const;
    int GetInteger(const std::string& settingName) const;
};
