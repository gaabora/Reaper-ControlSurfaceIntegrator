//  utils.h
//  Shared utility functions: numeric conversions, logging, string helpers, rgbToColor()

#ifndef csi_utils_h
#define csi_utils_h

#include "../../lib/WDL/WDL/db2val.h"
#include "../../lib/WDL/WDL/wdlcstring.h"

#ifdef _DEBUG
  #if defined(__cpp_lib_stacktrace)
    #include <stacktrace>
  #endif
#endif

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <ctime>

inline double int14ToNormalized(unsigned char msb, unsigned char lsb) {
    int val = lsb | (msb << 7);
    double normalizedVal = val / 16383.0;
    normalizedVal = normalizedVal < 0.0 ? 0.0 : normalizedVal;
    normalizedVal = normalizedVal > 1.0 ? 1.0 : normalizedVal;
    return normalizedVal;
}

inline double normalizedToVol(double val) {
    double pos = val * 1000.0;
    pos = SLIDER2DB(pos);
    return DB2VAL(pos);
}

inline double volToNormalized(double vol) {
    double d = (DB2SLIDER(VAL2DB(vol)) / 1000.0);
    if (d < 0.0) d = 0.0;
    else if (d > 1.0) d = 1.0;
    return d;
}

inline double normalizedToPan(double val) {
    return 2.0 * val - 1.0;
}

inline double panToNormalized(double val) {
    return 0.5 * (val + 1.0);
}

enum MultiState {
    Undefined = -1,
    False = 0,
    True = 1,
    Mixed = 2,
};

enum DebugLevel {
    DEBUG_LEVEL_ERROR = 0,
    DEBUG_LEVEL_WARNING = 1,
    DEBUG_LEVEL_NOTICE = 2,
    DEBUG_LEVEL_INFO = 3,
    DEBUG_LEVEL_DEBUG = 4
};

inline const char* DebugLevelToString(int level) {
    switch (level) {
        case DEBUG_LEVEL_ERROR: return "ERROR";
        case DEBUG_LEVEL_WARNING: return "WARNING";
        case DEBUG_LEVEL_NOTICE: return "NOTICE";
        case DEBUG_LEVEL_INFO: return "INFO";
        case DEBUG_LEVEL_DEBUG: return "DEBUG";
        default: return "UNKNOWN";
    }
}

inline void LogMessage(const char* msg) {
    std::ofstream logFile(std::string(GetResourcePath()) + "/CSI/CSI.log", std::ios::app);
    if (logFile.is_open()) {
        char timeStr[32];
        time_t rawtime;
        time(&rawtime);
        struct tm timeinfo_buf;
#ifdef _WIN32
        localtime_s(&timeinfo_buf, &rawtime);
#else
        localtime_r(&rawtime, &timeinfo_buf);
#endif
        strftime(timeStr, sizeof(timeStr), "[%y-%m-%d %H:%M:%S] ", &timeinfo_buf);
        logFile << timeStr << msg;
    }
}

template <size_t N, typename... Args>
inline void LogToConsole(const char (&format)[N], Args&&... args) {
    std::vector<char> buffer(2048);
    snprintf(buffer.data(), buffer.size(), format, std::forward<Args>(args)...);
    ShowConsoleMsg(buffer.data());
    LogMessage(buffer.data());
}

inline void LogToConsole(const char* message) {
    ShowConsoleMsg(message);
    LogMessage(message);
}

inline void LogStackTraceToConsole() {
#ifdef _DEBUG
  #if defined(__cpp_lib_stacktrace)
    auto trace = stacktrace::current();
    LogToConsole("===== Stack Trace Start =====\n");
    for (const auto& frame : trace) {
        stringstream ss;
        ss << frame;
        string line = ss.str();
        if (line.find('\\') != string::npos || line.find('/') != string::npos)
        {
            size_t pos = 0;
            while ((pos = line.find("reaper_csurf_integrator!", pos)) != string::npos)
                line.replace(pos, 24, "");

            pos = line.find("+0x");
            if (pos != string::npos)
                line = line.substr(0, pos);

            LogToConsole("%s\n", line.c_str());
        }
    }
    LogToConsole("===== Stack Trace End =====\n");
  #else
    LogToConsole("LogStackTraceToConsole not supported on this compiler.\n");
  #endif
#endif
}

// -------------------------------------------------------------------------
// String helpers
// -------------------------------------------------------------------------
inline bool IsSameString(const char* a, const char* b) {
    if (a == nullptr || b == nullptr) return false;
    return strcmp(a, b) == 0;
}
inline bool IsSameString(const std::string& a, const std::string& b) { return a == b; }
inline bool IsSameString(const std::string& a, const char* b) { return IsSameString(a.c_str(), b); }
inline bool IsSameString(const char* a, const std::string& b) { return IsSameString(a, b.c_str()); }

inline std::string GetRelativePath(const char* absolutePath) {
    const char* resourcePath = GetResourcePath();
    size_t resourcePathLen = strlen(resourcePath);

    if (strncmp(absolutePath, resourcePath, resourcePathLen) == 0) {
        const char* rel = absolutePath + resourcePathLen;
        if (*rel == '/' || *rel == '\\')
            ++rel;

        std::string relativePath;
        for (const char* ptr = rel; *ptr != '\0'; ++ptr)
            relativePath.push_back(*ptr == '\\' ? '/' : *ptr);
        return relativePath;
    }
    return std::string(absolutePath);
}

inline bool IsSameRelativePath(const char* a, const char* b) {
    return IsSameString(GetRelativePath(a), GetRelativePath(b));
}

inline int ExtractSuffixNumber(const std::string& name) {
    if (name.empty()) return -1;
    int result = -1;
    int index = static_cast<int>(name.length()) - 1;
    while (index >= 0 && isdigit(static_cast<unsigned char>(name[index])))
        index--;
    if (index < static_cast<int>(name.length()) - 1)
        result = stoi(name.substr(index + 1));
    return result;
}

inline std::string JoinStringVector(const std::vector<std::string>& strings, const std::string& delimiter) {
    std::ostringstream oss;
    for (size_t i = 0; i < strings.size(); ++i) {
        oss << strings[i];
        if (i < strings.size() - 1)
            oss << delimiter;
    }
    return oss.str();
}

template <size_t N>
inline int CycleNextValue(const int (&arr)[N], int currentValue) {
    for (size_t i = 0; i < N; ++i) {
        if (arr[i] == currentValue)
            return arr[(i + 1) % N];
    }
    return arr[0];
}

inline char* format_number(double v, char* buf, int bufsz) {
    snprintf(buf, bufsz, "%.12f", v);
    WDL_remove_trailing_decimal_zeros(buf, 2);
    return buf;
}

inline bool IsCommentedOrEmpty(const string& line) {
    return line.empty() || line[0] == '\r' || line[0] == '/' || line[0] == '#';
}

inline void TrimLine(string& line) {
    const string tmp = line;
    const char* p = tmp.c_str();

    line.clear();
    for (;;) {
        while (*p > 0 && isspace(static_cast<unsigned char>(*p))) p++;
        if (!*p || p[0] == '/') break;
        if (line.length()) line.append(" ", 1);
        while (*p && !isspace(static_cast<unsigned char>(*p))) {
            if (p[0] == '/' && p[1] == '/') break;
            line.append(p++, 1);
        }
    }
}

#include "types.h"

inline bool GetColorValue(const char* hexColor, rgba_color& colorValue) {
    if (strlen(hexColor) == 7)
        return sscanf(hexColor, "#%2x%2x%2x", &colorValue.r, &colorValue.g, &colorValue.b) == 3;
    if (strlen(hexColor) == 9)
        return sscanf(hexColor, "#%2x%2x%2x%2x", &colorValue.r, &colorValue.g, &colorValue.b, &colorValue.a) == 4;
    return false;
}

enum XTouchColorIndex {
    XTCOLOR_OFF     = 0,
    XTCOLOR_RED     = 1,
    XTCOLOR_GREEN   = 2,
    XTCOLOR_YELLOW  = 3,
    XTCOLOR_BLUE    = 4,
    XTCOLOR_MAGENTA = 5,
    XTCOLOR_CYAN    = 6,
    XTCOLOR_WHITE   = 7
};

inline int rgbToColor(int r, int g, int b) {
    // RGB -> HSV conversion (HSV is better for light matching)
    float rf = (float) r / 255.0f;
    float gf = (float) g / 255.0f;
    float bf = (float) b / 255.0f;

    float h, s, v, colorMin, delta;
    v = (float) std::max(std::max(rf, gf), bf);

    if (v <= 0.10f)
        return XTCOLOR_WHITE;

    colorMin = (float) std::min(std::min(rf, gf), bf);
    delta = v - colorMin;
    s = delta / v;

    if (s <= 0.10f)
        return XTCOLOR_WHITE;

    if (rf >= v)        h =  (gf - bf) / delta;
    else if (gf >= v)   h = ((bf - rf) / delta) + 2.0f;
    else                h = ((rf - gf) / delta) + 4.0f;

    h *= 60.0f;
    if (h < 0) h += 360.0f;

    if (h >= 330 || h < 20)  return XTCOLOR_RED;
    if (h >= 250)            return XTCOLOR_MAGENTA;
    if (h >= 210)            return XTCOLOR_BLUE;
    if (h >= 160)            return XTCOLOR_CYAN;
    if (h >= 80)             return XTCOLOR_GREEN;
    if (h >= 20)             return XTCOLOR_YELLOW;
    return XTCOLOR_WHITE;
}

#endif /* csi_utils_h */
