#include "save.h"
#include "platform/platform.h"
#include <cstring>
static uint32_t sum(const SaveData& s) {
    uint32_t h = 2166136261u; const unsigned char* p = (const unsigned char*)&s; size_t n = sizeof(SaveData) - sizeof(uint32_t);
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    const char* salt = "cf-pixel-dawn-7"; while (*salt) { h ^= (unsigned char)*salt++; h *= 16777619u; }
    return h;
}
bool loadSave(SaveData& s) {
    SaveData t; int n = plat::load("save.dat", &t, sizeof(t));
    if (n != (int)sizeof(t) || t.magic != 0x43463031 || t.check != sum(t)) return false;
    s = t; return true;
}
void writeSave(SaveData& s) { s.check = sum(s); plat::save("save.dat", &s, sizeof(s)); }
