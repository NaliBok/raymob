#include "art.h"
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstring>

namespace art {
namespace {
const Color kOutline = {12, 9, 20, 255};
uint32_t H(const char* s, uint32_t seed = 7) { uint32_t h = 2166136261u ^ seed; while (*s) h = (h ^ (unsigned char)*s++) * 16777619u; return h; }
Color Shade(Color c, int pct) { return {(unsigned char)std::min(255, c.r * pct / 100), (unsigned char)std::min(255, c.g * pct / 100), (unsigned char)std::min(255, c.b * pct / 100), c.a}; }
void Px(Image& im, int x, int y, Color c) { if (x >= 0 && y >= 0 && x < im.width && y < im.height) ImageDrawPixel(&im, x, y, c); }
void R(Image& im, int x, int y, int w, int h, Color c) { for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) Px(im, x + i, y + j, c); }
Color At(const Image& im, int x, int y) { return GetImageColor(im, x, y); }

// dark 1px outline around opaque pixels: gives the "one bit adventure" readability on any ground
void Outline(Image& im) {
  Image src = ImageCopy(im);
  for (int y = 0; y < im.height; y++) for (int x = 0; x < im.width; x++) {
    if (At(src, x, y).a) continue;
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    for (int k = 0; k < 4; k++) { int nx = x + dx[k], ny = y + dy[k]; if (nx >= 0 && ny >= 0 && nx < im.width && ny < im.height && At(src, nx, ny).a > 200 && At(src, nx, ny).r != kOutline.r) { ImageDrawPixel(&im, x, y, kOutline); break; } }
  }
  UnloadImage(src);
}
Texture2D ToTex(Image& im) { Texture2D t = LoadTextureFromImage(im); SetTextureFilter(t, TEXTURE_FILTER_POINT); UnloadImage(im); return t; }

Color Skin(const gd::RaceDef& r, uint32_t h) {
  if (strcmp(r.id, "undead") == 0) return {(unsigned char)(150 + h % 30), (unsigned char)(190 + h % 20), 150, 255};
  static const Color tones[3] = {{240, 200, 160, 255}, {220, 170, 130, 255}, {190, 140, 100, 255}};
  return tones[h % 3];
}
Color AtkGlow(int atk) {
  switch (atk) { case gd::ATK_MAGIC: return {120, 200, 255, 255}; case gd::ATK_CHAOS: return {200, 90, 255, 255}; case gd::ATK_SIEGE: return {255, 150, 60, 255}; case gd::ATK_PIERCE: return {230, 230, 160, 255}; default: return {230, 230, 235, 255}; }
}

void DrawUnit(Image& im, const gd::UnitDef& u, const gd::RaceDef& race, int frame) {
  uint32_t h = H(u.name);
  Color main = {race.color.r, race.color.g, race.color.b, 255};
  Color alt = Shade(main, 60 + (int)(h % 40)), skin = Skin(race, h >> 3), glow = AtkGlow(u.atkType);
  bool ranged = u.rangeWc3 > 150, mage = u.atkType == gd::ATK_MAGIC || u.atkType == gd::ATK_CHAOS, big = u.hp >= 800;
  bool undead = strcmp(race.id, "undead") == 0;
  R(im, 4, 14, 8, 1, {0, 0, 0, 70});                        // shadow
  int bw = big ? 10 : 8, bx = big ? 3 : 4;
  R(im, 5, 11, 2, 3 - (frame ? 1 : 0), Shade(alt, 70));             // legs alternate
  R(im, 9, 11, 2, 3 - (frame ? 0 : 1), Shade(alt, 70));
  R(im, bx, 6, bw, 5, main); R(im, bx, 9, bw, 2, alt);          // torso + belt
  R(im, bx, 6, bw, 1, Shade(main, 125));
  R(im, 6, 2, 4, 4, skin); Px(im, 7, 3, kOutline); Px(im, 9, 3, kOutline);     // head + eyes
  if (undead) { Px(im, 7, 3, {255, 60, 60, 255}); Px(im, 9, 3, {255, 60, 60, 255}); R(im, 7, 5, 2, 1, kOutline); }
  if (mage) {                                                                  // pointed hat + staff with glow
    R(im, 5, 1, 6, 1, alt); R(im, 6, 0, 4, 1, alt); R(im, 7, -1 + 1, 2, 1, Shade(alt, 120));
    R(im, 13, 4, 1, 9, {120, 80, 50, 255}); R(im, 12, 2, 3, 2, glow);
  } else if (ranged) {                                                         // hood + bow
    R(im, 5, 1, 6, 2, Shade(main, 80)); R(im, 12, 3, 1, 9, {150, 100, 60, 255}); Px(im, 11, 3, {150, 100, 60, 255}); Px(im, 11, 11, {150, 100, 60, 255});
    R(im, 13, 4, 1, 7, {230, 230, 230, 255});
  } else {                                                                     // helmet + blade / club
    R(im, 5, 1, 6, 2, {150, 150, 165, 255}); R(im, 5, 3, 1, 1, {150, 150, 165, 255}); R(im, 10, 3, 1, 1, {150, 150, 165, 255});
    if (u.atkType == gd::ATK_SIEGE) { R(im, 12, 4, 3, 5, {110, 100, 90, 255}); R(im, 12, 9, 1, 4, {120, 80, 50, 255}); }
    else { R(im, 12, 3, 1, 7, glow); R(im, 11, 9, 3, 1, {200, 170, 60, 255}); }
  }
  if (big) { Px(im, 4, 1, {235, 235, 235, 255}); Px(im, 11, 1, {235, 235, 235, 255}); }   // horns/crest on heavy units
  Outline(im);
}

void Brick(Image& im, int x, int y, int w, int h, Color c, uint32_t seed) {
  R(im, x, y, w, h, c);
  for (int j = 0; j < h; j += 3) for (int i = 0; i < w; i++) if (((i + (j / 3) * 3 + (int)seed) % 6) == 0) Px(im, x + i, y + j, Shade(c, 78));
}

void DrawBuilding(Image& im, const gd::BuildingDef& b, const gd::RaceDef& race) {
  uint32_t h = H(b.name, 99);
  int S = b.size * 16;
  Color main = {race.color.r, race.color.g, race.color.b, 255};
  Color stone = strcmp(race.id, "undead") == 0 ? Color{86, 82, 100, 255} : Color{150, 138, 120, 255};
  if (b.isTower) {
    Brick(im, 3, 5, 10, 10, stone, h); R(im, 2, 3, 12, 3, Shade(stone, 115));
    for (int i = 2; i < 14; i += 4) R(im, i, 1, 2, 3, Shade(stone, 115));
    R(im, 7, 9, 2, 4, kOutline); Px(im, 7, 7, AtkGlow(b.towAtk)); Px(im, 8, 7, AtkGlow(b.towAtk));
    R(im, 6, 0, 4, 1, main);
    Outline(im); return;
  }
  int wallTop = S / 3 + 2;
  Brick(im, 2, wallTop, S - 4, S - wallTop - 1, stone, h);
  int style = (int)(h % 3);
  if (style == 0) {                                      // pitched roof
    for (int r = 0; r < wallTop - 1; r++) { int rw = 4 + r * (S - 6) / std::max(1, wallTop - 2); R(im, S / 2 - rw / 2, r + 1, rw, 1, Shade(main, 90 + r * 4)); }
    R(im, 1, wallTop - 2, S - 2, 2, Shade(main, 70));
  } else if (style == 1) {                               // flat top with banner
    R(im, 1, wallTop - 3, S - 2, 3, Shade(main, 85));
    R(im, S / 2, 1, 1, wallTop - 3, {90, 70, 50, 255}); R(im, S / 2 + 1, 1, 5, 3, main);
  } else {                                               // twin turrets
    R(im, 1, wallTop - 5, 4, 6, Shade(main, 95)); R(im, S - 5, wallTop - 5, 4, 6, Shade(main, 95)); R(im, 1, wallTop, S - 2, 2, Shade(main, 70));
  }
  R(im, S / 2 - 2, S - 7, 4, 6, kOutline);               // door
  Px(im, S / 2 + 1, S - 4, {230, 190, 70, 255});
  R(im, 4, wallTop + 3, 2, 2, {250, 220, 120, 255}); R(im, S - 6, wallTop + 3, 2, 2, {250, 220, 120, 255});   // lit windows
  if (strcmp(race.id, "undead") == 0) { R(im, S / 2 - 1, wallTop + 2, 3, 3, {235, 235, 225, 255}); Px(im, S / 2 - 1, wallTop + 3, kOutline); Px(im, S / 2 + 1, wallTop + 3, kOutline); }
  if (b.legendary) { R(im, S / 2 - 1, 0, 3, 2, {255, 215, 80, 255}); }
  Outline(im);
}

void DrawCastle(Image& im, Color team) {
  Color stone = {128, 124, 140, 255};
  Brick(im, 2, 12, 60, 19, stone, 3);
  for (int x : {0, 52}) { Brick(im, x, 4, 12, 27, Shade(stone, 90), x); for (int i = 0; i < 12; i += 4) R(im, x + i, 2, 2, 3, Shade(stone, 110)); R(im, x + 5, 12, 2, 4, kOutline); }
  for (int i = 14; i < 50; i += 6) R(im, i, 9, 3, 4, Shade(stone, 112));
  R(im, 26, 18, 12, 13, kOutline); R(im, 28, 20, 8, 11, {60, 40, 30, 255}); R(im, 31, 24, 2, 7, {230, 190, 70, 255});
  R(im, 6, 0, 1, 4, {90, 70, 50, 255}); R(im, 7, 0, 5, 3, team); R(im, 57, 0, 1, 4, {90, 70, 50, 255}); R(im, 52, 0, 5, 3, team);
  R(im, 31, 3, 2, 7, team);
  Outline(im);
}

void DrawGrass(Image& im, uint32_t seed) {
  R(im, 0, 0, 16, 16, {34, 54, 40, 255});
  for (int i = 0; i < 9; i++) { uint32_t r = (seed + i * 2654435761u) >> 7; Px(im, (int)(r % 16), (int)((r >> 4) % 16), {48, 76, 52, 255}); }
  Px(im, (int)((seed * 7) % 15), (int)((seed * 3) % 15), {70, 100, 62, 255});
}
}

void Build(Atlas& a) {
  memset(&a, 0, sizeof a);
  const auto& D = gd::Data();
  for (int i = 0; i < D.unitCount && i < kMaxUnits; i++)
    for (int f = 0; f < 2; f++) { Image im = GenImageColor(16, 16, BLANK); DrawUnit(im, D.units[i], D.races[D.units[i].race], f); a.unit[i][f] = ToTex(im); }
  for (int i = 0; i < D.buildingCount && i < kMaxBuildings; i++) {
    int S = D.buildings[i].size * 16; Image im = GenImageColor(S, S, BLANK); DrawBuilding(im, D.buildings[i], D.races[D.buildings[i].race]); a.building[i] = ToTex(im);
  }
  { Image im = GenImageColor(16, 16, BLANK); R(im, 3, 6, 10, 9, {120, 80, 50, 255}); R(im, 3, 6, 10, 2, {160, 110, 60, 255}); R(im, 7, 8, 2, 2, {230, 190, 70, 255}); R(im, 4, 3, 8, 3, {230, 190, 70, 255}); Outline(im); a.treasure = ToTex(im); }
  for (int t = 0; t < 2; t++) { Image im = GenImageColor(64, 32, BLANK); DrawCastle(im, t == 0 ? Color{70, 130, 255, 255} : Color{230, 70, 70, 255}); a.castle[t] = ToTex(im); }
  for (int i = 0; i < 3; i++) { Image im = GenImageColor(16, 16, BLANK); DrawGrass(im, 17 + i * 31); a.grass[i] = ToTex(im); }
  { Image im = GenImageColor(16, 16, BLANK); DrawGrass(im, 5); R(im, 2, 5, 12, 9, {110, 108, 120, 255}); R(im, 4, 3, 8, 3, {130, 128, 140, 255}); R(im, 3, 6, 4, 2, {150, 148, 160, 255}); Outline(im); a.rock = ToTex(im); }
  for (int t = 0; t < 2; t++) {
    Image im = GenImageColor(16, 16, BLANK); Color base = t == 0 ? Color{44, 58, 84, 255} : Color{84, 48, 58, 255};
    R(im, 0, 0, 16, 16, base); R(im, 0, 0, 16, 1, Shade(base, 125)); R(im, 0, 0, 1, 16, Shade(base, 125)); R(im, 15, 0, 1, 16, Shade(base, 75)); R(im, 0, 15, 16, 1, Shade(base, 75));
    a.zone[t] = ToTex(im);
  }
  { Image im = GenImageColor(8, 8, BLANK); R(im, 1, 1, 6, 6, {250, 210, 60, 255}); R(im, 2, 2, 4, 4, {255, 240, 140, 255}); Px(im, 3, 3, {200, 150, 30, 255}); a.coin = ToTex(im); }
  { Image im = GenImageColor(8, 8, BLANK); R(im, 1, 3, 6, 2, {150, 100, 60, 255}); R(im, 1, 3, 1, 2, {190, 140, 90, 255}); R(im, 2, 1, 4, 2, {150, 100, 60, 255}); a.wood = ToTex(im); }
  for (int f = 0; f < 2; f++) { Image im = GenImageColor(5, 4, BLANK); if (f == 0) { R(im, 0, 0, 2, 3, {150, 220, 255, 255}); R(im, 3, 0, 2, 3, {150, 220, 255, 255}); Px(im, 2, 1, {255, 255, 255, 255}); } else { R(im, 1, 1, 1, 2, {120, 190, 255, 255}); R(im, 3, 1, 1, 2, {120, 190, 255, 255}); Px(im, 2, 1, {255, 255, 255, 255}); } a.butterfly[f] = ToTex(im); }
}

void Free(Atlas& a) {
  for (auto& u : a.unit) for (auto& t : u) if (t.id) UnloadTexture(t);
  for (auto& t : a.building) if (t.id) UnloadTexture(t);
  for (auto t : {a.treasure, a.castle[0], a.castle[1], a.grass[0], a.grass[1], a.grass[2], a.rock, a.zone[0], a.zone[1], a.coin, a.wood, a.butterfly[0], a.butterfly[1]}) if (t.id) UnloadTexture(t);
}
}
