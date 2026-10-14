#include "game/app.h"
#include "gfx/font.h"
#include "gfx/art.h"
#include "audio/sfx.h"
#include "platform/platform.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>

using namespace sim;
int g_sc = 1;
#ifdef CF_NO_RT   // только для автотестов в программном рендерере (там нельзя читать render-текстуру)
#include "rlgl.h"
#define CF_BEGIN BeginDrawing(); ClearBackground(BLACK); rlPushMatrix(); rlScalef((float)scale, (float)scale, 1.f); g_sc = scale;
#define CF_END rlPopMatrix(); EndDrawing();
#else
#define CF_BEGIN BeginTextureMode(rt);
#define CF_END EndTextureMode();
#endif

const char* RACE_RU[12] = {"Люди", "Эльфы", "Орки", "Ночные эльфы", "Природа", "Наги", "Нежить", "Порченые", "Север", "Стихии", "Механизмы", "Хаос"};
// Описания - мои короткие пояснения (не из данных карты).
const char* RACE_DESC[12] = {
    "Универсальная армия: пехота, стрелки, жрецы и осадные машины.", "Меткие стрелки и летающие существа.",
    "Тяжёлые бойцы и всадники на волках и кодо.", "Охотницы, друиды и лесные стражи.",
    "Древние стражи и могучие звери леса.", "Морские твари, змеи и магия воды.",
    "Орды мёртвых: зомби, вампиры и костяные драконы.", "Порченые твари и искажённые стражи.",
    "Ледяные тролли и существа вечной зимы.", "Элементали огня, воды и воздуха.",
    "Машины, големы и осадная техника.", "Демоны и воины хаоса."};
static const char* RACE_SHORT[12] = {"Люди", "Эльфы", "Орки", "Н. эльфы", "Природа", "Наги", "Нежить", "Порченые", "Север", "Стихии", "Механизмы", "Хаос"};
const int UNLOCK_ORDER[10] = {6, 3, 1, 4, 5, 8, 9, 10, 11, 7};

static Color K(int r, int g, int b, int a = 255) { return Color{(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a}; }
static const Color GLD = {240, 195, 90, 255}, INK = {245, 238, 225, 255}, DIM = {160, 150, 180, 255}, OKG = {120, 220, 120, 255}, REDC = {240, 100, 90, 255};
static uint32_t hh(uint32_t a, uint32_t b) { uint32_t h = a * 2654435761u ^ (b + 0x9E3779B9u + (a << 6)); h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; return h; }

// ------------------------------------------------------------------ ядро
void App::init() {
    font::init(); art::init(); sfx::init();
    if (!loadSave(sd)) { sd = SaveData(); }
    sfx::setEnabled(sd.sound, sd.music);
    preRace = sd.lastRace; if (!((sd.unlocked >> preRace) & 1)) preRace = 0;
    layout(); fade = 1.f; cur = nxt = SC_SPLASH;
}
void App::shutdown() { UnloadRenderTexture(rt); sfx::shutdown(); art::shutdown(); font::shutdown(); }

void App::layout() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    int s = std::max(1, sw / 240);
    int w = sw / s, h = sh / s;
    if (!rt.id || w != VW || h != VH || s != scale) {
        if (rt.id) UnloadRenderTexture(rt);
        scale = s; VW = w; VH = h; rt = LoadRenderTexture(VW, VH); SetTextureFilter(rt.texture, TEXTURE_FILTER_POINT);
    }
    TOPI = std::max(8, (int)(sh * 0.035f / scale));
}
void App::readInput(float) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    int offX = (sw - VW * scale) / 2, offY = (sh - VH * scale) / 2;
    int mx = (GetMouseX() - offX) / scale, my = (GetMouseY() - offY) / scale;
    bool d = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    Input n; n.x = mx; n.y = my; n.down = d; n.pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT); n.released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    if (fakeTap) { n.x = fx; n.y = fy; n.pressed = n.released = true; n.down = false; fakeTap = false; }
    n.dx = n.x - in.x; n.dy = n.y - in.y;
    if (n.pressed) { n.dx = n.dy = 0; n.dragTotal = 0; } else n.dragTotal = in.dragTotal + (d ? std::abs(n.dx) + std::abs(n.dy) : 0);
    if (!d && !n.released) n.dragTotal = 0;
    n.back = IsKeyPressed(KEY_BACK) || IsKeyPressed(KEY_ESCAPE);
    in = n;
}
void App::go(int s) { if (s == cur && fade <= 0) return; msgT = 0; nxt = s; if (fade < 0) fade = 0; }
void App::testStart(int a, int b, int m, int l) { startMatch(a, b, m, l, -1); fade = 0; cur = nxt = SC_GAME; }

void App::frame() {
    float dt = std::min(0.1f, GetFrameTime()); tm += dt; dtNow = dt;
    layout(); readInput(dt); sfx::update();
    if (quitArm > 0) quitArm -= dt;
    if (cur != nxt) { fade += dt * 5.f; if (fade >= 1.f) { cur = nxt; pressId = 0; scrollY = 0; dragging = false; menuT = 0; if (cur == SC_GAME) { } } }
    else if (fade > 0) fade = std::max(0.f, fade - dt * 4.f);
    if (msgT > 0) msgT -= dt;

    CF_BEGIN
    ClearBackground(K(14, 10, 30));
    switch (cur) {
        case SC_SPLASH: drawSplash(dt); break; case SC_MENU: drawMenu(); break; case SC_PRE: drawPre(); break; case SC_CAMP: drawCamp(); break;
        case SC_RACES: drawRaces(); break; case SC_RACE: drawRace(); break; case SC_SET: drawSettings(); break; case SC_HELP: drawHelp(); break;
        case SC_GAME: { stepSim(); drawGame((float)(acc / 0.05)); gameUI(); } break;
        case SC_RESULT: drawResult(); break;
    }
    if (msgT > 0 && cur != SC_GAME) { int w = font::width(msg) + 12; panel((VW - w) / 2, VH - 48, w, 16, K(30, 22, 50), GLD); font::drawCenter(msg, VW / 2, VH - 44, INK); }
    if (fade > 0) DrawRectangle(0, 0, VW, VH, K(8, 6, 16, (int)(std::min(1.f, fade) * 255)));
    CF_END
#ifndef CF_NO_RT
    BeginDrawing(); ClearBackground(BLACK);
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawTexturePro(rt.texture, {0, 0, (float)VW, -(float)VH}, {(float)((sw - VW * scale) / 2), (float)((sh - VH * scale) / 2), (float)(VW * scale), (float)(VH * scale)}, {0, 0}, 0, WHITE);
    EndDrawing();
#endif
    if (in.released) pressId = 0;
}
Image App::snapshot() { return LoadImageFromTexture(rt.texture); }

// ------------------------------------------------------------------ UI
void App::panel(int x, int y, int w, int h, Color fill, Color border) {
    DrawRectangle(x, y, w, h, fill);
    DrawRectangle(x + 1, y + 1, w - 2, 1, K(255, 255, 255, 28)); DrawRectangle(x + 1, y + h - 2, w - 2, 1, K(0, 0, 0, 60));
    DrawRectangle(x, y, w, 1, border); DrawRectangle(x, y + h - 1, w, 1, border); DrawRectangle(x, y, 1, h, border); DrawRectangle(x + w - 1, y, 1, h, border);
    DrawRectangle(x, y, 1, 1, K(0, 0, 0, 0)); 
}
bool App::button(int x, int y, int w, int h, const char* t, int style, bool enabled) {
    int id = x * 7919 + y * 104729 + w * 31 + 1;
    bool inside = hit(x, y, w, h);
    if (enabled && in.pressed && inside) pressId = id;
    bool down = enabled && pressId == id && in.down && inside;
    Color fill, border = GLD, text = INK;
    switch (style) { case 1: fill = K(160, 110, 30); border = K(255, 225, 130); break; case 2: fill = K(130, 40, 50); border = K(255, 140, 130); break;
        case 3: fill = K(40, 110, 60); border = K(150, 255, 160); break; default: fill = K(66, 52, 104); break; }
    if (!enabled) { fill = K(44, 40, 58); border = K(90, 84, 108); text = K(120, 112, 140); }
    else if (down) fill = K(fill.r + 36, fill.g + 36, fill.b + 36);
    int oy = down ? 1 : 0;
    panel(x, y + oy, w, h - oy, fill, border);
    font::drawCenter(t, x + w / 2, y + (h - 7) / 2 + oy, text, enabled);
    if (enabled && in.released && pressId == id && inside && in.dragTotal <= 6) { sfx::play(sfx::S_TAP); pressId = 0; return true; }
    return false;
}
void App::icon(int id, int x, int y, Color tint) { DrawTextureRec(art::tex(), art::icon(id), {(float)x, (float)y}, tint); }
void App::sprite(Rectangle r, int x, int y, Color tint, int s) { DrawTexturePro(art::tex(), r, {(float)x, (float)y, r.width * s, r.height * s}, {0, 0}, 0, tint); }
float App::scrollList(float s, int x, int y, int w, int h, float contentH) {
    if (in.pressed && hit(x, y, w, h)) dragging = true;
    if (dragging && in.down) s -= in.dy;
    if (in.released) dragging = false;
    float mx = std::max(0.f, contentH - h); if (s < 0) s = 0; if (s > mx) s = mx; return s;
}
void App::title(const char* t, int y) {
    int w = font::width(t) * 2;
    font::drawScaled(t, (VW - w) / 2 + 1, y + 2, K(10, 6, 20), 2); font::drawScaled(t, (VW - w) / 2, y, GLD, 2);
}

static void bfly(int x, int y, int fr, Color c) {
    if (fr) { DrawRectangle(x - 2, y - 1, 2, 2, c); DrawRectangle(x, y - 1, 2, 2, c); DrawRectangle(x - 1, y + 1, 1, 1, K(255, 255, 255, 200)); }
    else { DrawRectangle(x - 1, y, 1, 2, c); DrawRectangle(x, y, 1, 2, c); }
    DrawRectangle(x - 1, y, 1, 1, K(30, 20, 40)); 
}
void App::backdrop(float t, bool withCastle) {
    int hor = (int)(VH * (withCastle ? 0.52f : 0.66f));
    for (int y = 0; y < hor; y += 3) {
        float k = (float)y / hor; DrawRectangle(0, y, VW, 3, K((int)(14 + 70 * k * k), (int)(10 + 34 * k * k), (int)(38 + 60 * k)));
    }
    for (int i = 0; i < 46; i++) {
        uint32_t r = hh(i, 99); int x = r % VW, y = (r >> 10) % (hor - 30);
        int tw = (int)(t * 2 + i) % 5; DrawRectangle(x, y, 1, 1, K(255, 255, 255, tw == 0 ? 90 : 200));
    }
    int mx = VW * 3 / 4, my = TOPI + 46;
    DrawCircle(mx, my, 15, K(255, 245, 200, 40)); DrawCircle(mx, my, 11, K(250, 240, 200)); DrawCircle(mx - 4, my - 2, 3, K(225, 215, 175)); DrawCircle(mx + 3, my + 4, 2, K(225, 215, 175));
    for (int layer = 0; layer < 3; layer++) {
        Color c = layer == 0 ? K(40, 30, 70) : (layer == 1 ? K(30, 24, 56) : K(20, 28, 36));
        for (int x = 0; x < VW; x++) {
            float off = t * (2 + layer * 3);
            int h = (int)(26 + layer * 26 + 10 * sinf((x + off * 0.2f * layer) * 0.04f + layer) + 6 * sinf((x + layer * 40) * 0.11f));
            int y = hor + layer * 18 - h + 24; if (layer == 0) y = hor - h + 14;
            DrawRectangle(x, y, 1, VH - y, c);
        }
    }
    if (withCastle) {
        sprite(art::castle(0, 0), VW / 2 - 80, hor - 92, K(120, 108, 150), 2);
        for (int i = 0; i < 3; i++) { if (((int)(t * 1.5f) + i) % 3) DrawRectangle(VW / 2 - 56 + i * 56, hor - 52, 3, 5, K(255, 210, 100)); }
    }
    for (int i = 0; i < 3; i++) {                       // туман
        float x = fmodf(t * (6 + i * 4) + i * 90, (float)(VW + 120)) - 120;
        DrawRectangle((int)x, hor - 6 + i * 22, 120, 6, K(180, 170, 210, 28)); DrawRectangle((int)x + 20, hor - 9 + i * 22, 80, 3, K(180, 170, 210, 20));
    }
    for (int i = 0; i < 3; i++) {                       // летучие мыши
        float x = fmodf(t * (22 + i * 9) + i * 70, (float)(VW + 40)) - 20, y = TOPI + 18 + i * 20 + 6 * sinf(t * 2 + i);
        int f = (int)(t * 8 + i) & 1; Color c = K(10, 6, 20);
        DrawRectangle((int)x - 1, (int)y, 3, 2, c); if (f) { DrawRectangle((int)x - 4, (int)y - 1, 3, 1, c); DrawRectangle((int)x + 2, (int)y - 1, 3, 1, c); }
        else { DrawRectangle((int)x - 4, (int)y + 1, 3, 1, c); DrawRectangle((int)x + 2, (int)y + 1, 3, 1, c); }
    }
    for (int i = 0; i < 16; i++) {                      // сверкающие бабочки
        float bx = (hh(i, 5) % VW) + 14 * sinf(t * 0.7f + i), by = hor + 6 + (hh(i, 9) % (VH - hor - 20)) * 0.9f + 8 * sinf(t * 1.3f + i * 2);
        float hue = (float)((hh(i, 3) % 4) * 60 + 200); Color c = ColorFromHSV(hue, 0.5f, 1.f);
        bfly((int)bx, (int)by, ((int)(t * 6 + i)) & 1, c);
        if (((int)(t * 3) + i) % 6 == 0) DrawRectangle((int)bx + 3, (int)by - 3, 1, 1, K(255, 255, 255));
    }
}

// ------------------------------------------------------------------ экраны
void App::drawSplash(float) {
    backdrop(tm, true);
    title("БИТВА ЗАМКОВ", VH / 3 - 6);
    font::drawCenter("ЗАГРУЗКА...", VW / 2, VH - 40, DIM);
    int w = 100, x = (VW - w) / 2; DrawRectangle(x, VH - 28, w, 5, K(30, 22, 50)); DrawRectangle(x, VH - 28, (int)(w * std::min(1.f, tm / 1.4f)), 5, GLD);
    if (tm > 1.5f && cur == nxt) go(SC_MENU);
}
void App::drawMenu() {
    menuT++;
    backdrop(tm, true);
    title("БИТВА ЗАМКОВ", TOPI + 8);
    font::drawCenter("12 рас. 3 карты. Один замок.", VW / 2, TOPI + 28, DIM);
    int bw = 170, bh = 24, gap = 6, y = (int)(VH * 0.56f);
    const char* names[5] = {"Кампания", "Быстрая игра", "Расы", "Настройки", "Как играть"};
    int tgt[5] = {SC_CAMP, SC_PRE, SC_RACES, SC_SET, SC_HELP};
    for (int i = 0; i < 5; i++) {
        float k = std::min(1.f, std::max(0.f, (menuT - i * 5) / 14.f)); k = 1 - (1 - k) * (1 - k);
        int x = (VW - bw) / 2 - (int)((1 - k) * VW);
        if (button(x, y + i * (bh + gap), bw, bh, names[i], i == 0 ? 1 : 0) && cur == nxt) {
            if (tgt[i] == SC_PRE) { preMission = -1; }
            go(tgt[i]);
        }
    }
    char b[48]; snprintf(b, sizeof b, "ПОБЕД: %d   ПОРАЖЕНИЙ: %d", sd.wins, sd.losses);
    font::drawCenter(b, VW / 2, VH - 16, DIM);
    if (in.back) { if (quitArm > 0) quit = true; else { quitArm = 2.f; showMsg("Нажмите ещё раз для выхода"); } }
}


void App::drawPre() {
    backdrop(tm * 0.3f, false); DrawRectangle(0, 0, VW, VH, K(14, 10, 30, 190));
    bool camp = preMission >= 0;
    char b[64];
    if (camp) snprintf(b, sizeof b, "МИССИЯ %d", preMission + 1); else snprintf(b, sizeof b, "БЫСТРАЯ ИГРА");
    title(b, TOPI + 4);
    font::draw("ВАША РАСА", 8, TOPI + 26, GLD);
    int cols = 3, tw = (VW - 16 - 2 * 4) / cols, th = 24, gy = TOPI + 38;
    for (int r = 0; r < 12; r++) {
        int x = 8 + (r % cols) * (tw + 4), y = gy + (r / cols) * (th + 4);
        bool locked = !((sd.unlocked >> r) & 1), sel = r == preRace;
        Color fill = sel ? K(90, 70, 140) : K(50, 40, 80), bord = sel ? K(255, 225, 130) : K(110, 96, 150);
        if (locked) { fill = K(36, 32, 48); bord = K(70, 66, 86); }
        panel(x, y, tw, th, fill, bord);
        Color rc = art::raceColor(r); if (locked) rc = K(70, 66, 86);
        DrawRectangle(x + 2, y + 2, 3, th - 4, rc);
        char nm[24]; snprintf(nm, sizeof nm, "%s", RACE_SHORT[r]);
        while (font::width(nm) > tw - 22 && strlen(nm) > 3) nm[strlen(nm) - 1] = 0;
        font::draw(nm, x + 8, y + 8, locked ? K(110, 104, 130) : INK);
        if (locked) icon(art::IC_LOCK, x + tw - 10, y + 8, K(200, 200, 215));
        if (hit(x, y, tw, th) && in.released && in.dragTotal <= 6) { if (locked) { sfx::play(sfx::S_ERROR); showMsg("Раса откроется в кампании"); } else { preRace = r; sfx::play(sfx::S_TAP); } }
    }
    int y = gy + 4 * (th + 4) + 2;
    font::wrap(RACE_DESC[preRace], VW - 16, 8, y, DIM, 9); y += 24;
    if (!camp) {
        char t[48];
        snprintf(t, sizeof t, "ВРАГ: %s", preRandFoe ? "Случайно" : RACE_RU[preFoe]);
        if (button(8, y, VW - 16, 20, t)) { if (preRandFoe) { preRandFoe = false; preFoe = 0; } else if (++preFoe >= 12) { preFoe = 0; preRandFoe = true; } }
        y += 24; snprintf(t, sizeof t, "КАРТА: %s", mapName(preMap)); if (button(8, y, VW - 16, 20, t)) preMap = (preMap + 1) % MAP_COUNT;
        y += 24; snprintf(t, sizeof t, "БОТ: %s", ai::LEVEL_NAMES[preLevel]); if (button(8, y, VW - 16, 20, t)) preLevel = (preLevel + 1) % 5;
    } else {
        snprintf(b, sizeof b, "ВРАГ: %s", RACE_RU[UNLOCK_ORDER[preMission]]); font::draw(b, 8, y, INK); y += 11;
        snprintf(b, sizeof b, "КАРТА: %s   БОТ: %s", mapName(preMission % MAP_COUNT), ai::LEVEL_NAMES[std::min(4, preMission / 2)]); font::draw(b, 8, y, INK); y += 11;
        snprintf(b, sizeof b, "НАГРАДА: открыть расу %s", RACE_RU[UNLOCK_ORDER[preMission]]); font::draw(b, 8, y, GLD);
    }
    if (button(8, VH - 30, 70, 22, "Назад", 0) || in.back) go(camp ? SC_CAMP : SC_MENU);
    if (button(84, VH - 30, VW - 92, 22, "В БОЙ!", 1) && cur == nxt) {
        sd.lastRace = preRace; writeSave(sd);
        if (camp) startMatch(preRace, UNLOCK_ORDER[preMission], preMission % MAP_COUNT, std::min(4, preMission / 2), preMission);
        else { int fr = preRandFoe ? (int)(GetTime() * 1000) % 12 : preFoe; startMatch(preRace, fr, preMap, preLevel, -1); }
    }
}

void App::drawCamp() {
    backdrop(tm * 0.3f, false); DrawRectangle(0, 0, VW, VH, K(14, 10, 30, 190));
    title("КАМПАНИЯ", TOPI + 4);
    int top = TOPI + 28, h = VH - top - 36, rowH = 38;
    scrollY = scrollList(scrollY, 0, top, VW, h, 10.f * rowH);
    scis(0, top, VW, h);
    for (int i = 0; i < 10; i++) {
        int y = top + i * rowH - (int)scrollY; bool done = i < sd.campaign, open = i <= sd.campaign;
        panel(8, y, VW - 16, rowH - 4, open ? K(52, 42, 86) : K(34, 30, 46), done ? K(130, 220, 130) : (open ? GLD : K(70, 66, 86)));
        char b[64]; snprintf(b, sizeof b, "%d. ВРАГ: %s", i + 1, RACE_RU[UNLOCK_ORDER[i]]); font::draw(b, 16, y + 5, open ? INK : DIM);
        snprintf(b, sizeof b, "%s, бот: %s", mapName(i % MAP_COUNT), ai::LEVEL_NAMES[std::min(4, i / 2)]); font::draw(b, 16, y + 16, DIM);
        snprintf(b, sizeof b, "Награда: %s", RACE_RU[UNLOCK_ORDER[i]]); font::draw(b, 16, y + 25, GLD);
        if (done) font::draw("ПРОЙДЕНО", VW - 66, y + 5, OKG); else if (!open) icon(art::IC_LOCK, VW - 26, y + 6, K(160, 160, 180));
        if (open && hit(8, y, VW - 16, rowH - 4) && y > top - 4 && in.released && in.dragTotal <= 6 && cur == nxt) { sfx::play(sfx::S_TAP); preMission = i; go(SC_PRE); }
    }
    EndScissorMode();
    if (button(8, VH - 30, 70, 22, "Назад") || in.back) go(SC_MENU);
}

void App::drawRaces() {
    backdrop(tm * 0.3f, false); DrawRectangle(0, 0, VW, VH, K(14, 10, 30, 190));
    title("РАСЫ", TOPI + 4);
    int top = TOPI + 28, h = VH - top - 36, rowH = 34;
    scrollY = scrollList(scrollY, 0, top, VW, h, 12.f * rowH);
    scis(0, top, VW, h);
    for (int r = 0; r < 12; r++) {
        int y = top + r * rowH - (int)scrollY; bool locked = !((sd.unlocked >> r) & 1);
        panel(8, y, VW - 16, rowH - 4, locked ? K(34, 30, 46) : K(52, 42, 86), locked ? K(70, 66, 86) : art::raceColor(r));
        const RaceDef& R = race(r);
        sprite(art::unit(r, 0, 0, 0), 14, y + 6, locked ? K(90, 90, 100) : WHITE, 1);
        if (art::unitSize(r, 0) == 32) {}
        font::draw(RACE_RU[r], 36, y + 6, locked ? DIM : INK);
        char b[48]; snprintf(b, sizeof b, "ЗДАНИЙ: %d", R.nb); font::draw(b, 36, y + 17, DIM);
        if (locked) icon(art::IC_LOCK, VW - 26, y + 10, K(160, 160, 180));
        if (hit(8, y, VW - 16, rowH - 4) && y > top - 4 && in.released && in.dragTotal <= 6 && cur == nxt) { sfx::play(sfx::S_TAP); raceSel = r; go(SC_RACE); }
    }
    EndScissorMode();
    if (button(8, VH - 30, 70, 22, "Назад") || in.back) go(SC_MENU);
}

static const char* ATK_SHORT[6] = {"НОРМ", "ПРОН", "ОСАД", "МАГ", "ХАОС", "ГЕРОЙ"};
static const char* ARM_SHORT[7] = {"ЛЁГК", "СРЕД", "ТЯЖ", "КРЕП", "ГЕР", "БЕЗ", "БОЖ"};

void App::drawRace() {
    backdrop(tm * 0.3f, false); DrawRectangle(0, 0, VW, VH, K(14, 10, 30, 200));
    const RaceDef& R = race(raceSel); bool locked = !((sd.unlocked >> raceSel) & 1);
    title(RACE_RU[raceSel], TOPI + 2);
    int top = TOPI + 24, h = VH - top - 34, rowH = 52;
    scrollY = scrollList(scrollY, 0, top, VW, h, (float)(R.nb * rowH + 24));
    scis(0, top, VW, h);
    int y0 = top - (int)scrollY;
    int hd = font::wrap(RACE_DESC[raceSel], VW - 16, 8, y0 + 2, DIM, 9);
    if (locked) font::draw("ЗАКРЫТО: ПОБЕДА В КАМПАНИИ", 8, y0 + 2 + hd, REDC);
    for (int i = 0; i < R.nb; i++) {
        int y = y0 + 20 + i * rowH; const BuildDef& b = R.b[i]; const UnitDef& u = R.u[i];
        panel(6, y, VW - 12, rowH - 3, K(46, 38, 74), K(100, 88, 140));
        sprite(art::building(raceSel, i, 0), 10, y + 3, WHITE, 1);
        int bs = std::min(b.w * 16, 48); (void)bs;
        int ux = 10 + b.w * 16 + 4;
        sprite(art::unit(raceSel, i, 0, 0), ux, y + 3, WHITE, 1);
        int tx = ux + art::unitSize(raceSel, i) + 6;
        char l[96]; snprintf(l, sizeof l, "%s / %s", b.name, u.name);
        while (font::width(l) > VW - tx - 10 && strlen(l) > 4) l[strlen(l) - 1] = 0;
        font::draw(l, tx, y + 4, INK);
        snprintf(l, sizeof l, "ЦЕНА %d  ЗДР %d  УРОН %d", b.cost, u.hp, u.dmg); font::draw(l, tx, y + 14, GLD);
        snprintf(l, sizeof l, "%s / %s %d", ATK_SHORT[u.atk], ARM_SHORT[u.arm], u.armor); font::draw(l, tx, y + 24, DIM);
        snprintf(l, sizeof l, "ДАЛЬН %d  СКОР %d  СПАВН %ds", u.range, u.speed, b.spawn / 20); font::draw(l, tx, y + 34, DIM);
        if (u.desc && u.desc[0]) { char d[64]; snprintf(d, sizeof d, "%s", u.desc); while (font::width(d) > VW - tx - 8 && strlen(d) > 3) d[strlen(d) - 1] = 0; font::draw(d, tx, y + 43, K(150, 210, 255)); }
    }
    EndScissorMode();
    if (button(8, VH - 30, 70, 22, "Назад") || in.back) go(SC_RACES);
}

void App::drawSettings() {
    backdrop(tm * 0.3f, false); DrawRectangle(0, 0, VW, VH, K(14, 10, 30, 190));
    title("НАСТРОЙКИ", TOPI + 4);
    int y = TOPI + 40, w = VW - 24;
    char b[40];
    snprintf(b, sizeof b, "ВИБРАЦИЯ: %s", sd.vibration ? "ВКЛ" : "ВЫКЛ"); if (button(12, y, w, 24, b, sd.vibration ? 3 : 0)) { sd.vibration ^= 1; writeSave(sd); if (sd.vibration) plat::vibrate(60); }
    y += 30; snprintf(b, sizeof b, "ЗВУК: %s", sd.sound ? "ВКЛ" : "ВЫКЛ"); if (button(12, y, w, 24, b, sd.sound ? 3 : 0)) { sd.sound ^= 1; sfx::setEnabled(sd.sound, sd.music); writeSave(sd); }
    y += 30; snprintf(b, sizeof b, "МУЗЫКА: %s", sd.music ? "ВКЛ" : "ВЫКЛ"); if (button(12, y, w, 24, b, sd.music ? 3 : 0)) { sd.music ^= 1; sfx::setEnabled(sd.sound, sd.music); writeSave(sd); }
    y += 40;
    font::wrap("Вибрация срабатывает при ударах по вашему замку, при победе и поражении.", VW - 24, 12, y, DIM, 9); y += 32;
    if (button(12, y, w, 24, resetArm ? "ТОЧНО СБРОСИТЬ? НАЖМИ ЕЩЁ РАЗ" : "Сбросить прогресс", 2)) {
        if (resetArm) { SaveData n; n.vibration = sd.vibration; n.sound = sd.sound; n.music = sd.music; sd = n; writeSave(sd); resetArm = 0; showMsg("Прогресс сброшен"); } else resetArm = 1;
    }
    if (button(8, VH - 30, 70, 22, "Назад") || in.back) { resetArm = 0; go(SC_MENU); }
}

void App::drawHelp() {
    backdrop(tm * 0.3f, false); DrawRectangle(0, 0, VW, VH, K(14, 10, 30, 200));
    title("КАК ИГРАТЬ", TOPI + 4);
    int y = TOPI + 30; int W = VW - 16;
    if (helpPage == 0) {
        y += font::wrap("ЦЕЛЬ: разрушить замок врага. Ваш замок внизу, вражеский вверху.", W, 8, y, INK) + 6;
        y += font::wrap("Вы не управляете юнитами. Стройте здания в своей зоне: каждое само создаёт бойцов, и они идут на врага.", W, 8, y, INK) + 6;
        y += font::wrap("СТРОЙКА: выберите здание внизу, коснитесь клетки на карте, нажмите «Построить». Карту двигайте пальцем вверх и вниз.", W, 8, y, INK) + 6;
        y += font::wrap("Часть зданий требует другого: они открываются после постройки нужного здания.", W, 8, y, DIM);
    } else if (helpPage == 1) {
        y += font::wrap("ДОХОД приходит раз в 10 секунд: базовые 5 золота плюс вклад каждого вашего здания.", W, 8, y, INK) + 6;
        y += font::wrap("Первые четыре здания дают бонус золота: 20, 15, 10 и 5.", W, 8, y, INK) + 6;
        y += font::wrap("Золото также дают за убийство врагов и разрушение зданий.", W, 8, y, INK) + 6;
        y += font::wrap("ПРОКАЧКА: оружие, броня, живучесть, казна (+25% дохода), скорость создания и стены замка.", W, 8, y, INK) + 6;
        y += font::wrap("Если никто не побеждает 9 минут, слабейший замок начинает рушиться.", W, 8, y, DIM);
    } else {
        font::draw("УРОН В % (АТАКА / БРОНЯ)", 8, y, GLD); y += 12;
        int cw = (VW - 16 - 32) / 7;
        for (int c = 0; c < 7; c++) font::draw(ARM_SHORT[c], 8 + 32 + c * cw, y, DIM);
        y += 10;
        for (int a = 0; a < 6; a++) {
            font::draw(ATK_SHORT[a], 8, y + 2, DIM);
            for (int c = 0; c < 7; c++) {
                int v = DMG_MULT[a][c]; Color col = v >= 150 ? K(60, 150, 70) : (v >= 100 ? K(70, 90, 140) : (v >= 70 ? K(140, 110, 50) : K(150, 55, 55)));
                DrawRectangle(8 + 32 + c * cw, y, cw - 2, 12, col); char t[8]; snprintf(t, sizeof t, "%d", v); font::drawCenter(t, 8 + 32 + c * cw + (cw - 2) / 2, y + 2, INK, false);
            }
            y += 14;
        }
        y += 6; font::wrap("Подбирайте юнитов под тип брони врага. Броня числом снижает урон: 1/(1+0,06*брони).", W, 8, y, DIM);
    }
    char b[16]; snprintf(b, sizeof b, "%d / 3", helpPage + 1); font::drawCenter(b, VW / 2, VH - 24, DIM);
    if (button(VW - 78, VH - 30, 70, 22, helpPage < 2 ? "Далее" : "Готово", 1)) { if (helpPage < 2) helpPage++; else { helpPage = 0; go(SC_MENU); } }
    if (button(8, VH - 30, 70, 22, "Назад") || in.back) { if (helpPage > 0) helpPage--; else go(SC_MENU); }
}

void App::drawResult() {
    backdrop(tm * 0.5f, false); DrawRectangle(0, 0, VW, VH, K(14, 10, 30, 160));
    int y = TOPI + 30;
    if (won) { title("ПОБЕДА!", y); } else { Color c = REDC; int w = font::width("ПОРАЖЕНИЕ") * 2; font::drawScaled("ПОРАЖЕНИЕ", (VW - w) / 2 + 1, y + 2, K(10, 6, 20), 2); font::drawScaled("ПОРАЖЕНИЕ", (VW - w) / 2, y, c, 2); }
    y += 36; char b[64]; int secs = world.tick / 20;
    panel(20, y, VW - 40, 74, K(40, 32, 70), GLD);
    snprintf(b, sizeof b, "ВРЕМЯ: %d:%02d", secs / 60, secs % 60); font::draw(b, 30, y + 8, INK);
    snprintf(b, sizeof b, "УБИТО ВРАГОВ: %d", world.pl[0].kills); font::draw(b, 30, y + 22, INK);
    snprintf(b, sizeof b, "ЗОЛОТА ЗАРАБОТАНО: %d", world.pl[0].earned); font::draw(b, 30, y + 36, INK);
    snprintf(b, sizeof b, "ПОСТРОЕНО ЗДАНИЙ: %d", world.pl[0].built); font::draw(b, 30, y + 50, INK);
    y += 84;
    if (resultUnlock >= 0) { icon(art::IC_STAR, VW / 2 - 4, y, GLD); font::drawCenter("НОВАЯ РАСА ОТКРЫТА!", VW / 2, y + 12, GLD); snprintf(b, sizeof b, "%s", RACE_RU[resultUnlock]); font::drawCenter(b, VW / 2, y + 24, INK); }
    if (button(20, VH - 64, VW - 40, 24, "Ещё раз", 1) && cur == nxt) startMatch(myRace, foeRace, mapId, level, mission);
    if (button(20, VH - 34, VW - 40, 24, "В меню") || in.back) go(mission >= 0 ? SC_CAMP : SC_MENU);
}
