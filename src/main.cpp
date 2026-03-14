#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <android/log.h>
#include "pl/Gloss.h"

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "EnchantUnbound", __VA_ARGS__)

static int g_GroupOffset{};

// For Enchant::isCompatibleWith TridentChannelingEnchant::isCompatibleWith TridentRiptideEnchant::isCompatibleWith CrossbowEnchant::isCompatibleWith
bool Enchant_isCompatibleWith(void* a1, uint8_t a2) {
    int Group = *(int*)((uintptr_t)a1 + g_GroupOffset);
    //LOG("Group=%d 1stAnvilSlot=%u", Group, a2);
    if (Group == 2 && (a2 == 16 || a2 == 18)) {
        if (a2 == 16) LOG("Blocked Silk Touch + Fortune!");
        if (a2 == 18) LOG("Blocked Fortune + Silk Touch!");
        return false;
    }
    if ((Group == 0 || Group == 6) && (a2 == 30 || a2 == 31 || a2 == 32)) {
        if (Group == 0 && a2 == 30) LOG("Blocked Riptide + Channeling!");
        if (Group == 6 && a2 == 32) LOG("Blocked Channeling + Riptide!");
        if (Group == 6 && a2 == 30) LOG("Blocked Riptide + Loyalty!");
        if (Group == 6 && a2 == 31) LOG("Blocked Loyalty + Riptide!");
        return false;
    }
    return true;
}

void** FindVtable(const char* cls) {
    static uintptr_t rodata{}, drr{}, zts{}, zti{};
    static size_t rodataSize{}, drrSize{};
    if (!rodata) {
        rodata = GlossGetLibSection("libminecraftpe.so", ".rodata", &rodataSize);
        drr = GlossGetLibSection("libminecraftpe.so", ".data.rel.ro", &drrSize);
    }
    char* s = (char*)memmem((void*)rodata, rodataSize, cls, strlen(cls) + 1);
    if (s) zts = (uintptr_t)s;
    if (!zts) return nullptr;
    for (size_t i = 0; i < drrSize; i += sizeof(uintptr_t))
        if (*(uintptr_t*)(drr + i) == zts) {
            zti = drr + i - sizeof(uintptr_t);
            break;
        }
    if (!zti) return nullptr;
    for (size_t i = 0; i < drrSize; i += sizeof(uintptr_t))
        if (*(uintptr_t*)(drr + i) == zti)
            return (void**)(drr + i + sizeof(uintptr_t));
    return nullptr;
}

void HookCompatible() {
    size_t drrSize{};
    uintptr_t drr = GlossGetLibSection("libminecraftpe.so", ".data.rel.ro", &drrSize);
    uintptr_t end = drr + drrSize;
    int replaced{};
    auto Redirect = [&](const char* sym, std::initializer_list<int> idx, uintptr_t hook) {
        void** vt = FindVtable(sym);
        if (!vt) {
            LOG("%s not found", sym);
            return;
        }
        for (int i : idx) {
            uintptr_t func = (uintptr_t)vt[i];
            for (uintptr_t p = drr; p < end; p += sizeof(uintptr_t)) {
                uintptr_t* entry = (uintptr_t*)p;
                if (*entry == func) {
                    Unprotect((uintptr_t)entry, sizeof(uintptr_t));
                    *entry = hook;
                    replaced++;
                }
            }
        }
    };
    if (void** vt = FindVtable("14MendingEnchant")) {
        uintptr_t func = (uintptr_t)vt[2];
        g_GroupOffset = ((*(uint32_t*)func >> 10) & 0xFFF) * 4;
        Redirect("14MendingEnchant", {2}, (uintptr_t)Enchant_isCompatibleWith);
    }
    Redirect("24TridentChannelingEnchant", {2}, (uintptr_t)Enchant_isCompatibleWith);
    Redirect("21TridentRiptideEnchant", {2}, (uintptr_t)Enchant_isCompatibleWith);
    Redirect("15CrossbowEnchant", {2}, (uintptr_t)Enchant_isCompatibleWith);
    LOG("redirected %d vtable references", replaced);
}

__attribute__((constructor))
void init() {
    LOG("EnchantUnbound Loaded");
    HookCompatible();
}