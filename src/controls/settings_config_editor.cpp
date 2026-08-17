#include "settings_config_editor.h"

#include "preamble.h"

#include <cctype>
#include <chrono>

struct SettingsConfigSourceLine {
    string text;
    string ending;
};

struct SettingsConfigTokenSpan {
    size_t start = 0;
    size_t end = 0;
    string text;
};

static vector<SettingsConfigSourceLine> SplitSettingsConfigSource(const string& source) {
    vector<SettingsConfigSourceLine> lines;
    size_t start = 0;
    while (start < source.size()) {
        const size_t lineFeed = source.find('\n', start);
        if (lineFeed == string::npos) {
            lines.push_back({ source.substr(start), "" });
            return lines;
        }
        const bool hasCarriageReturn = lineFeed > start && source[lineFeed - 1] == '\r';
        const size_t textEnd = hasCarriageReturn ? lineFeed - 1 : lineFeed;
        lines.push_back({ source.substr(start, textEnd - start), hasCarriageReturn ? "\r\n" : "\n" });
        start = lineFeed + 1;
    }
    if (source.empty()) lines.push_back({ "", "" });
    return lines;
}

static string JoinSettingsConfigSource(const vector<SettingsConfigSourceLine>& lines) {
    string result;
    for (const SettingsConfigSourceLine& line : lines) result += line.text + line.ending;
    return result;
}

static vector<SettingsConfigTokenSpan> ScanSettingsConfigTokens(const string& line) {
    vector<SettingsConfigTokenSpan> tokens;
    size_t position = 0;
    while (position < line.size()) {
        while (position < line.size() && std::isspace(static_cast<unsigned char>(line[position]))) position++;
        if (position >= line.size()) break;
        const size_t start = position;
        bool insideQuote = false;
        while (position < line.size()) {
            if (line[position] == '"') insideQuote = !insideQuote;
            else if (!insideQuote && std::isspace(static_cast<unsigned char>(line[position]))) break;
            position++;
        }
        tokens.push_back({ start, position, line.substr(start, position - start) });
    }
    return tokens;
}

static string SettingsConfigPropertyName(const string& token) {
    const size_t separator = token.find('=');
    return separator == string::npos ? "" : token.substr(0, separator);
}

static string SettingsConfigPropertyValue(const string& token) {
    const size_t separator = token.find('=');
    if (separator == string::npos) return "";
    string value = token.substr(separator + 1);
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') value = value.substr(1, value.size() - 2);
    return value;
}

static string FindSettingsConfigPropertyValue(const vector<SettingsConfigTokenSpan>& tokens, const string& propertyName) {
    for (const SettingsConfigTokenSpan& token : tokens) if (SettingsConfigPropertyName(token.text) == propertyName) return SettingsConfigPropertyValue(token.text);
    return "";
}

static bool ReplaceSettingsConfigToken(string& line, const string& settingName, const std::optional<string>& value, bool& found, string& errorMessage) {
    found = false;
    const vector<SettingsConfigTokenSpan> tokens = ScanSettingsConfigTokens(line);
    for (const SettingsConfigTokenSpan& token : tokens) {
        if (SettingsConfigPropertyName(token.text) != settingName) continue;
        if (found) {
            errorMessage = "Setting " + settingName + " is duplicated on one configuration line";
            return false;
        }
        found = true;
        if (value) {
            line.replace(token.start, token.end - token.start, settingName + "=" + *value);
        } else {
            size_t removeStart = token.start;
            while (removeStart > 0 && std::isspace(static_cast<unsigned char>(line[removeStart - 1]))) removeStart--;
            line.erase(removeStart, token.end - removeStart);
        }
    }
    return true;
}

static bool IsSettingsDirectiveLine(const string& line) {
    const vector<SettingsConfigTokenSpan> tokens = ScanSettingsConfigTokens(line);
    return !tokens.empty() && tokens[0].text == "Settings";
}

static bool EditProductSettings(vector<SettingsConfigSourceLine>& lines, const SettingsConfigEditRequest& request, string& errorMessage) {
    vector<size_t> settingsLineIndices;
    for (size_t lineIdx = 0; lineIdx < lines.size(); lineIdx++) if (IsSettingsDirectiveLine(lines[lineIdx].text)) settingsLineIndices.push_back(lineIdx);

    for (const auto& change : request.changes) {
        bool found = false;
        for (size_t lineIdx : settingsLineIndices) {
            bool foundOnLine = false;
            if (!ReplaceSettingsConfigToken(lines[lineIdx].text, change.first, change.second, foundOnLine, errorMessage)) return false;
            if (found && foundOnLine) {
                errorMessage = "Setting " + change.first + " is duplicated in Product scope";
                return false;
            }
            found = found || foundOnLine;
        }
        if (found || !change.second) continue;
        if (settingsLineIndices.empty()) {
            const string lineEnding = !lines.empty() && !lines[0].ending.empty() ? lines[0].ending : "\n";
            if (!lines.empty() && lines[0].ending.empty()) lines[0].ending = lineEnding;
            const size_t insertIndex = lines.empty() ? 0 : 1;
            lines.insert(lines.begin() + insertIndex, { "Settings " + change.first + "=" + *change.second, lineEnding });
            settingsLineIndices.push_back(insertIndex);
        } else {
            lines[settingsLineIndices[0]].text += " " + change.first + "=" + *change.second;
        }
    }

    for (size_t reverseIdx = settingsLineIndices.size(); reverseIdx > 0; reverseIdx--) {
        const size_t lineIdx = settingsLineIndices[reverseIdx - 1];
        if (ScanSettingsConfigTokens(lines[lineIdx].text).size() == 1) lines.erase(lines.begin() + lineIdx);
    }
    return true;
}

static bool EditSurfaceSettings(vector<SettingsConfigSourceLine>& lines, const SettingsConfigEditRequest& request, string& errorMessage) {
    string currentPageName;
    size_t targetLineIndex = lines.size();
    for (size_t lineIdx = 0; lineIdx < lines.size(); lineIdx++) {
        const vector<SettingsConfigTokenSpan> tokens = ScanSettingsConfigTokens(lines[lineIdx].text);
        const string pageName = FindSettingsConfigPropertyValue(tokens, "PageName");
        if (!pageName.empty()) currentPageName = pageName;
        const string surfaceName = FindSettingsConfigPropertyValue(tokens, "Surface");
        if (currentPageName != request.pageName || surfaceName != request.surfaceName) continue;
        if (targetLineIndex != lines.size()) {
            errorMessage = "More than one Surface=" + request.surfaceName + " assignment exists on Page=" + request.pageName;
            return false;
        }
        targetLineIndex = lineIdx;
    }
    if (targetLineIndex == lines.size()) {
        errorMessage = "Cannot find Surface=" + request.surfaceName + " on Page=" + request.pageName;
        return false;
    }

    for (const auto& change : request.changes) {
        bool found = false;
        if (!ReplaceSettingsConfigToken(lines[targetLineIndex].text, change.first, change.second, found, errorMessage)) return false;
        if (!found && change.second) lines[targetLineIndex].text += " " + change.first + "=" + *change.second;
    }
    return true;
}

bool EditSettingsConfigSource(const string& source, const SettingsConfigEditRequest& request, string& result, string& errorMessage) {
    if (request.scope != "Product" && request.scope != "Surface") {
        errorMessage = "Settings scope must be Product or Surface";
        return false;
    }
    if (request.changes.empty()) {
        errorMessage = "Settings edit has no changes";
        return false;
    }

    vector<SettingsConfigSourceLine> lines = SplitSettingsConfigSource(source);
    const bool edited = request.scope == "Product" ? EditProductSettings(lines, request, errorMessage) : EditSurfaceSettings(lines, request, errorMessage);
    if (!edited) return false;
    result = JoinSettingsConfigSource(lines);
    return true;
}

bool WriteSettingsConfigAtomically(const filesystem::path& configPath, const string& source, string& errorMessage) {
    const long long timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    filesystem::path temporaryPath = configPath;
    temporaryPath += ".settings-" + std::to_string(timestamp) + ".tmp";
    {
        ofstream temporaryFile(temporaryPath, std::ios::binary | std::ios::trunc);
        if (!temporaryFile.is_open()) {
            errorMessage = "Cannot create temporary settings file: " + temporaryPath.string();
            return false;
        }
        temporaryFile.write(source.data(), static_cast<std::streamsize>(source.size()));
        temporaryFile.flush();
        if (!temporaryFile.good()) {
            errorMessage = "Cannot write temporary settings file: " + temporaryPath.string();
            temporaryFile.close();
            std::error_code removeError;
            filesystem::remove(temporaryPath, removeError);
            return false;
        }
    }

#ifdef _WIN32
    if (!MoveFileExW(temporaryPath.wstring().c_str(), configPath.wstring().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        errorMessage = "Cannot replace settings configuration file; Windows error " + std::to_string(GetLastError());
        std::error_code removeError;
        filesystem::remove(temporaryPath, removeError);
        return false;
    }
#else
    std::error_code renameError;
    filesystem::rename(temporaryPath, configPath, renameError);
    if (renameError) {
        errorMessage = "Cannot replace settings configuration file: " + renameError.message();
        std::error_code removeError;
        filesystem::remove(temporaryPath, removeError);
        return false;
    }
#endif
    return true;
}
