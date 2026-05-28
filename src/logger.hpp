#pragma once

#define MOD_NAME "EnchantUnbound"

#ifdef _WIN32
#include <Windows.h>
#include <cstdio>
#include <cstdarg>
#include <mutex>
#include <vector>
#endif

#ifdef __ANDROID__
#include <android/log.h>

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, MOD_NAME, __VA_ARGS__)
#endif

#ifdef _WIN32
#ifdef _DEV
struct LogEntry { char text[1200]; };
inline std::vector<LogEntry> g_DebugBuffer;
#endif

inline std::mutex g_LogMtx;

inline void InitConsole() {
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
}

inline void LogBase(const char* level, bool buffer, const char* fmt, ...) {
    char msg[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    
    SYSTEMTIME st;
    GetLocalTime(&st);
    char finalLine[1200];
    snprintf(finalLine, sizeof(finalLine), "%02d:%02d:%02d.%03d %s [%s] %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, level, MOD_NAME, msg);
    
    std::lock_guard<std::mutex> lock(g_LogMtx);
    #ifdef _DEV
    if (buffer) {
        LogEntry entry;
        strncpy(entry.text, finalLine, sizeof(entry.text));
        g_DebugBuffer.push_back(entry); 
    }
    else {
        printf("%s", finalLine);
        fflush(stdout);
    }
    #else
    if (!buffer)
        printf("%s", finalLine);
    #endif
}

#ifdef _DEV
#define LOG(...)  LogBase("INFO", false, __VA_ARGS__) 
#define LOGD(...) LogBase("INFO", true, __VA_ARGS__) 
#else
#define LOG(...)  LogBase("INFO", false, __VA_ARGS__)
#define LOGD(...)
#endif

#ifdef _DEV
inline void DumpDebugReport() {
    LOG("============ DEBUG REPORT ============");
    std::lock_guard<std::mutex> lock(g_LogMtx);
    for (const auto& line : g_DebugBuffer)
        printf("%s", line.text);
    fflush(stdout);
    g_DebugBuffer.clear();
}
#endif
#endif