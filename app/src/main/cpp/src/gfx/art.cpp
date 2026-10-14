#include "art.h"
#include "sim/data.h"
#include <vector>
#include <unordered_map>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace art {
typedef Color C;
static inline C mk(int r, int g, int b, int a = 255) { return C{(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a}; }
static inline int cl(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }
static inline C shade(C c, int d) { return mk(cl(c.r + d), cl(c.g + d), cl(c.b + d), c.a); }
static inline C hsv(float h, float s, float v) { return ColorFromHSV(h, s, v); }
static uint32_t hs(const char* s) { uint32_t h = 2166136261u; while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; } return h; }
static uint32_t hi(uint32_t a, uint32_t b) { uint32_t h = a * 2654435761u ^ (b + 0x9E3779B9u + (a << 6)); h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; return h; }

static const C DARK = {20, 14, 28, 255};
C teamColor(int owner) { return owner == 0 ? mk(70, 140, 240) : mk(225, 70, 60); }
C raceColor(int race) {
    static const C T[12] = {{80,140,230,255},{200,160,255,255},{110,170,70,255},{80,60,170,255},{110,180,80,255},{60,170,190,255},
                            {150,150,170,255},{150,90,170,255},{150,210,240,255},{255,170,70,255},{170,170,180,255},{210,60,70,255}};
    return T[race % 12];
}

// ------------------------------------------------------------------ атлас
static Image atlas; static Texture2D atex; static bool inited = false;
static int curX = 0, curY = 0, rowH = 0;
static const int AW = 2048, AH = 1024;
static std::unordered_map<uint64_t, Rectangle> cache;

struct Cv {
    int w, h; std::vector<C> p;
    Cv(int W, int H) : w(W), h(H), p((size_t)W * H, BLANK) {}
    void px(int x, int y, C c) { if (x >= 0 && y >= 0 && x < w && y < h) p[(size_t)y * w + x] = c; }
    C get(int x, int y) const { return (x < 0 || y < 0 || x >= w || y >= h) ? BLANK : p[(size_t)y * w + x]; }
    void rect(int x, int y, int ww, int hh, C c) { for (int j = 0; j < hh; j++) for (int i = 0; i < ww; i++) px(x + i, y + j, c); }
    void outline(C c) {
        std::vector<C> q = p;
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) if (p[(size_t)y * w + x].a == 0) {
            if (get(x - 1, y).a || get(x + 1, y).a || get(x, y - 1).a || get(x, y + 1).a) q[(size_t)y * w + x] = c;
        }
        p = q;
    }
};
struct Pk { Cv* cv; int k; void R(int x, int y, int w, int h, C c) { cv->rect(x * k, y * k, w * k, h * k, c); } };

static Rectangle alloc(int w, int h) {
    if (curX + w > AW) { curX = 0; curY += rowH; rowH = 0; }
    if (curY + h > AH) { curX = 0; curY = 0; rowH = 0; }     // переполнение: перезапись (не должно случаться)
    Rectangle r = {(float)curX, (float)curY, (float)w, (float)h};
    curX += w; if (h > rowH) rowH = h; return r;
}
static Rectangle put(Cv& cv) {
    Rectangle r = alloc(cv.w, cv.h);
    C* a = (C*)atlas.data;
    for (int y = 0; y < cv.h; y++) memcpy(&a[(size_t)((int)r.y + y) * AW + (int)r.x], &cv.p[(size_t)y * cv.w], sizeof(C) * cv.w);
    UpdateTextureRec(atex, r, cv.p.data());
    return r;
}

void init() {
    atlas = GenImageColor(AW, AH, BLANK); atex = LoadTextureFromImage(atlas);
    SetTextureFilter(atex, TEXTURE_FILTER_POINT); inited = true; cache.clear(); curX = curY = rowH = 0;
}
void shutdown() { if (inited) { UnloadTexture(atex); UnloadImage(atlas); inited = false; } cache.clear(); }
Texture2D tex() { return atex; }
static uint64_t key(int a, int b, int c, int d, int e = 0) { return ((uint64_t)a << 48) | ((uint64_t)b << 36) | ((uint64_t)c << 24) | ((uint64_t)d << 12) | (uint64_t)e; }

// ------------------------------------------------------------------ юниты
struct Look { C skin, hair, body, legs, trim, steel, orb, wood; };
static C skinOf(int race) {
    switch (race) { case 6: return mk(210, 210, 185); case 7: return mk(125, 165, 105); case 11: return mk(175, 65, 60); case 2: return mk(105, 160, 70);
        case 5: return mk(70, 160, 170); case 9: return mk(210, 160, 255); case 10: return mk(170, 170, 185); case 4: return mk(150, 120, 80);
        case 8: return mk(175, 205, 235); default: return mk(240, 200, 160); }
}
static Look lookFor(int race, int def, const sim::UnitDef& d) {
    uint32_t h = hs(d.name);
    float hue = (float)((race * 29 + h % 70) % 360);
    Look L; L.skin = skinOf(race);
    L.body = hsv(hue, 0.50f, 0.78f); L.legs = shade(L.body, -60); L.hair = hsv(hue + 20, 0.5f, 0.35f);
    L.trim = hsv(hue + 40, 0.7f, 0.95f); L.steel = mk(205, 210, 225); L.wood = mk(130, 90, 50);
    L.orb = d.atk == sim::ATK_MAGIC ? mk(120, 200, 255) : (d.atk == sim::ATK_CHAOS ? mk(255, 80, 80) : mk(255, 220, 90));
    if (race == 6 || race == 7 || race == 11) { L.body = shade(L.body, -35); L.hair = mk(60, 50, 60); }
    (void)def; return L;
}
static void paintHuman(Pk& g, const Look& L, const sim::UnitDef& d, int frame, int owner) {
    C team = teamColor(owner);
    bool robe = d.weapon == sim::W_STAFF || d.weapon == sim::W_NONE;
    int lift = frame == 2 ? -1 : 0;
    g.R(5, 2, 5, 4, L.skin); g.R(5, 1, 5, 2, L.hair);
   
    if (owner == 1) { g.R(6, 4, 1, 1, DARK); g.R(8, 4, 1, 1, DARK); } else g.R(5, 3, 5, 1, L.hair);
    if (robe) { g.R(4, 6, 7, 8, L.body); g.R(4, 9, 7, 1, team); g.R(4, 13, 7, 1, shade(L.body, -35)); }
    else {
        g.R(4, 6, 7, 5, L.body); g.R(4, 9, 7, 1, team);
        int a = frame == 0 ? 0 : 1;
        g.R(5, 11, 2, 3 - a, L.legs); g.R(8, 11, 2, 2 + a, L.legs); g.R(5, 13 - a, 2, 1, DARK); g.R(8, 12 + a, 2, 1, DARK);
    }
    g.R(3, 7, 1, 3, L.skin);
    switch (d.weapon) {
        case sim::W_SWORD: g.R(12, 3 + lift, 1, 6, L.steel); g.R(11, 8 + lift, 3, 1, L.trim); g.R(11, 7, 1, 2, L.skin); g.R(1, 7, 3, 5, team); g.R(2, 8, 1, 3, L.trim); break;
        case sim::W_BOW: g.R(12, 3 + lift, 1, 8, L.wood); g.R(11, 3 + lift, 1, 1, L.wood); g.R(11, 10 + lift, 1, 1, L.wood); g.R(13, 4 + lift, 1, 6, L.steel); break;
        case sim::W_STAFF: g.R(12, 2 + lift, 1, 11, L.wood); g.R(11, 0 + lift, 3, 2, L.orb); break;
        case sim::W_CLAW: g.R(11, 8 + lift, 3, 1, L.steel); g.R(13, 9 + lift, 1, 1, L.steel); g.R(11, 7, 1, 2, L.skin); g.R(2, 8 + lift, 1, 2, L.steel); break;
        case sim::W_CLUB: g.R(12, 3 + lift, 2, 7, L.wood); g.R(11, 2 + lift, 4, 3, shade(L.wood, 20)); break;
        case sim::W_LANCE: g.R(13, 0 + lift, 1, 13, L.steel); g.R(12, 0, 3, 1, L.trim); break;
        default: g.R(11, 8, 1, 2, L.orb); break;
    }
    if (frame == 2) g.R(13, 9, 2, 1, mk(255, 255, 200));
}
static void paintKnight(Pk& g, const Look& L, const sim::UnitDef&, int frame, int owner) {
    C horse = shade(L.wood, 10), team = teamColor(owner); int a = frame == 0 ? 0 : 1;
    g.R(2, 7, 10, 5, horse); g.R(11, 5, 3, 4, horse); g.R(13, 6, 1, 1, DARK); g.R(1, 7, 1, 4, shade(horse, -40));
    g.R(3, 12, 2, 3 - a, shade(horse, -30)); g.R(9, 12, 2, 2 + a, shade(horse, -30)); g.R(6, 12, 2, 2 + a, shade(horse, -30));
    g.R(5, 6, 6, 1, team);
    g.R(6, 1, 4, 3, L.skin); g.R(6, 0, 4, 2, L.steel); g.R(5, 4, 6, 3, L.body); g.R(5, 5, 6, 1, team);
    g.R(13, 0 + (frame == 2 ? 1 : 0), 1, 9, L.steel); g.R(12, 0, 3, 1, L.trim);
}
static void paintSiege(Pk& g, const Look& L, const sim::UnitDef&, int frame, int owner) {
    C w = L.wood, w2 = shade(w, -35), team = teamColor(owner); int s = frame == 0 ? 0 : 1;
    g.R(2, 8, 12, 3, w); g.R(2, 8, 12, 1, shade(w, 25)); g.R(2, 10, 12, 1, w2);
    g.R(1, 10, 4, 4, w2); g.R(2, 11, 2, 2, mk(90, 90, 95)); g.R(2 + s, 11 + s, 1, 1, L.steel);
    g.R(11, 10, 4, 4, w2); g.R(12, 11, 2, 2, mk(90, 90, 95)); g.R(12 + s, 11 + s, 1, 1, L.steel);
    int up = frame == 2 ? 2 : 0;
    g.R(7, 2 - up, 2, 7 + up, w); g.R(5, 1 - up, 6, 2, w2); g.R(6, -1 + 0, 4, 1, L.trim);
    if (frame != 2) g.R(6, 0, 4, 2, mk(110, 110, 115));
    g.R(2, 6, 3, 2, team);
}
static void paintFlyer(Pk& g, const Look& L, const sim::UnitDef&, int frame, int owner) {
    C wing = shade(L.body, 10), team = teamColor(owner);
    if (frame == 1) { g.R(0, 7, 6, 3, wing); g.R(1, 9, 4, 2, shade(wing, -30)); g.R(10, 7, 6, 3, wing); g.R(11, 9, 4, 2, shade(wing, -30)); }
    else { g.R(0, 3, 6, 3, wing); g.R(0, 5, 4, 3, shade(wing, -30)); g.R(10, 3, 6, 3, wing); g.R(12, 5, 4, 3, shade(wing, -30)); }
    g.R(6, 5, 4, 6, L.body); g.R(6, 3, 4, 3, shade(L.body, 15)); g.R(7, 6, 2, 1, L.trim);
    if (owner == 1) { g.R(6, 4, 1, 1, DARK); g.R(9, 4, 1, 1, DARK); }
    g.R(7, 11, 2, 3, shade(L.body, -35)); g.R(5, 8, 6, 1, team);
    if (frame == 2) g.R(7, 1, 2, 2, mk(255, 255, 200));
}
static void paintGolem(Pk& g, const Look& L, const sim::UnitDef& d, int frame, int owner) {
    C team = teamColor(owner); int a = frame == 0 ? 0 : 1; int lift = frame == 2 ? -2 : 0;
    g.R(3, 4, 10, 8, L.body); g.R(3, 4, 10, 1, shade(L.body, 25)); g.R(5, 7, 6, 1, shade(L.body, -45)); g.R(7, 5, 1, 5, shade(L.body, -45));
    g.R(5, 1, 6, 4, L.skin); g.R(5, 1, 6, 1, shade(L.skin, 25));
    if (owner == 1) { g.R(6, 3, 1, 1, DARK); g.R(9, 3, 1, 1, DARK); }
    g.R(1, 5 + lift, 2, 7, L.skin); g.R(13, 5 + lift, 2, 7, L.skin); g.R(0, 11 + lift, 3, 2, shade(L.skin, -25)); g.R(13, 11 + lift, 3, 2, shade(L.skin, -25));
    g.R(4, 12, 3, 3 - a, L.legs); g.R(9, 12, 3, 2 + a, L.legs); g.R(3, 9, 10, 1, team);
    if (d.weapon == sim::W_CLUB) g.R(14, 1 + lift, 2, 6, L.wood);
}
int unitSize(int race, int def) { const sim::UnitDef& d = sim::race(race).u[def]; return (d.hp >= 1800 || d.radius >= 9) ? 32 : 16; }
Rectangle unit(int race, int def, int owner, int frame) {
    uint64_t k = key(1, race, def, owner, frame);
    auto it = cache.find(k); if (it != cache.end()) return it->second;
    const sim::UnitDef& d = sim::race(race).u[def];
    int S = unitSize(race, def), kk = S / 16;
    Cv cv(S, S); Pk g{&cv, kk}; Look L = lookFor(race, def, d);
    switch (d.art) { case sim::ART_KNIGHT: paintKnight(g, L, d, frame, owner); break; case sim::ART_SIEGE: paintSiege(g, L, d, frame, owner); break;
        case sim::ART_FLYER: paintFlyer(g, L, d, frame, owner); break; case sim::ART_GOLEM: paintGolem(g, L, d, frame, owner); break;
        default: paintHuman(g, L, d, frame, owner); }
    cv.outline(DARK);
    return cache[k] = put(cv);
}

// ------------------------------------------------------------------ здания
static void brick(Cv& cv, int x, int y, int w, int h, C base) {
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) {
        int row = j / 4; bool mortar = (j % 4 == 0) || ((i + (row & 1) * 4) % 8 == 0);
        cv.px(x + i, y + j, mortar ? shade(base, -28) : (((i * 7 + j * 3) & 7) == 0 ? shade(base, 12) : base));
    }
}
Rectangle building(int race, int def, int owner) {
    uint64_t k = key(2, race, def, owner, 0);
    auto it = cache.find(k); if (it != cache.end()) return it->second;
    const sim::BuildDef& b = sim::race(race).b[def];
    int W = b.w * 16, H = b.h * 16; Cv cv(W, H); Pk g{&cv, 1};
    uint32_t h = hs(b.name); bool dark = race == 6 || race == 7 || race == 11 || race == 5;
    float hue = (float)((race * 29 + (h >> 8) % 40) % 360);
    C wall = dark ? hsv(hue, 0.18f, 0.40f) : hsv(hue, 0.15f, 0.78f), roof = hsv(hue + 10, dark ? 0.55f : 0.65f, dark ? 0.40f : 0.62f);
    C glow = dark ? mk(150, 255, 160) : mk(255, 225, 120), team = teamColor(owner), wood = mk(120, 80, 45);
    int y0 = H * 40 / 100, shape = b.shape;
    auto gable = [&](int top, int base, int x0, int x1, C c) {
        for (int y = top; y < base; y++) { int t = (y - top) * ((x1 - x0) / 2) / std::max(1, base - top); cv.rect(x0 + (x1 - x0) / 2 - t - 1, y, 2 * t + 2, 1, (y - top) % 3 == 2 ? shade(c, -22) : c); }
    };
    auto door = [&](int w) { g.R(W / 2 - w / 2, H - 9, w, 8, DARK); g.R(W / 2 - w / 2 + 1, H - 10, w - 2, 1, DARK); g.R(W / 2 - w / 2, H - 9, 1, 8, wood); };
    switch (shape) {
    case 1: brick(cv, 2, y0 + 2, W - 4, H - y0 - 3, wall); g.R(1, y0, W - 2, 3, roof); g.R(1, y0 + 3, W - 2, 1, shade(roof, -40));
        g.R(W / 2 - 4, y0 + 6, 9, 9, mk(240, 240, 240)); g.R(W / 2 - 3, y0 + 7, 7, 7, mk(210, 50, 50)); g.R(W / 2 - 2, y0 + 8, 5, 5, mk(240, 240, 240)); g.R(W / 2 - 1, y0 + 9, 3, 3, mk(210, 50, 50)); door(5); break;
    case 2: brick(cv, 3, y0 + 4, W - 6, H - y0 - 5, wall); g.R(W / 2 - 4, 3, 8, y0 + 3, wall); gable(0, 8, W / 2 - 6, W / 2 + 6, roof);
        g.R(W / 2 - 1, 7, 2, 4, glow); g.R(W / 2 - 2, 8, 4, 2, glow); g.R(5, y0 + 8, 3, 4, glow); g.R(W - 8, y0 + 8, 3, 4, glow); door(5); break;
    case 3: brick(cv, 1, y0 + 4, W - 2, H - y0 - 5, shade(wall, -10)); g.R(0, y0, W, 5, roof); g.R(0, y0 + 5, W, 1, shade(roof, -45));
        g.R(3, H - 11, W / 2 - 5, 10, DARK); g.R(W / 2 + 2, H - 11, W / 2 - 5, 10, DARK); g.R(3, H - 11, W / 2 - 5, 2, wood); g.R(W / 2 + 2, H - 11, W / 2 - 5, 2, wood);
        for (int i = 0; i < 6; i++) g.R(2 + i * 4, y0 + 6 + (i & 1), 3, 2, mk(220, 190, 80)); break;
    case 4: { int tw = W / 2 + 2; int tx = (W - tw) / 2; brick(cv, tx, y0 - 6, tw, H - y0 + 5, wall);
        g.R(tx - 1, y0 - 8, tw + 2, 3, roof); for (int i = 0; i < tw + 2; i += 4) g.R(tx - 1 + i, y0 - 11, 2, 3, roof);
        g.R(W / 2 - 3, 2, 6, 6, glow); g.R(W / 2 - 2, 3, 4, 4, shade(glow, 40)); g.R(W / 2 - 1, 8, 2, y0 - 16, shade(wall, -40));
        g.R(W / 2 - 2, y0 + 2, 4, 5, glow); door(5); break; }
    case 5: brick(cv, 2, y0 + 1, W - 4, H - y0 - 2, wall); g.R(1, y0 - 2, W - 2, 4, roof); g.R(W - 10, 2, 5, y0, mk(90, 90, 100)); g.R(W - 11, 1, 7, 2, shade(mk(90, 90, 100), -30));
        g.R(W - 9, 0, 3, 2, mk(200, 200, 205, 200)); for (int i = 0; i < 12; i++) { double a = i * 0.5236; cv.px(10 + (int)(5 * cos(a)), y0 + 9 + (int)(5 * sin(a)), mk(200, 200, 210)); }
        door(9); break;
    case 6: g.R(2, H - 14, W - 4, 13, mk(105, 100, 105)); g.R(4, H - 18, W - 8, 6, mk(125, 120, 125));
        g.R(W / 2 - 9, H - 22, 18, 8, mk(150, 110, 60)); g.R(W / 2 - 6, H - 21, 12, 5, mk(95, 65, 35)); g.R(W / 2 - 2, H - 21, 4, 2, glow);
        for (int i = 0; i < 4; i++) g.R(5 + i * (W - 12) / 3, H - 26 - (i & 1) * 3, 2, 8, mk(200, 200, 190)); break;
    default: brick(cv, 2, y0 + 3, W - 4, H - y0 - 4, wall); gable(2, y0 + 5, 0, W, roof); g.R(5, y0 + 8, 4, 4, glow); g.R(W - 9, y0 + 8, 4, 4, glow); door(6); break;
    }
    if (dark && shape != 6) { g.R(W / 2 - 2, y0 + 1, 4, 3, mk(225, 225, 205)); g.R(W / 2 - 1, y0 + 2, 1, 1, DARK); g.R(W / 2 + 1, y0 + 2, 1, 1, DARK); }
    g.R(W - 6, 1, 1, 9, wood); g.R(W - 5, 1, 4, 3, team); g.R(W - 5, 4, 3, 1, shade(team, -30));
    cv.outline(DARK);
    return cache[k] = put(cv);
}

Rectangle castle(int owner, int damaged) {
    uint64_t k = key(3, owner, damaged, 0);
    auto it = cache.find(k); if (it != cache.end()) return it->second;
    Cv cv(80, 48); Pk g{&cv, 1}; C st = mk(150, 150, 165), team = teamColor(owner), wood = mk(120, 80, 45);
    brick(cv, 10, 16, 60, 31, st); for (int i = 0; i < 60; i += 8) g.R(10 + i, 13, 5, 4, st);
    for (int t = 0; t < 2; t++) { int x = t ? 64 : 0;
        brick(cv, x + 1, 8, 14, 39, shade(st, 10)); for (int i = 0; i < 14; i += 5) g.R(x + 1 + i, 4, 3, 5, shade(st, 10));
        g.R(x + 6, 16, 3, 6, DARK); g.R(x + 7, 0, 1, 8, wood); g.R(x + 8, 0, 6, 4, team); g.R(x + 8, 4, 4, 1, shade(team, -30)); }
    g.R(33, 28, 14, 19, DARK); g.R(34, 26, 12, 2, DARK); g.R(35, 25, 10, 1, DARK); g.R(33, 28, 14, 1, wood);
    for (int i = 0; i < 14; i += 3) g.R(33 + i, 28, 1, 19, mk(90, 90, 100)); g.R(36, 4, 8, 9, team); g.R(38, 6, 4, 5, mk(255, 215, 90));
    if (damaged) {
        for (int i = 0; i < 18; i++) { cv.px(20 + i, 20 + i / 2, DARK); cv.px(52 - i / 2, 18 + i, DARK); cv.px(36 + (i & 1), 30 + i / 2, DARK); }
        g.R(24, 13, 5, 4, BLANK); g.R(48, 13, 6, 4, BLANK); g.R(12, 30, 6, 5, shade(st, -60)); g.R(58, 36, 8, 6, shade(st, -60));
        for (int i = 0; i < 9; i++) g.R(30 + i * 3, 6 - (i % 3), 2, 2, mk(255, 120 + i * 10, 40)); }
    cv.outline(DARK);
    return cache[k] = put(cv);
}

// ------------------------------------------------------------------ тайлы и мелочи
Rectangle tile(int kind, int variant) {
    uint64_t k = key(4, kind, variant & 7, 0);
    auto it = cache.find(k); if (it != cache.end()) return it->second;
    Cv cv(16, 16); uint32_t v = variant & 7;
    auto grass = [&]() {
        C base = mk(78, 138, 58); cv.rect(0, 0, 16, 16, base);
        for (int i = 0; i < 18; i++) { uint32_t r = hi(v + 11, i); C c = (r & 1) ? shade(base, 18) : shade(base, -16); cv.px(r >> 8 & 15, r >> 12 & 15, c); }
        if (v == 2) { cv.px(4, 5, mk(250, 240, 120)); cv.px(11, 10, mk(255, 255, 255)); } if (v == 5) { cv.px(8, 3, mk(240, 120, 160)); cv.px(3, 12, mk(250, 240, 120)); }
    };
    switch (kind) {
    case TL_ZONE0: case TL_ZONE1: { C base = kind == TL_ZONE0 ? mk(112, 128, 156) : mk(150, 108, 124);
        cv.rect(0, 0, 16, 16, shade(base, -34)); cv.rect(0, 0, 15, 15, base); cv.rect(1, 1, 13, 1, shade(base, 22));
        for (int i = 0; i < 6; i++) { uint32_t r = hi(v + 3, i); cv.px(r >> 4 & 14, r >> 9 & 14, shade(base, 14)); } break; }
    case TL_ROCK: grass(); cv.rect(1, 3, 14, 12, mk(105, 105, 118)); cv.rect(2, 2, 12, 2, mk(130, 130, 145)); cv.rect(3, 4, 4, 2, mk(150, 150, 165));
        cv.rect(1, 12, 14, 3, mk(75, 75, 88)); cv.px(10, 7, mk(80, 80, 95)); cv.px(11, 8, mk(80, 80, 95)); break;
    case TL_WATER: { int f = v & 1; cv.rect(0, 0, 16, 16, mk(48, 98, 190));
        for (int i = 0; i < 4; i++) cv.rect(((i * 5 + f * 3) % 12) + 1, 2 + i * 4, 4, 1, mk(120, 175, 245)); cv.rect(0, 15, 16, 1, mk(40, 80, 160)); break; }
    case TL_TREE: grass(); cv.rect(7, 9, 2, 6, mk(100, 65, 35)); cv.rect(3, 1, 10, 9, mk(44, 105, 52)); cv.rect(1, 3, 14, 5, mk(44, 105, 52));
        cv.rect(4, 2, 5, 3, mk(70, 150, 70)); cv.rect(10, 6, 3, 2, mk(30, 80, 42)); cv.rect(5, 7, 4, 1, mk(30, 80, 42)); break;
    default: grass();
    }
    return cache[k] = put(cv);
}
Rectangle shadow() {
    uint64_t k = key(5, 0, 0, 0); auto it = cache.find(k); if (it != cache.end()) return it->second;
    Cv cv(16, 8); cv.rect(3, 2, 10, 4, mk(0, 0, 0, 70)); cv.rect(1, 3, 14, 2, mk(0, 0, 0, 70)); return cache[k] = put(cv);
}
Rectangle icon(int id) {
    uint64_t k = key(6, id, 0, 0); auto it = cache.find(k); if (it != cache.end()) return it->second;
    static const char* S[IC_COUNT] = {
        ".oyyyyo.oyyyyyyooyywyyyoyywyyyooyyyyyyo.oyyyyo.........",
        "", "", "", "", "", "", "", ""};
    (void)S;
    // 8x8, символы: . пусто, k тёмный, y жёлтый, o оранж, r красный, w белый, g серый, b синий, p розовый
    static const char* M[IC_COUNT] = {
        "..kkkk....kyyyyk.kywyyoOkywyyyok.kyyyoOk..kyyok....kkkk..",
        "", "", "", "", "", "", "", ""};
    (void)M;
    static const char* P[IC_COUNT][8] = {
        {"..kkkk..", ".kyyyyk.", "kyywyyok", "kywyyyok", "kyyyyook", "kyyyoook", ".kooook.", "..kkkk.."},
        {"........", ".kk..kk.", "krrkkrrk", "krrrrrrk", "krrrrrrk", ".krrrrk.", "..krrk..", "...kk..."},
        {".....kk.", "....kwwk", "...kwwk.", "..kwwk..", "k.kwk...", ".kkk....", ".kok....", "kk.kk..."},
        {".kkkkkk.", "kbbwwbbk", "kbbwwbbk", "kbbbbbbk", ".kbbbbk.", ".kbbbbk.", "..kbbk..", "...kk..."},
        {"..kkkk..", ".kwwwwk.", "kwwkwwwk", "kwwkwwwk", "kwwkkwwk", "kwwwwwwk", ".kwwwwk.", "..kkkk.."},
        {"..kkkk..", ".kggggk.", ".kg..gk.", ".kg..gk.", "kkkkkkkk", "kggyyggk", "kggyyggk", "kkkkkkkk"},
        {"...kk...", "...kyk..", "kkkkyykk", "kyyyyyyk", ".kyyyyk.", ".kyyyyk.", "kyykkyyk", "kkk..kkk"},
        {"...kk...", "..kyyk..", ".kyyyyk.", "kyyyyyyk", "..kyyk..", "..kyyk..", "..kyyk..", "...kk..."},
        {"...kk...", "..kyyk..", "..kyyk..", "..kyyk..", "kyyyyyyk", ".kyyyyk.", "..kyyk..", "...kk..."}};
    Cv cv(8, 8);
    for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) {
        char c = P[id][y][x]; C col = BLANK;
        switch (c) { case 'k': col = DARK; break; case 'y': col = mk(255, 215, 70); break; case 'o': col = mk(200, 130, 30); break; case 'r': col = mk(230, 60, 70); break;
            case 'w': col = mk(245, 245, 250); break; case 'g': col = mk(150, 150, 165); break; case 'b': col = mk(70, 140, 240); break; }
        cv.px(x, y, col);
    }
    return cache[k] = put(cv);
}
}
