#pragma once

#include <cstdint>
#include <cstring>

#include "logger.hpp"

#ifdef _WIN32
#include <Windows.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "idisasm.hpp"
#endif

#ifdef __ANDROID__
#include <stdio.h>
#include <stdlib.h>

#include <dlfcn.h>
#include <fcntl.h>
#include <link.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef _WIN32
struct SectionInfo { uintptr_t start; size_t size; };

struct PatchStats {
    void* original = nullptr;
    void* replacement = nullptr;
    int replaced{};
    const char* name = nullptr;
};

inline PatchStats gTargets[4];

inline uintptr_t g_base = 0;
inline uintptr_t g_moduleEnd = 0;

inline PIMAGE_NT_HEADERS g_nt = nullptr;

inline SectionInfo g_text{ 0, 0 };
inline SectionInfo g_data{ 0, 0 };
inline SectionInfo g_rdata{ 0, 0 };

inline uintptr_t g_textEnd = 0;
inline uintptr_t g_dataEnd = 0;
inline uintptr_t g_rdataEnd = 0;

inline std::vector<uintptr_t> g_vtableRefs;
inline std::atomic<bool> g_cacheReady{ false };
#endif