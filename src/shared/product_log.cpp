#include "product_log.h"

#include "product_identity.h"
#include "product_paths.h"
#include "reaper_plugin_functions.h"

#include <cstdint>
#include <ctime>
#include <fstream>
#include <mutex>
#include <string>

extern HWND g_hwnd;

class ProductLogState
{
public:
    std::mutex mutex;
    bool initialized = false;
    bool writeFile = true;
    bool showConsole = false;
    std::filesystem::path activeFile;
    std::filesystem::path activeDirectory;
    std::string dailyId;
    std::string activeDate;
    std::uintmax_t startOffset = 0;
    std::time_t nextDateRefresh = 0;
};

static ProductLogState& GetProductLogState() {
    static ProductLogState state;
    return state;
}

static void PublishProductLogState(const ProductLogState& state) {
    ::SetExtState(ProductIdentity::ExtStateLog, "SessionId", state.dailyId.c_str(), false);
    ::SetExtState(ProductIdentity::ExtStateLog, "Directory", state.activeDirectory.string().c_str(), false);
    ::SetExtState(ProductIdentity::ExtStateLog, "File", state.activeFile.string().c_str(), false);
    const std::string startOffset = std::to_string(state.startOffset);
    ::SetExtState(ProductIdentity::ExtStateLog, "StartOffset", startOffset.c_str(), false);
    ::SetExtState(ProductIdentity::ExtStateLog, "WriteFile", state.writeFile ? "1" : "0", false);
    ::SetExtState(ProductIdentity::ExtStateLog, "ShowConsole", state.showConsole ? "1" : "0", false);
}

static std::tm GetProductLogLocalTime(std::time_t rawTime) {
    std::tm localTime {};
#ifdef _WIN32
    localtime_s(&localTime, &rawTime);
#else
    localtime_r(&rawTime, &localTime);
#endif
    return localTime;
}

static std::string FormatProductLogTime(const std::tm& localTime, const char* format) {
    char text[32] {};
    strftime(text, sizeof(text), format, &localTime);
    return text;
}

static std::time_t FindNextProductLogDateRefresh(std::tm localTime) {
    localTime.tm_sec = 0;
    localTime.tm_min = 0;
    localTime.tm_hour = 0;
    localTime.tm_mday += 1;
    localTime.tm_isdst = -1;
    return std::mktime(&localTime);
}

static void ClearProductLogFileState(ProductLogState& state) {
    state.activeFile.clear();
    state.activeDirectory.clear();
    state.dailyId.clear();
    state.activeDate.clear();
    state.startOffset = 0;
}

static void RefreshProductLogFileState(ProductLogState& state, std::time_t rawTime, const std::tm& localTime, bool force) {
    if (!force && rawTime < state.nextDateRefresh && (!state.writeFile || !state.activeFile.empty())) return;
    state.nextDateRefresh = FindNextProductLogDateRefresh(localTime);
    if (!state.writeFile) {
        if (!state.activeFile.empty()) {
            ClearProductLogFileState(state);
            PublishProductLogState(state);
        }
        return;
    }
    const std::string date = FormatProductLogTime(localTime, "%Y-%m-%d");
    if (date == state.activeDate && !state.activeFile.empty()) return;
    try {
        const std::filesystem::path logsRoot = ProductPaths::FromReaperResourcePath().TemporaryLogsRoot();
        const std::string month = FormatProductLogTime(localTime, "%Y-%m");
        const std::filesystem::path activeDirectory = logsRoot / (std::string(ProductIdentity::ExtStatePrefix) + "_logs_" + month);
        const std::filesystem::path activeFile = activeDirectory / (std::string(ProductIdentity::ExtStatePrefix) + "_" + date + ".log");
        std::filesystem::create_directories(activeDirectory);
        std::ofstream createFile(activeFile, std::ios::binary | std::ios::app);
        if (!createFile.is_open()) {
            ClearProductLogFileState(state);
        } else {
            state.dailyId = date;
            state.activeDate = date;
            state.activeDirectory = activeDirectory;
            state.activeFile = activeFile;
            state.startOffset = std::filesystem::file_size(activeFile);
        }
    } catch (...) {
        ClearProductLogFileState(state);
    }
    PublishProductLogState(state);
}

static void InitializeProductLogState(ProductLogState& state) {
    if (state.initialized) return;
    state.initialized = true;
    const std::time_t rawTime = std::time(nullptr);
    const std::tm localTime = GetProductLogLocalTime(rawTime);
    RefreshProductLogFileState(state, rawTime, localTime, true);
    if (!state.writeFile) PublishProductLogState(state);
}

static bool OpenProductLogPath(const std::filesystem::path& target) {
    if (target.empty()) return false;
#ifdef _WIN32
    const HINSTANCE result = ::ShellExecute(g_hwnd, "open", target.string().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<std::intptr_t>(result) > 32;
#else
    return ::ShellExecute(g_hwnd, "open", target.string().c_str(), "", "", SW_SHOWNORMAL) != FALSE;
#endif
}

namespace ProductLog {
void Initialize() {
    ProductLogState& state = GetProductLogState();
    std::lock_guard<std::mutex> lock(state.mutex);
    InitializeProductLogState(state);
}

void Refresh() {
    ProductLogState& state = GetProductLogState();
    std::lock_guard<std::mutex> lock(state.mutex);
    InitializeProductLogState(state);
    const std::time_t rawTime = std::time(nullptr);
    RefreshProductLogFileState(state, rawTime, GetProductLogLocalTime(rawTime), false);
}

void SetOutputs(bool writeFile, bool showConsole) {
    ProductLogState& state = GetProductLogState();
    std::lock_guard<std::mutex> lock(state.mutex);
    InitializeProductLogState(state);
    const bool fileOutputChanged = state.writeFile != writeFile;
    const bool consoleOutputChanged = state.showConsole != showConsole;
    state.writeFile = writeFile;
    state.showConsole = showConsole;
    if (!writeFile) ClearProductLogFileState(state);
    if (writeFile) {
        const std::time_t rawTime = std::time(nullptr);
        RefreshProductLogFileState(state, rawTime, GetProductLogLocalTime(rawTime), fileOutputChanged);
    }
    if (!writeFile || consoleOutputChanged) PublishProductLogState(state);
}

std::filesystem::path ActiveFile() {
    ProductLogState& state = GetProductLogState();
    std::lock_guard<std::mutex> lock(state.mutex);
    InitializeProductLogState(state);
    const std::time_t rawTime = std::time(nullptr);
    RefreshProductLogFileState(state, rawTime, GetProductLogLocalTime(rawTime), false);
    return state.activeFile;
}

std::filesystem::path ActiveDirectory() {
    ProductLogState& state = GetProductLogState();
    std::lock_guard<std::mutex> lock(state.mutex);
    InitializeProductLogState(state);
    const std::time_t rawTime = std::time(nullptr);
    RefreshProductLogFileState(state, rawTime, GetProductLogLocalTime(rawTime), false);
    return state.activeDirectory;
}

void Write(const char* message) {
    ProductLogState& state = GetProductLogState();
    std::lock_guard<std::mutex> lock(state.mutex);
    InitializeProductLogState(state);
    const std::time_t rawTime = std::time(nullptr);
    const std::tm localTime = GetProductLogLocalTime(rawTime);
    RefreshProductLogFileState(state, rawTime, localTime, false);
    const std::string text = message ? message : "";
    std::string record = FormatProductLogTime(localTime, "[%H:%M:%S] ") + text;
    if (record.empty() || record.back() != '\n') record += '\n';
    if (state.writeFile && !state.activeFile.empty()) {
        std::ofstream logFile(state.activeFile, std::ios::binary | std::ios::app);
        if (logFile.is_open()) logFile << record;
    }
    if (state.showConsole && ::ShowConsoleMsg) ::ShowConsoleMsg(record.c_str());
}

bool OpenActiveFile() { return OpenProductLogPath(ProductLog::ActiveFile()); }

bool OpenActiveDirectory() { return OpenProductLogPath(ProductLog::ActiveDirectory()); }
}
