#include "main.hpp"
#include "utils.hpp"

// For Enchant::isCompatibleWith TridentChannelingEnchant::isCompatibleWith TridentRiptideEnchant::isCompatibleWith CrossbowEnchant::isCompatibleWith
namespace Enchant {
    
    bool isCompatibleWith(void* a1, uint8_t ID) {
        int CompatibilityID = *(int*)((uintptr_t)a1 + 0x1C);
        
        if (CompatibilityID == 2 && (ID == 16 || ID == 18)) {
            if (ID == 16) LOG("Blocked Silk Touch + Fortune!");
            if (ID == 18) LOG("Blocked Fortune + Silk Touch!");
            return false;
        }
        
        if ((CompatibilityID == 0 || CompatibilityID == 6) && (ID == 30 || ID == 31 || ID == 32)) {
            if (CompatibilityID == 0 && ID == 30) LOG("Blocked Riptide + Channeling!");
            if (CompatibilityID == 6 && ID == 32) LOG("Blocked Channeling + Riptide!");
            if (CompatibilityID == 6 && ID == 30) LOG("Blocked Riptide + Loyalty!");
            if (CompatibilityID == 6 && ID == 31) LOG("Blocked Loyalty + Riptide!");
            return false;
        }
        
        return true;
    }
    
}

#ifdef __ANDROID__
uintptr_t GetLibSection(const char* libname, const char* section_name, size_t* out_size) {
    if (!libname) return 0;
    if (!section_name) section_name = ".text";

    uintptr_t base_addr{};
    char lib_path[512] = {0};

    FILE* maps = fopen("/proc/self/maps", "r");
    if (!maps) return 0;

    char line[512];
    while (fgets(line, sizeof(line), maps)) {
        if (strstr(line, libname)) {
            char path[256] = {0};
            if (sscanf(line, "%llx-%*x %*s %*x %*s %*d %255s", (unsigned long long*)&base_addr, path) >= 1) {
                if (path[0] == '/') {
                    strncpy(lib_path, path, sizeof(lib_path) - 1);
                    break;
                }
            }
        }
    }
    fclose(maps);

    if (lib_path[0] == '\0' || base_addr == 0) return 0;

    int fd = open(lib_path, O_RDONLY);
    if (fd < 0) return 0;

    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return 0;
    }

    void* map_base = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (map_base == MAP_FAILED) return 0;

    uintptr_t section_runtime_addr{};
    
    ElfW(Ehdr)* ehdr = (ElfW(Ehdr)*)map_base;

    if (ehdr->e_ident[EI_MAG0] == ELFMAG0 && ehdr->e_ident[EI_MAG1] == ELFMAG1 && ehdr->e_ident[EI_MAG2] == ELFMAG2 && ehdr->e_ident[EI_MAG3] == ELFMAG3) {
        
        ElfW(Shdr)* shdr = (ElfW(Shdr)*)((uintptr_t)map_base + ehdr->e_shoff);
        const char* shstrtab = (const char*)((uintptr_t)map_base + shdr[ehdr->e_shstrndx].sh_offset);

        for (int i{}; i < ehdr->e_shnum; i++) {
            const char* current_section_name = shstrtab + shdr[i].sh_name;
            
            if (strcasecmp(current_section_name, section_name) == 0) {
                section_runtime_addr = base_addr + shdr[i].sh_addr;
                if (out_size) {
                    *out_size = shdr[i].sh_size;
                }
                break;
            }
        }
    }

    munmap(map_base, st.st_size);
    return section_runtime_addr;
}

bool SetMemoryPermission(uintptr_t addr, size_t len, int prot) {
    if (!addr || !len) return false;
    size_t pagesize = sysconf(_SC_PAGESIZE);
    uintptr_t aligned_addr = addr & ~(pagesize - 1);
    size_t aligned_len = ((addr + len + pagesize - 1) & ~(pagesize - 1)) - aligned_addr;
    return mprotect((void*)aligned_addr, aligned_len, prot) == 0;
}

inline bool Unprotect(uintptr_t addr, size_t len) {
    return SetMemoryPermission(addr, len, PROT_READ | PROT_WRITE);
}

inline bool Protect(uintptr_t addr, size_t len) {
    return SetMemoryPermission(addr, len, PROT_READ);
}

void** FindVtable(const char* typeStr) {
    static uintptr_t rodata{}, drr{}, libBase{};
    static size_t rodataSize{}, drrSize{};
    
    if (!rodata) {
        rodata = GetLibSection("libminecraftpe.so", ".rodata", &rodataSize);
        drr = GetLibSection("libminecraftpe.so", ".data.rel.ro", &drrSize);
        
        Dl_info info;
        if (dladdr((void*)rodata, &info)) {
            libBase = (uintptr_t)info.dli_fbase;
        }
    }
    
    char* ztsPtr= nullptr;
    size_t classLen = strlen(typeStr);
    size_t offset{};
    
    // Find String (_ZTS)
    while (offset < rodataSize) {
        char* match = (char*)memmem((void*)(rodata + offset), rodataSize - offset, typeStr, classLen + 1);
        if (!match) break;
        
        if (match == (char*)rodata || *(match - 1) == '\0') {
            ztsPtr = match;
            break;
        }
        offset = (uintptr_t)match - rodata + 1;
    }
    
    if (!ztsPtr) return nullptr;
    
    // Find TypeInfo (_ZTI)
    uintptr_t zts = (uintptr_t)ztsPtr;
    uintptr_t zti{};
    for (size_t i{}; i < drrSize; i += sizeof(uintptr_t)) {
        if (*(uintptr_t*)(drr + i) == zts) {
            zti = drr + i - sizeof(uintptr_t);
            break;
        }
    }
    
    if (!zti) return nullptr;
    
    // Find Vtable (_ZTV)
    uintptr_t vtable{};
    for (size_t i{}; i < drrSize; i += sizeof(uintptr_t)) {
        if (*(uintptr_t*)(drr + i) == zti) {
            uintptr_t potential_vtable = drr + i + sizeof(uintptr_t);
            
            if (i >= sizeof(uintptr_t) && *(uintptr_t*)(drr + i - sizeof(uintptr_t)) == 0) {
                vtable = potential_vtable;
                break; 
            }
            if (!vtable) {
                vtable = potential_vtable; 
            }
        }
    }
        
    if (!vtable) return nullptr;

    if (libBase)
        LOG("[Find] %s -> ZTS: 0x%lX | ZTI: 0x%lX | ZTV: 0x%lX", typeStr, zts - libBase, zti - libBase, vtable - libBase);
    else
        LOG("[Find] %s -> ZTS: .rodata+0x%zX | ZTI: .data.rel.ro+0x%zX | ZTV: .data.rel.ro+0x%zX", typeStr, (size_t)(zts - rodata), (size_t)(zti - drr), (size_t)(vtable - drr));
        
    return (void**)vtable;
}

void HookCompatible() {
    size_t drrSize{};
    uintptr_t drr = GetLibSection("libminecraftpe.so", ".data.rel.ro", &drrSize);
    uintptr_t end = drr + drrSize;
    int replaced{};

    auto FindTarget = [](const char* typeStr, int idx, uintptr_t hook) -> uintptr_t {
        struct CacheNode {
            uintptr_t slotAddr;
            uintptr_t originalVal;
            CacheNode* next;
        };
        
        static CacheNode* cacheHead = nullptr;

        void** vt = FindVtable(typeStr);
        if (!vt) return 0;
        
        uintptr_t slotAddr = (uintptr_t)&vt[idx];
        uintptr_t currentVal = (uintptr_t)vt[idx];
        
        if (currentVal != hook) {
            CacheNode* newNode = (CacheNode*)malloc(sizeof(CacheNode));
            if (newNode) {
                newNode->slotAddr = slotAddr;
                newNode->originalVal = currentVal;
                newNode->next = cacheHead;
                cacheHead = newNode;
            }
            return currentVal;
        }
        
        CacheNode* curr = cacheHead;
        while (curr) {
            if (curr->slotAddr == slotAddr) {
                return curr->originalVal;
            }
            curr = curr->next;
        }
        
        return currentVal;
    };
    
    auto Redirect = [&](const char* typeStr, int idx, uintptr_t hook) {
        uintptr_t targetPtr = FindTarget(typeStr, idx, hook);
        if (!targetPtr) {
            LOG("%s not found", typeStr);
            return;
        }
        
        for (uintptr_t p = drr; p < end; p += sizeof(uintptr_t)) {
            uintptr_t* entry = (uintptr_t*)p;
            if (*entry == targetPtr) {
                Unprotect((uintptr_t)entry, sizeof(uintptr_t));
                *entry = hook;
                replaced++;
                Protect((uintptr_t)entry, sizeof(uintptr_t));
            }
        }
    };
    
    Redirect("14MendingEnchant", 2, (uintptr_t)Enchant::isCompatibleWith);
    Redirect("24TridentChannelingEnchant", 2, (uintptr_t)Enchant::isCompatibleWith);
    Redirect("21TridentRiptideEnchant", 2, (uintptr_t)Enchant::isCompatibleWith);
    Redirect("15CrossbowEnchant", 2, (uintptr_t)Enchant::isCompatibleWith);
    
    LOG("redirected %d vtable references", replaced);
}

__attribute__((constructor))
void Init() {
    LOG("EnchantUnbound Loaded");
    HookCompatible();
}
#endif

#ifdef _WIN32
SectionInfo GetSection(const char* name) {
    size_t len = strlen(name);
    auto sec = IMAGE_FIRST_SECTION(g_nt);
    for (int i{}; i < g_nt->FileHeader.NumberOfSections; i++, sec++) {
        if (memcmp(sec->Name, name, len) == 0) {
            return {
                g_base + sec->VirtualAddress,
                sec->Misc.VirtualSize
            };
        }
    }
    return { 0, 0 };
}

void Init() {
    g_base = (uintptr_t)GetModuleHandleA(nullptr);
    g_nt = (PIMAGE_NT_HEADERS)(g_base + ((PIMAGE_DOS_HEADER)g_base)->e_lfanew);
    g_moduleEnd = g_base + g_nt->OptionalHeader.SizeOfImage;
    g_text = GetSection(".text");
    g_data = GetSection(".data");
    g_rdata = GetSection(".rdata");
    g_textEnd = g_text.start + g_text.size;
    g_dataEnd = g_data.start + g_data.size;
    g_rdataEnd = g_rdata.start + g_rdata.size;
    
    Utils::BuildCache();
    
    struct TargetString { const char* str; size_t len; };
    const TargetString names[4] = {
        {"enchantment.tridentLoyalty", sizeof("enchantment.tridentLoyalty") - 1},
        {"enchantment.crossbowMultishot", sizeof("enchantment.crossbowMultishot") - 1},
        {"enchantment.tridentChanneling", sizeof("enchantment.tridentChanneling") - 1},
        {"enchantment.tridentRiptide", sizeof("enchantment.tridentRiptide") - 1}
    };
    
    LOG("EnchantUnbound Initialization");
    
    auto startTime = std::chrono::high_resolution_clock::now();
    std::atomic<int> totalReferences{0};
    uintptr_t targetStrAddrs[4] = {0};
    uintptr_t foundLeas[4] = {0};
    std::thread strThreads[4];
    
    for (int i{}; i < 4; i++) {
        strThreads[i] = std::thread([&, i]() {
            targetStrAddrs[i] = FindStringInRdata(names[i].str, names[i].len);
        });
    }

    for (int i{}; i < 4; i++)
        strThreads[i].join();

    FindLEA(targetStrAddrs, foundLeas, 4);
    
    struct TempResult { 
        uintptr_t vtableAddr{}; 
        uintptr_t leaUsed{}; 
    } results[4];
    
    for (int i{}; i < 4; i++) {
        if (!foundLeas[i]) continue;
        LOGD("[TARGET] %s", names[i].str);
        uintptr_t vtInstAddr = Scanner::ScanMain((uint8_t*)(foundLeas[i] + 7));
        if (vtInstAddr) {
            int32_t disp = *(int32_t*)((uint8_t*)vtInstAddr + 3);
            results[i].vtableAddr = (uintptr_t)((uint8_t*)vtInstAddr + 7 + disp);
            results[i].leaUsed = vtInstAddr;
        }
    }
    
    bool allSame = (results[0].vtableAddr != 0);
    for (int i{}; i < 4; i++) {
        if (results[i].vtableAddr != results[0].vtableAddr) {
            allSame = false;
            break;
        }
    }
    
    if (allSame && results[0].vtableAddr != 0) {
        LOGD("Detected same vtable. Starting Specialized Scan...");
        for (int i{}; i < 4; i++) {
            uintptr_t spec = Scanner::ScanSpecial((uint8_t*)(results[i].leaUsed + 7));
            if (spec) {
                results[i].leaUsed = spec;
                int32_t disp = *(int32_t*)((uint8_t*)spec + 3);
                results[i].vtableAddr = (uintptr_t)((uint8_t*)spec + 7 + disp);
            }
        }
    }
    
    int validCount{};
    for (int i{}; i < 4; i++) {
        if (!results[i].vtableAddr) continue;
        
        void* original = ((void**)results[i].vtableAddr)[1];
        bool duplicate = false;
        
        for (int j{}; j < validCount; j++) {
            if (gTargets[j].original == original) {
                duplicate = true;
                break;
            }
        }
        
        if (duplicate) continue;
        
        gTargets[validCount].name = names[i].str;
        gTargets[validCount].replacement = (void*)&Enchant::isCompatibleWith;
        gTargets[validCount].original = original;
        validCount++;
    }
    
    ReplaceRefs(totalReferences, validCount);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = endTime - startTime;
    LOG("Redirected %d references in %.3fs", totalReferences.load(), elapsed.count());
    
    #ifdef _DEV
    DumpDebugReport(); 
    LOG("Final Replacements");
    for (int i{}; i < 4; i++)
        if (gTargets[i].name)
            LOG("%s -> %d Replacements", gTargets[i].name, gTargets[i].replaced);
    #endif
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
            #ifdef _DEV
            InitConsole();
            #endif
            Init();
            return 0;
        }, nullptr, 0, nullptr);
    }
    return TRUE;
}
#endif