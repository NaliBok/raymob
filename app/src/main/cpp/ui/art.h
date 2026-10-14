#pragma once
// Procedural pixel art (16x16 per tile). Everything here can be replaced by PNGs later: keep the sizes
// (16x16 units, 16*N square buildings, 64x32 castle) and swap the generator calls for LoadTexture.
#include "raylib.h"
#include "../data/gamedata.h"

namespace art {
constexpr int kMaxUnits = 64, kMaxBuildings = 64;
struct Atlas {
  Texture2D unit[kMaxUnits][2];        // two walk frames
  Texture2D building[kMaxBuildings];   // size = def size * 16
  Texture2D treasure, castle[2];
  Texture2D grass[3], rock, zone[2];
  Texture2D coin, wood, butterfly[2];
};
void Build(Atlas& a);
void Free(Atlas& a);
}
