#include "save.h"
#include <cstdio>
#include <cstring>
#include "../platform/platform.h"

Settings gSettings; Progress gProgress;

namespace {
struct Blob { uint32_t magic, version; Settings s; Progress p; uint32_t check; };
constexpr uint32_t kMagic = 0x43464B31, kKey = 0x9E3779B1u;
uint32_t Sum(const Blob& b) {
  const unsigned char* d = (const unsigned char*)&b; uint32_t h = 2166136261u ^ kKey;
  for (size_t i = 0; i < offsetof(Blob, check); i++) h = (h ^ d[i]) * 16777619u;
  return h;
}
void Crypt(Blob& b) {            // light obfuscation (xor stream); integrity comes from the checksum
  unsigned char* d = (unsigned char*)&b; uint32_t k = kKey;
  for (size_t i = 0; i < sizeof(Blob); i++) { k = k * 1664525u + 1013904223u; d[i] ^= (unsigned char)(k >> 24); }
}
}

void WriteSave() {
  Blob b{}; b.magic = kMagic; b.version = 1; b.s = gSettings; b.p = gProgress; b.check = Sum(b);
  Crypt(b);
  FILE* f = fopen(plat::DataFile("save.dat").c_str(), "wb");
  if (f) { fwrite(&b, sizeof b, 1, f); fclose(f); }
}
void LoadSave() {
  FILE* f = fopen(plat::DataFile("save.dat").c_str(), "rb");
  if (!f) return;
  Blob b{}; size_t n = fread(&b, 1, sizeof b, f); fclose(f);
  if (n != sizeof b) return;
  Crypt(b);
  if (b.magic != kMagic || b.check != Sum(b)) return;       // tampered or corrupt: start fresh
  gSettings = b.s; gProgress = b.p; gProgress.unlockedRaces |= 1u;
  if (gSettings.wins < 1 || gSettings.wins > 3) gSettings.wins = 1;
}
