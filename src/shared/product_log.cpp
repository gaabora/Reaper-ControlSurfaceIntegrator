#include "product_log.h"

#include "product_identity.h"
#include "product_paths.h"
#include "reaper_plugin_functions.h"

#include <chrono>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <mutex>
#include <system_error>

extern HWND g_hwnd;

class ProductLogState
{
public:
    std::mutex mutex;
    bool initialized = false;
    std::filesystem::path activeFile;
    std::filesystem::path sessionDirectory;
    std::string sessionId;
};

static ProductLogState& GetProductLogState() {
    static ProductLogState state;
    return state;
}

static void PublishProductLogState(const ProductLogState& state) {
    ::SetExtState(ProductIdentity::ExtStateLog, "SessionId", state.sessionId.c_str(), false);
    ::SetExtState(ProductIdentity::ExtStateLog, "Directory", state.sessionDirectory.string().c_str(), false);
    ::SetExtState(ProductIdentity::ExtStateLog, "File", state.activeFile.string().c_str(), false);
}

static void InitializeProductLogState(ProductLogState& state) {
    if (state.initialized) return;
    state.initialized = true;
    try {
        const std::filesystem::path logsRoot = ProductPaths::FromReaperResourcePath().TemporaryLogsRoot();
        std::filesystem::create_directories(logsRoot);
        const long long timestamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        for (int attempt = 0; attempt < 1000; attempt++) {
            const std::string candidateId = "session-" + std::to_string(timestamp) + "-" + std::to_string(attempt);
            const std::filesystem::path candidateDirectory = logsRoot / candidateId;
            std::error_code createError;
            if (!std::filesystem::create_directory(candidateDirectory, createError)) {
                if (createError && createError != std::errc::file_exists) break;
                continue;
            }
            state.sessionId = candidateId;
            state.sessionDirectory = candidateDirectory;
            state.activeFile = candidateDirectory / ProductIdentity::LogFilename;
            std::ofstream createFile(state.activeFile, std::ios::binary | std::ios::app);
            if (!createFile.is_open()) {
                state.activeFile.clear();
                state.sessionDirectory.clear();
                state.sessionId.clear();
            }
            break;
        }
    } catch (...) {
        state.activeFile.clear();
        state.sessionDirectory.clear();
        state.sessionId.clear();
    }
    PublishProductLogState(state);
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

std::filesystem::path ActiveFile() {
    ProductLogState& state = GetProductLogState();
    std::lock_guard<std::mutex> lock(state.mutex);
    InitializeProductLogState(state);
    return state.activeFile;
}

std::filesystem::path SessionDirectory() {
    ProductLogState& state = GetProductLogState();
    std::lock_guard<std::mutex> lock(state.mutex);
    InitializeProductLogState(state);
    return state.sessionDirectory;
}

void Write(const char* message) {
    ProductLogState& state = GetProductLogState();
    std::lock_guard<std::mutex> lock(state.mutex);
    InitializeProductLogState(state);
    if (state.activeFile.empty()) return;
    std::ofstream logFile(state.activeFile, std::ios::binary | std::ios::app);
    if (!logFile.is_open()) return;
    char timeText[32];
    const std::time_t rawTime = std::time(nullptr);
    std::tm localTime;
#ifdef _WIN32
    localtime_s(&localTime, &rawTime);
#else
    localtime_r(&rawTime, &localTime);
#endif
    strftime(timeText, sizeof(timeText), "[%H:%M:%S] ", &localTime);
    const std::string text = message ? message : "";
    logFile << timeText << text;
    if (text.empty() || text.back() != '\n') logFile << '\n';
}

bool OpenActiveFile() { return OpenProductLogPath(ProductLog::ActiveFile()); }

bool OpenSessionDirectory() { return OpenProductLogPath(ProductLog::SessionDirectory()); }
}
