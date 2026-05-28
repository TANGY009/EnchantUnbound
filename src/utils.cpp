#include "utils.hpp"

#ifdef _WIN32
namespace Utils {
    
    void BuildCache() {
        if (g_cacheReady) return;
        
        unsigned int numThreads = std::thread::hardware_concurrency();
        std::vector<std::vector<uintptr_t>> threadResults(numThreads);
        std::vector<std::thread> threads;
        size_t totalRange = (size_t)g_textEnd - (size_t)g_text.start;
        size_t chunkSize = totalRange / numThreads;
        
        for (unsigned int t{}; t < numThreads; t++) {
            threads.emplace_back([&, t = t]() {
                threadResults[t].reserve(500000); 
                uint8_t* cur = (uint8_t*)g_text.start + (t * chunkSize);
                uint8_t* end = (t == numThreads - 1) ? (uint8_t*)g_textEnd : (cur + chunkSize);

                while (cur < end - 15) {
                    if ((*cur == 0x48 || *cur == 0x4C) && *(cur + 1) == 0x8D && (*(cur + 2) & 0xC7) == 0x05) {
                        uintptr_t target = (uintptr_t)cur + 7 + *(int32_t*)(cur + 3);
                        if ((target >= g_data.start && target <= g_dataEnd) || (target >= g_rdata.start && target <= g_rdataEnd)) {
                            threadResults[t].push_back(target);
                        }
                        cur += 7; 
                    } else {
                        size_t len = ldisasm(cur, true);
                        cur += (len > 0) ? len : 1; 
                    }
                }
                while (cur < end - 7) {
                    if ((*cur == 0x48 || *cur == 0x4C) && *(cur + 1) == 0x8D && (*(cur + 2) & 0xC7) == 0x05) {
                        uintptr_t target = (uintptr_t)cur + 7 + *(int32_t*)(cur + 3);
                        if ((target >= g_data.start && target <= g_dataEnd) || (target >= g_rdata.start && target <= g_rdataEnd)) {
                            threadResults[t].push_back(target);
                        }
                        cur += 7;
                    } else {
                        cur++;
                    }
                }
            });
        }
        for (auto& t : threads) t.join();
        
        size_t total{}, offset{};
        for (auto& v : threadResults) total += v.size();
        g_vtableRefs.resize(total);
        for (auto& v : threadResults) {
            memcpy(&g_vtableRefs[offset], v.data(), v.size() * sizeof(uintptr_t));
            offset += v.size();
        }
        g_cacheReady = true;
    }
}

namespace Scanner {

    uintptr_t ScanSpecial(uint8_t* start) {
        uint8_t* cur = start;
        while (cur < (uint8_t*)g_textEnd) {
            size_t instLen = ldisasm(cur, true);
            if (instLen == 0) break;

            if (*cur == 0xC3 || *cur == 0xCB) {
                LOGD("   |-- [Special] Reached RETN without finding a second vftable");
                break;
            }

            if ((*cur == 0x48 || *cur == 0x4C) && *(cur + 1) == 0x8D && (*(cur + 2) & 0xC7) == 0x05) {
                int32_t disp = *(int32_t*)(cur + 3);
                uintptr_t vt = (uintptr_t)cur + 7 + disp;
                if (Utils::IsValidVtable(vt)) {
                    LOGD("   |-- [Special] Actual Vftable found at IDA: %llX", Utils::ToFileOffset((uintptr_t)cur));
                    return (uintptr_t)cur;
                }
            }
            cur += instLen;
        }
        return 0;
    }

    uintptr_t ScanSub(uint8_t* func, const char* indent) {
        LOGD("%s-- Starting Scan", indent);
        uint8_t* cur = func;
        while (cur < (uint8_t*)g_textEnd) {
            size_t instLen = ldisasm(cur, true);
            if (instLen == 0) break;

            if (*cur == 0xC3 || *cur == 0xCB) {
                LOGD("%s-- RETN stopper hit: %llX", indent, Utils::ToFileOffset((uintptr_t)cur));
                break;
            }

            if ((*cur == 0x48 || *cur == 0x4C) && *(cur + 1) == 0x8D && (*(cur + 2) & 0xC7) == 0x05) {
                int32_t disp = *(int32_t*)(cur + 3);
                uintptr_t vt = (uintptr_t)cur + 7 + disp;
                if (Utils::IsValidVtable(vt)) {
                    LOGD("%s-- Vftable LEA found at IDA: %llX", indent, Utils::ToFileOffset((uintptr_t)cur));
                    LOGD("%s   |-- [HEX] %02X %02X %02X %02X %02X %02X %02X", indent, cur[0], cur[1], cur[2], cur[3], cur[4], cur[5], cur[6]);
                    LOGD("%s-- Address of the Vftable: %llX", indent, Utils::ToFileOffset(vt));
                    return (uintptr_t)cur;
                }
            }
            cur += instLen;
        }
        LOGD("%s-- Vftable lea not found", indent);
        LOGD("%s-- Scan Completed", indent);
        return 0;
    }

    uintptr_t ScanMain(uint8_t* start) {
        uintptr_t foundCall{};
        uint8_t* callInst = nullptr;
        uintptr_t foundVt{};
        bool foundSSO = false;
        LOGD("   |-- Starting Scan");
        uint8_t* cur = start;
        
        while (cur < (uint8_t*)g_textEnd) {
            size_t instLen = ldisasm(cur, true);
            if (instLen == 0) break;

            if (!foundVt && (*cur == 0x48 || *cur == 0x4C) && *(cur + 1) == 0x8D && (*(cur + 2) & 0xC7) == 0x05) {
                int32_t disp = *(int32_t*)(cur + 3);
                uintptr_t vt = (uintptr_t)cur + 7 + disp;
                if (Utils::IsValidVtable(vt)) {
                    foundVt = (uintptr_t)cur;
                    break;
                }
            }
            if (!foundCall && *cur == 0xE8) {
                int32_t offset = *(int32_t*)(cur + 1);
                uintptr_t dest = (uintptr_t)cur + 5 + offset;
                if (dest > g_base && dest < g_moduleEnd) {
                    foundCall = dest;
                    callInst = cur;
                }
            }
            if (!foundSSO && *cur == 0x48 && *(cur + 1) == 0x83 && (*(cur + 2) & 0xF8) == 0xF8) {
                uint8_t imm = *(cur + 3);
                if (imm == 0x10 || imm == 0x0F) {
                    foundSSO = true;
                    LOGD("   |-- SSO Signature detected at %llX. Loosening safety stoppers.", Utils::ToFileOffset((uintptr_t)cur));
                }
            }
            if (*cur == 0xC3 || *cur == 0xCB) {
                LOGD("   |-- RETN stopper hit: %llX", Utils::ToFileOffset((uintptr_t)cur));
                break;
            }
            cur += instLen;
        }
        
        LOGD("   |-- Scan Completed");
        LOGD("   |   |-- Found Vftable Lea : %s", foundVt ? "Yes" : "No");
        if (foundVt) {
            LOGD("   |   |   |-- Found at %llX", Utils::ToFileOffset(foundVt));
            LOGD("   |   |   |-- [HEX] %02X %02X %02X %02X %02X %02X %02X", ((uint8_t*)foundVt)[0], ((uint8_t*)foundVt)[1], ((uint8_t*)foundVt)[2], ((uint8_t*)foundVt)[3], ((uint8_t*)foundVt)[4], ((uint8_t*)foundVt)[5], ((uint8_t*)foundVt)[6]);
        }
        LOGD("   |   |-- Found Function Call : %s", foundCall ? "Yes" : "No");
        if (foundCall) {
            LOGD("   |   |   |-- Found at %llX", Utils::ToFileOffset((uintptr_t)callInst));
            LOGD("   |   |   |-- [HEX] %02X %02X %02X %02X %02X %02X", callInst[0], callInst[1], callInst[2], callInst[3], callInst[4], callInst[5]);
        }
        
        if (foundVt) {
            int32_t disp = *(int32_t*)((uint8_t*)foundVt + 3);
            uintptr_t vt = (uintptr_t)foundVt + 7 + disp;
            LOGD("   |-- Vftable LEA found at IDA: %llX", Utils::ToFileOffset(foundVt));
            LOGD("   |-- Address of the Vftable: %llX", Utils::ToFileOffset(vt));
            return foundVt;
        }
        if (foundCall) {
            LOGD("   |-- Entering The Call");
            uintptr_t vtInst = ScanSub((uint8_t*)foundCall);
            if (!vtInst) {
                LOGD("   |   |-- Vftable lea not found, going deeper");
                LOGD("   |-- +1 Depth Scan");
                uint8_t* curSub = (uint8_t*)foundCall;
                while ((uintptr_t)curSub < g_moduleEnd) {
                    size_t subLen = ldisasm(curSub, true);
                    if (subLen == 0) break;

                    if (*curSub == 0xC3 || *curSub == 0xCB)
                        break;

                    if (*curSub == 0xE8) {
                        int32_t offset = *(int32_t*)(curSub + 1);
                        uintptr_t dest = (uintptr_t)curSub + 5 + offset;
                        if (dest > g_base && dest < g_moduleEnd) {
                            LOGD("   |   |-- Sub-call: %llX", Utils::ToFileOffset(dest));
                            uintptr_t nested = ScanSub((uint8_t*)dest, "   |   |   |");
                            if (nested) return nested;
                        }
                    }
                    curSub += subLen;
                }
            }
            else return vtInst;
        }
        return 0;
    }
}

uintptr_t FindStringInRdata(const char* str, size_t len) {
    uint8_t* start = (uint8_t*)g_rdata.start;
    size_t size = g_rdata.size;
    
    if (size < len) return 0;

    for (size_t j{}; j <= size - len; j++) {
        bool match = true;
        for (size_t k{}; k < len; k++) {
            if (start[j + k] != (uint8_t)str[k]) {
                match = false;
                break;
            }
        }
        if (match) return (uintptr_t)(start + j);
    }
    return 0;
}

void FindLEA(uintptr_t* strAddrs, uintptr_t* foundLeas, int count) {
    uintptr_t start = g_text.start;
    size_t totalSize = g_text.size;
    int validTargets{};
    for (int i{}; i < count; i++)
        if (strAddrs[i] != 0)
            validTargets++;

    if (validTargets == 0) return;

    int numThreads = (std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() : 1);
    size_t chunkSize = totalSize / numThreads;
    std::vector<std::thread> threads;
    std::atomic<int> totalFound{0};
    for (int t{}; t < numThreads; t++) {
        threads.emplace_back([&, t = t]() {
            uintptr_t cur = start + (t * chunkSize);
            uintptr_t end = (t == numThreads - 1) ? (start + totalSize) : (cur + chunkSize);
            while (cur < end - 7) {
                if (totalFound.load(std::memory_order_relaxed) >= validTargets)
                    return;
                    
                if (!(((*(uint8_t*)cur == 0x48 || *(uint8_t*)cur == 0x4C)) && *(uint8_t*)(cur + 1) == 0x8D && ((*(uint8_t*)(cur + 2) & 0xC7) == 0x05))) {
                    cur++;
                    continue;
                }
                
                int32_t disp = *(int32_t*)(cur + 3);
                uintptr_t target = cur + 7 + disp;
                for (int k{}; k < count; k++) {
                    if (strAddrs[k] != 0 && foundLeas[k] == 0 && target == strAddrs[k]) {
                        std::atomic<uintptr_t>& atomicLea = reinterpret_cast<std::atomic<uintptr_t>&>(foundLeas[k]);
                        uintptr_t expected{};
                        if (atomicLea.compare_exchange_strong(expected, cur, std::memory_order_relaxed)) {
                            totalFound.fetch_add(1, std::memory_order_relaxed);
                            break;
                        }
                    }
                }
                cur += 7;
            }
        });
    }
    for (auto& th : threads)
        th.join();
}

void ReplaceRefs(std::atomic<int>& totalReplaced, int targetCount) {
    struct ScanRange { uintptr_t start; size_t size; DWORD oldProtect; };
    ScanRange ranges[] = { 
        { g_data.start, g_data.size, 0 },
        { g_rdata.start, g_rdata.size, 0 } 
    };

    for (auto& range : ranges)
        VirtualProtect((LPVOID)range.start, range.size, PAGE_EXECUTE_READWRITE, &range.oldProtect);

    int numThreads = std::thread::hardware_concurrency();
    struct ThreadResult { int counts[4] = {0}; };
    std::vector<ThreadResult> threadResults(numThreads);
    std::vector<std::thread> threads;

    for (int i{}; i < numThreads; i++) {
        threads.emplace_back([&, i = i]() {
            int localCounts[4] = {0}; 
            
            for (const auto& range : ranges) {
                uintptr_t cur = (range.start + 7) & ~7;
                uintptr_t end = (range.start + range.size) - sizeof(void*);
                size_t totalLen = end - cur;
                size_t chunkSize = totalLen / numThreads;
                uintptr_t threadStart = (cur + (i * chunkSize) + 7) & ~7;
                uintptr_t nextThreadStart = (cur + ((i + 1) * chunkSize) + 7) & ~7;
                uintptr_t threadEnd = (i == numThreads - 1) ? end : nextThreadStart;

                for (uintptr_t j = threadStart; j < threadEnd; j += 8) {
                    void* value = *(void**)j;
                    for (int k{}; k < targetCount; k++) {
                        if (value == gTargets[k].original) {
                            *(void**)j = gTargets[k].replacement;
                            localCounts[k]++;
                            break;
                        }
                    }
                }
            }
            memcpy(threadResults[i].counts, localCounts, sizeof(localCounts));
        });
    }

    for (auto& th : threads) th.join();

    for (int i{}; i < numThreads; i++) {
        for (int k{}; k < targetCount; k++) {
            if (threadResults[i].counts[k] > 0) {
                gTargets[k].replaced += threadResults[i].counts[k];
                totalReplaced.fetch_add(threadResults[i].counts[k], std::memory_order_relaxed);
            }
        }
    }

    for (auto& range : ranges) {
        DWORD dummy;
        VirtualProtect((LPVOID)range.start, range.size, range.oldProtect, &dummy);
    }
}
#endif