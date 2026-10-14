#include "platform.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef PLATFORM_ANDROID
#include "raymob.h"
#endif
namespace plat {
void vibrate(int ms) {
#ifdef PLATFORM_ANDROID
    VibrateMS((uint64_t)ms);
#else
    (void)ms;
#endif
}
bool save(const char* name, const void* data, int size) {
#ifdef PLATFORM_ANDROID
    return WriteToAppStorage(name, (void*)data, (unsigned)size);   // внутреннее хранилище приложения, недоступно другим программам
#else
    FILE* f = fopen(name, "wb"); if (!f) return false; fwrite(data, 1, size, f); fclose(f); return true;
#endif
}
int load(const char* name, void* out, int maxSize) {
#ifdef PLATFORM_ANDROID
    if (!IsFileExistsInAppStorage(name)) return 0;
    int sz = 0; void* p = ReadFromAppStorage(name, &sz);
    if (!p) return 0; if (sz > maxSize) sz = maxSize; memcpy(out, p, sz); free(p); return sz;
#else
    FILE* f = fopen(name, "rb"); if (!f) return 0; int n = (int)fread(out, 1, maxSize, f); fclose(f); return n;
#endif
}
}
