#pragma once

#define MOD_NAME "EnchantUnbound"

#ifdef _WIN32
#include <Windows.h>
#include <cstdio>
#include <cstdarg>
#include <mutex>
#include <vector>
#include <cstring>
#elif defined(__ANDROID__)
#include <android/log.h>
#elif defined(__linux__)
#include <cstdio>
#include <cstdarg>
#include <mutex>
#include <cstring>
#include <sys/time.h>
#include <time.h>
#endif

#ifdef __ANDROID__
#define LOG(...) __android_log_print(ANDROID_LOG_INFO, MOD_NAME, __VA_ARGS__)

#elif defined(_WIN32)
#ifdef _DEV
struct LogEntry { char text[1200]; };
inline std::vector<LogEntry> g_DebugBuffer;
#endif

inline std::mutex g_LogMtx;

inline void InitConsole() {
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
}

inline bool IsServerMod() {
    wchar_t path[MAX_PATH];
    if (GetModuleFileNameW(nullptr, path, MAX_PATH) == 0) return false;
    
    wchar_t* filename = wcsrchr(path, L'\\');
    filename = (filename != nullptr) ? filename + 1 : path;
    
    return (_wcsicmp(filename, L"bedrock_server_mod.exe") == 0);
}

inline void LogBase(const char* level, bool buffer, const char* fmt, ...) {
    char msg[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    
    char finalLine[1200];

    SYSTEMTIME st;
    GetLocalTime(&st);

    if (IsServerMod()) {
        snprintf(finalLine, sizeof(finalLine),
                 "%02d:%02d:%02d.%03d %s [%s] %s\n",
                 st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                 level, MOD_NAME, msg);
    } else {
        snprintf(finalLine, sizeof(finalLine),
                 "[%04d-%02d-%02d %02d:%02d:%02d:%03d %s] [%s] %s\n",
                 st.wYear, st.wMonth, st.wDay,
                 st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                 level, MOD_NAME, msg);
    }
    
    std::lock_guard<std::mutex> lock(g_LogMtx);

#ifdef _DEV
    if (buffer) {
        LogEntry entry;
        strncpy(entry.text, finalLine, sizeof(entry.text));
        g_DebugBuffer.push_back(entry);
    } else {
        printf("%s", finalLine);
        fflush(stdout);
    }
#else
    if (!buffer)
        printf("%s", finalLine);
#endif
}

#define LOG(...) LogBase("INFO", false, __VA_ARGS__)

#ifdef _DEV
#define LOGD(...) LogBase("INFO", true, __VA_ARGS__)

inline void DumpDebugReport() {
    LOG("============ DEBUG REPORT ============");

    std::lock_guard<std::mutex> lock(g_LogMtx);
    for (const auto& line : g_DebugBuffer)
        printf("%s", line.text);

    fflush(stdout);
    g_DebugBuffer.clear();
}
#else
#define LOGD(...)
#endif

#elif defined(__linux__)
inline std::mutex g_LogMtx;

inline void LogBase(const char* level, bool buffer, const char* fmt, ...) {
    char msg[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    
    char finalLine[1200];

    struct timeval tv;
    gettimeofday(&tv, NULL);

    struct tm* localTime = localtime(&tv.tv_sec);

    snprintf(finalLine, sizeof(finalLine),
            "[%04d-%02d-%02d %02d:%02d:%02d:%03d %s] [%s] %s\n",
            localTime->tm_year + 1900, localTime->tm_mon + 1, localTime->tm_mday,
            localTime->tm_hour, localTime->tm_min, localTime->tm_sec, (int)(tv.tv_usec / 1000),
            level, MOD_NAME, msg);
    
    std::lock_guard<std::mutex> lock(g_LogMtx);

    if (!buffer) {
        printf("%s", finalLine);
        fflush(stdout);
    }
}

#define LOG(...) LogBase("INFO", false, __VA_ARGS__)
#endif
