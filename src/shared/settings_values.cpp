#include "settings_values.h"

#include <charconv>
#include <system_error>
#include <utility>

static bool ParseSettingInteger(const std::string& source, int& result) {
    if (source.empty()) return false;
    const char* begin = source.data();
    const char* end = begin + source.size();
    const std::from_chars_result parseResult = std::from_chars(begin, end, result);
    return parseResult.ec == std::errc() && parseResult.ptr == end;
}

static bool IsEnumValueAllowed(const Settings::Definition& definition, const std::string& value) {
    const std::string values = definition.enumValues;
    size_t start = 0;
    while (start <= values.size()) {
        const size_t separator = values.find(',', start);
        const size_t length = separator == std::string::npos ? values.size() - start : separator - start;
        if (values.compare(start, length, value) == 0 && length == value.size()) return true;
        if (separator == std::string::npos) break;
        start = separator + 1;
    }
    return false;
}

const Settings::Definition* FindSettingDefinition(const std::string& settingName) {
    for (const Settings::Definition& definition : Settings::Definitions) if (settingName == definition.name) return &definition;
    return nullptr;
}

bool SettingAllowsScope(const Settings::Definition& definition, const std::string& scope) {
    const std::string scopes = definition.scopes;
    size_t start = 0;
    while (start <= scopes.size()) {
        const size_t separator = scopes.find(',', start);
        const size_t length = separator == std::string::npos ? scopes.size() - start : separator - start;
        if (scopes.compare(start, length, scope) == 0 && length == scope.size()) return true;
        if (separator == std::string::npos) break;
        start = separator + 1;
    }
    return false;
}

SettingsValues::SettingsValues() {
    for (const Settings::Definition& definition : Settings::Definitions) this->values_[definition.name] = definition.defaultValue;
}

bool SettingsValues::TryApply(const SettingOverrides& overrides, const std::string& scope, SettingsValues& result, std::vector<SettingValidationIssue>& issues) const {
    issues.clear();
    SettingsValues candidate = *this;
    for (const auto& entry : overrides.values) {
        const Settings::Definition* definition = FindSettingDefinition(entry.first);
        if (!definition) {
            issues.push_back({ entry.first, "Unknown setting: " + entry.first });
            continue;
        }
        if (!SettingAllowsScope(*definition, scope)) {
            issues.push_back({ entry.first, "Setting " + entry.first + " is not allowed in " + scope + " scope" });
            continue;
        }
        if (definition->type == Settings::ValueType::Boolean) {
            if (entry.second != "0" && entry.second != "1") {
                issues.push_back({ entry.first, "Setting " + entry.first + " must be 0 or 1" });
                continue;
            }
        } else if (definition->type == Settings::ValueType::Enum) {
            if (!IsEnumValueAllowed(*definition, entry.second)) {
                issues.push_back({ entry.first, "Setting " + entry.first + " must be one of " + definition->enumValues });
                continue;
            }
        } else {
            int integerValue = 0;
            if (!ParseSettingInteger(entry.second, integerValue)) {
                issues.push_back({ entry.first, "Setting " + entry.first + " must be a complete integer" });
                continue;
            }
            if (integerValue < definition->minValue || integerValue > definition->maxValue) {
                issues.push_back({ entry.first, "Setting " + entry.first + " must be from " + std::to_string(definition->minValue) + " to " + std::to_string(definition->maxValue) });
                continue;
            }
        }
        candidate.values_[entry.first] = entry.second;
    }

    if (issues.empty()) {
        for (const Settings::Definition& definition : Settings::Definitions) {
            if (!definition.greaterThan || definition.greaterThan[0] == '\0') continue;
            const auto valueEntry = candidate.values_.find(definition.name);
            const auto referencedEntry = candidate.values_.find(definition.greaterThan);
            int value = 0;
            int referencedValue = 0;
            if (valueEntry == candidate.values_.end() || referencedEntry == candidate.values_.end() || !ParseSettingInteger(valueEntry->second, value) || !ParseSettingInteger(referencedEntry->second, referencedValue)) {
                issues.push_back({ definition.name, "Setting " + std::string(definition.name) + " has an invalid GreaterThan relationship" });
                continue;
            }
            if (value <= referencedValue) issues.push_back({ definition.name, "Setting " + std::string(definition.name) + " must be greater than " + definition.greaterThan });
        }
    }

    if (!overrides.valid || !issues.empty()) return false;
    result = std::move(candidate);
    return true;
}

const std::map<std::string, std::string>& SettingsValues::GetValues() const { return this->values_; }

bool SettingsValues::GetBoolean(const std::string& settingName) const { return this->GetString(settingName) == "1"; }

const std::string& SettingsValues::GetString(const std::string& settingName) const {
    static const std::string emptyValue;
    const auto entry = this->values_.find(settingName);
    return entry == this->values_.end() ? emptyValue : entry->second;
}

int SettingsValues::GetInteger(const std::string& settingName) const {
    int value = 0;
    ParseSettingInteger(this->GetString(settingName), value);
    return value;
}
