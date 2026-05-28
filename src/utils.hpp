#pragma once

#include <cstdint>
#include <atomic>
#include <vector>

#include "main.hpp" 

#ifdef _WIN32

namespace Utils {
    
    void BuildCache();

    inline uintptr_t ToFileOffset(uintptr_t runtimeAddr) {
        return runtimeAddr - g_base;
    }
    
    inline bool IsInTextSection(uintptr_t addr) {
        return addr >= g_text.start && addr < g_textEnd;
    }
    
    inline bool IsInVtableStorage(uintptr_t addr) {
        return (addr >= g_rdata.start && addr < g_rdataEnd) || (addr >= g_data.start  && addr < g_dataEnd);
    }
    
    inline bool IsInModule(uintptr_t addr) {
        return addr >= g_base && addr < g_moduleEnd;
    }
    
    inline int CountVtableRefs(uintptr_t vt) {
        if (!vt) return 0;
        if (!g_cacheReady) BuildCache();
        int count{};
        for (uintptr_t addr : g_vtableRefs) {
            if (addr == vt) {
                count++;
                if (count > 3) return count;
            }
        }
        return count;
    }

    inline bool IsValidVtable(uintptr_t vt) {
        if (!IsInModule(vt) || !IsInModule(vt + sizeof(uintptr_t) - 1))
            return false;
        if (!IsInTextSection(*(uintptr_t*)vt))
            return false;
        int refs = CountVtableRefs(vt);
        if (refs > 3) 
            return false;
        return true;
    }
}

namespace Scanner {
    uintptr_t ScanSpecial(uint8_t* start);
    uintptr_t ScanSub(uint8_t* func, const char* indent = "   |   |");
    uintptr_t ScanMain(uint8_t* start);
}

uintptr_t FindStringInRdata(const char* str, size_t len);
void FindLEA(uintptr_t* strAddrs, uintptr_t* foundLeas, int count);
void ReplaceRefs(std::atomic<int>& totalReplaced, int targetCount);
#endif