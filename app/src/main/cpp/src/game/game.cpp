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

static Color K(int r, int g, int b, int a = 255) { return Color{(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a}; }
static const Color GLD = {240, 195, 90, 255}, INK = {245, 238, 225, 255}, DIM = {160, 150, 180, 255}, REDC = {240, 100, 90, 255};
static uint32_t hh(uint32_t a, uint32_t b) { uint32_t h = a * 2654435761u ^ (b + 0x9E3779B9u + (a << 6)); h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; return h; }
static const int PANEL_H = 112;
static const char* ATK_SHORT[6] = {"НОРМ", "ПРОН", "ОСАД", "МАГ", "ХАОС", "ГЕРОЙ"};
static const char* ARM_SHORT[7] = {"ЛЁГК", "СРЕД", "ТЯЖ", "КРЕП", "ГЕР", "БЕЗ", "БОЖ"};

void App::vib(int ms) { if (sd.vibration) plat::vibrate(ms); }

void App::startMatch(int mr, int fr, int map, int lvl, int mis) {
    myRace = mr; foeRace = fr; mapId = map; level = lvl; mission = mis;
    unsigned seed = (unsigned)(GetTime() * 1000) + 7;
    world.init(map, mr, fr, seed);
    bot = ai::Bot(1, lvl, seed);
    acc = 0; speed = 1; paused = pauseMenu = false; tab = 0; selBuild = -1; ghostX = ghostY = -1; selPlaced = 0; tickCount = 0;
    shake = flash = 0; endTimer = -1; won = false; parts.clear(); projs.clear(); ftx.clear(); prev.clear(); resultUnlock = -1; vibGap = 0;
    camY = 1e6f; mapDrag = false;
    go(SC_GAME);
}

void App::spark(float x, float y, int n, Color c, float spd) {
    for (int i = 0; i < n && parts.size() < 500; i++) {
        float a = GetRandomValue(0, 628) / 100.f, s = spd * GetRandomValue(30, 100) / 100.f;
        parts.push_back({x, y, cosf(a) * s, sinf(a) * s - 6, 0.35f + GetRandomValue(0, 30) / 100.f, c});
    }
}

void App::stepSim() {
    float dt = dtNow;
    if (!pauseMenu && world.winner < 0) {
        acc += dt * speed; int n = 0;
        while (acc >= 0.05 && n < 8) {
            acc -= 0.05; n++;
            prev.clear(); for (auto& u : world.units) prev[u.id] = {u.x, u.y};
            cmds.clear(); bot.think(world, cmds); for (auto& c : cmds) world.submit(c);
            world.step(); handleEvents();
        }
    }
    if (world.winner >= 0 && endTimer < 0) endTimer = 2.2f;
    if (endTimer >= 0) { endTimer -= dt; if (endTimer <= 0 && cur == SC_GAME && cur == nxt) finishMatch(); }
    shake = std::max(0.f, shake - dt); flash = std::max(0.f, flash - dt * 2); vibGap = std::max(0.f, vibGap - dt);
    for (auto& p : parts) { p.x += p.vx * dt; p.y += p.vy * dt; p.vy += 60 * dt; p.life -= dt; }
    parts.erase(std::remove_if(parts.begin(), parts.end(), [](const Particle& p) { return p.life <= 0; }), parts.end());
    for (auto& p : projs) p.t += dt * 7.f;
    projs.erase(std::remove_if(projs.begin(), projs.end(), [](const Proj& p) { return p.t >= 1; }), projs.end());
    for (auto& f : ftx) { f.y -= 14 * dt; f.life -= dt; }
    ftx.erase(std::remove_if(ftx.begin(), ftx.end(), [](const FTxt& f) { return f.life <= 0; }), ftx.end());
}

void App::handleEvents() {
    static const Color ATKC[6] = {{255, 230, 120, 255}, {240, 240, 240, 255}, {255, 150, 60, 255}, {110, 210, 255, 255}, {230, 80, 230, 255}, {255, 210, 90, 255}};
    for (auto& e : world.events) {
        float px = (float)(e.x >> 4), py = (float)(e.y >> 4);
        switch (e.type) {
            case EV_SHOT: projs.push_back({(float)(e.x >> 4), (float)(e.y >> 4), (float)(e.x2 >> 4), (float)(e.y2 >> 4), 0, ATKC[e.aux % 6]}); sfx::play(sfx::S_SHOT); break;
            case EV_HIT: spark(px, py - 4, 2, e.owner == 0 ? K(120, 180, 255) : K(255, 130, 110), 22); sfx::play(sfx::S_HIT); break;
            case EV_DEATH: spark(px, py - 4, 9, e.owner == 0 ? K(90, 140, 230) : K(220, 70, 60), 38); sfx::play(sfx::S_DEATH); break;
            case EV_BOUNTY: if (e.owner == 0) { if (ftx.size() < 8) { FTxt f{}; f.x = px; f.y = py - 10; f.life = 0.9f; f.c = GLD; snprintf(f.s, sizeof f.s, "+%d", e.val); ftx.push_back(f); } sfx::play(sfx::S_COIN); } break;
            case EV_BUILD: spark(px, py, 10, K(200, 190, 170), 30); if (e.owner == 0) sfx::play(sfx::S_BUILD); break;
            case EV_SELL: spark(px, py, 8, K(255, 215, 90), 30); sfx::play(sfx::S_SELL); break;
            case EV_BDEATH: spark(px, py, 30, K(160, 150, 140), 55); shake = std::max(shake, 0.12f); sfx::play(sfx::S_CASTLE); break;
            case EV_HEAL: spark((float)(e.x2 >> 4), (float)(e.y2 >> 4) - 6, 3, K(120, 255, 140), 14); break;
            case EV_INCOME: if (e.owner == 0) { sfx::play(sfx::S_COIN); } break;
            case EV_UPGRADE: if (e.owner == 0) sfx::play(sfx::S_UPGRADE); break;
            case EV_CASTLE_HIT:
                if (e.aux == 1) break;
                if (e.owner == 0) { flash = 0.4f; shake = std::max(shake, 0.2f); if (vibGap <= 0) { vib(40); vibGap = 0.4f; } sfx::play(sfx::S_CASTLE); }
                else { shake = std::max(shake, 0.06f); }
                spark(px, py, 3, K(255, 200, 120), 40); break;
            case EV_END:
                if (e.val == 0) { vib(160); sfx::play(sfx::S_WIN); } else { vib(600); sfx::play(sfx::S_LOSE); }
                break;
        }
    }
    world.events.clear();
}

void App::finishMatch() {
    won = world.winner == 0;
    resultUnlock = -1;
    if (won) {
        sd.wins++;
        if (mission >= 0) {
            if (mission == sd.campaign) sd.campaign++;
            int r = UNLOCK_ORDER[mission];
            if (!((sd.unlocked >> r) & 1)) { sd.unlocked |= 1u << r; resultUnlock = r; }
        }
    } else sd.losses++;
    writeSave(sd);
    go(SC_RESULT);
}

void App::clampCam() {
    float view = (float)(mapBot - mapTop), mx = std::max(0.f, (float)(MAP_H * 16) - view);
    if (camY < 0) camY = 0; if (camY > mx) camY = mx;
}

bool App::placeable(int cx, int cy) const { return selBuild >= 0 && world.canBuild(0, selBuild, cx, cy); }
void App::findFree(int def) {
    const BuildDef& b = world.raceOf(0).b[def];
    int zTop = MAP_H - 12, zBot = MAP_H - 4;
    for (int y = zTop; y + b.h - 1 <= zBot; y++) for (int k = 0; k < MAP_W; k++) {
        int x = MAP_W / 2 - b.w / 2 + ((k & 1) ? (k + 1) / 2 : -(k / 2));
        if (x < 0 || x + b.w > MAP_W) continue;
        if (world.canBuild(0, def, x, y)) { ghostX = x; ghostY = y; return; }
    }
    ghostX = (MAP_W - b.w) / 2; ghostY = zTop + 2;
}

// ------------------------------------------------------------------ рендер карты
void App::drawGame(float alpha) {
    mapX0 = (VW - 240) / 2; mapTop = TOPI + 34; mapBot = VH - PANEL_H; clampCam();
    int viewH = mapBot - mapTop;
    DrawRectangle(0, mapTop, VW, viewH, K(34, 58, 38));
    scis(0, mapTop, VW, viewH);
    int shx = 0, shy = 0; if (shake > 0.02f) { shx = GetRandomValue(-2, 2); shy = GetRandomValue(-2, 2); }
    int cy0 = std::max(0, (int)camY / 16), cy1 = std::min(MAP_H - 1, ((int)camY + viewH) / 16);
    Texture2D T = art::tex();
    for (int r = cy0; r <= cy1; r++) for (int c = 0; c < MAP_W; c++) {
        u8 t = world.map->tile[r * MAP_W + c]; int kind = art::TL_GROUND, var = hh(c, r) & 7;
        if (t == T_ZONE) kind = zoneOwner(r) == 0 ? art::TL_ZONE0 : art::TL_ZONE1; else if (t == T_ROCK) kind = art::TL_ROCK;
        else if (t == T_WATER) { kind = art::TL_WATER; var = (int)(tm * 2 + (c + r) % 2) & 1; } else if (t == T_TREE) kind = art::TL_TREE;
        int sx, sy; worldToScr(c * 16, r * 16, sx, sy);
        DrawTextureRec(T, art::tile(kind, var), {(float)(sx + shx), (float)(sy + shy)}, WHITE);
    }
    if (selBuild >= 0) {
        for (int r = std::max(cy0, MAP_H - 12); r <= cy1; r++) for (int c = 0; c < MAP_W; c++) {
            if (world.map->tile[r * MAP_W + c] != T_ZONE || zoneOwner(r) != 0 || world.occ[r * MAP_W + c]) continue;
            int sx, sy; worldToScr(c * 16, r * 16, sx, sy); DrawRectangle(sx + shx, sy + shy, 15, 15, K(255, 255, 255, 26));
        }
    }
    // замки
    for (int o = 0; o < 2; o++) {
        int sx, sy; worldToScr(CASTLE_CX * 16, o == 1 ? 0 : (MAP_H - CASTLE_H) * 16, sx, sy);
        bool dmg = world.pl[o].castleHp * 2 < world.pl[o].castleMax;
        DrawTextureRec(T, art::castle(o, dmg), {(float)(sx + shx), (float)(sy + shy)}, WHITE);
    }
    // здания
    for (auto& b : world.blds) {
        const BuildDef& bd = world.raceOf(b.owner).b[b.def]; int sx, sy; worldToScr(b.cx * 16, b.cy * 16, sx, sy);
        if (sy > mapBot || sy + bd.h * 16 < mapTop) continue;
        DrawTextureRec(T, art::building(b.owner == 0 ? myRace : foeRace, b.def, b.owner), {(float)(sx + shx), (float)(sy + shy)}, WHITE);
        if (b.hp < b.maxhp) { int w = bd.w * 16 - 4; DrawRectangle(sx + 2 + shx, sy + shy - 3, w, 2, K(20, 14, 28)); DrawRectangle(sx + 2 + shx, sy + shy - 3, w * b.hp / b.maxhp, 2, b.owner == 0 ? K(90, 170, 255) : K(240, 90, 80)); }
        if (b.id == selPlaced) { Color c = ((int)(tm * 4) & 1) ? WHITE : GLD; DrawRectangleLines(sx + shx - 1, sy + shy - 1, bd.w * 16 + 2, bd.h * 16 + 2, c); }
    }
    // юниты по y
    static std::vector<int> idx; idx.clear(); for (size_t i = 0; i < world.units.size(); i++) idx.push_back((int)i);
    std::sort(idx.begin(), idx.end(), [&](int a, int b) { return world.units[a].y < world.units[b].y; });
    for (int i : idx) {
        const Unit& u = world.units[i]; const UnitDef& d = world.raceOf(u.owner).u[u.def]; int rc = u.owner == 0 ? myRace : foeRace;
        float ux = (float)u.x, uy = (float)u.y; auto it = prev.find(u.id);
        if (it != prev.end()) { ux = it->second.first + (u.x - it->second.first) * alpha; uy = it->second.second + (u.y - it->second.second) * alpha; }
        int wx = (int)ux >> 4, wy = (int)uy >> 4; int sx, sy; worldToScr(wx, wy, sx, sy);
        if (sy < mapTop - 40 || sy > mapBot + 40) continue;
        int S = art::unitSize(rc, u.def); bool fly = (d.flags & UF_FLY) != 0; int lift = fly ? 8 : 0;
        DrawTextureRec(T, art::shadow(), {(float)(sx - 8 + shx), (float)(sy - 2 + shy)}, K(255, 255, 255, fly ? 120 : 255));
        int frame = u.atkAnim > 0 ? 2 : ((u.anim / 4) & 1);
        DrawTextureRec(T, art::unit(rc, u.def, u.owner, frame), {(float)(sx - S / 2 + shx), (float)(sy - S * 3 / 4 - lift + shy)}, u.slow > 0 ? K(170, 200, 255) : WHITE);
        if (u.hp < u.maxhp) { int w = S * 3 / 4, bx = sx - w / 2 + shx, by = sy - S * 3 / 4 - lift - 3 + shy; DrawRectangle(bx, by, w, 2, K(20, 14, 28)); DrawRectangle(bx, by, std::max(1, w * u.hp / u.maxhp), 2, u.owner == 0 ? K(90, 190, 255) : K(240, 90, 80)); }
    }
    for (auto& p : projs) { float x = p.x0 + (p.x1 - p.x0) * p.t, y = p.y0 + (p.y1 - p.y0) * p.t - 6 - sinf(p.t * 3.14f) * 4; int sx, sy; worldToScr((int)x, (int)y, sx, sy); DrawRectangle(sx - 1 + shx, sy - 1 + shy, 3, 3, p.c); }
    for (auto& p : parts) { int sx, sy; worldToScr((int)p.x, (int)p.y, sx, sy); DrawRectangle(sx + shx, sy + shy, 2, 2, p.c); }
    for (auto& f : ftx) { int sx, sy; worldToScr((int)f.x, (int)f.y, sx, sy); font::drawShadow(f.s, sx - 8, sy, f.c, K(10, 6, 20)); }
    // бабочки на карте
    for (int i = 0; i < 8; i++) {
        float bx = 20 + (hh(i, 3) % 200) + 10 * sinf(tm * 0.6f + i), by = (hh(i, 8) % (MAP_H * 16 - 40)) + 6 * sinf(tm * 1.1f + i * 2);
        int sx, sy; worldToScr((int)bx, (int)by, sx, sy); if (sy < mapTop || sy > mapBot) continue;
        Color c = ColorFromHSV((float)((hh(i, 1) % 5) * 70 + 20), 0.5f, 1.f); int f = ((int)(tm * 6 + i)) & 1;
        if (f) { DrawRectangle(sx - 2, sy - 1, 2, 2, c); DrawRectangle(sx, sy - 1, 2, 2, c); } else { DrawRectangle(sx - 1, sy, 2, 2, c); }
    }
    // призрак здания
    if (selBuild >= 0 && ghostX >= 0) {
        const BuildDef& b = world.raceOf(0).b[selBuild]; int sx, sy; worldToScr(ghostX * 16, ghostY * 16, sx, sy);
        bool ok = placeable(ghostX, ghostY);
        DrawTextureRec(T, art::building(myRace, selBuild, 0), {(float)sx, (float)sy}, K(255, 255, 255, 170));
        DrawRectangle(sx, sy, b.w * 16, b.h * 16, ok ? K(80, 255, 120, 70) : K(255, 70, 70, 90));
        DrawRectangleLines(sx, sy, b.w * 16, b.h * 16, ok ? K(150, 255, 170) : K(255, 130, 120));
    }
    EndScissorMode();
    if (flash > 0) DrawRectangle(0, mapTop, VW, viewH, K(255, 30, 30, (int)(flash * 110)));
}

// ------------------------------------------------------------------ интерфейс игры
void App::drawBars() {
    int y = TOPI + 22, h = 10, half = VW / 2;
    for (int o = 1; o >= 0; o--) {
        int x = o == 1 ? 0 : half, w = half; const Player& P = world.pl[o];
        DrawRectangle(x, y, w, h, K(20, 14, 32)); int fw = (int)((long long)(w - 2) * std::max(0, P.castleHp) / P.castleMax);
        DrawRectangle(x + 1, y + 1, fw, h - 2, o == 0 ? K(60, 120, 230) : K(200, 60, 55));
        DrawRectangle(x + 1, y + 1, fw, 2, K(255, 255, 255, 50));
        char b[32]; snprintf(b, sizeof b, "%s %d", o == 0 ? "ВЫ" : "ВРАГ", std::max(0, P.castleHp)); font::drawShadow(b, x + 4, y + 2, INK, K(10, 6, 20));
    }
    DrawRectangle(half - 1, y, 2, h, K(10, 6, 20));
}

void App::gameInput() {
    bool overMap = in.y >= mapTop && in.y < mapBot && !pauseMenu;
    if (in.pressed && overMap) { mapDrag = true; mapDragMoved = 0; }
    if (mapDrag && in.down) { camY -= in.dy; mapDragMoved += std::abs(in.dx) + std::abs(in.dy); clampCam(); }
    if (mapDrag && in.released) {
        mapDrag = false;
        if (mapDragMoved < 6) {
            int wx = in.x - mapX0, wy = in.y - mapTop + (int)camY; int cx = wx / 16, cy = wy / 16;
            if (selBuild >= 0) {
                const BuildDef& b = world.raceOf(0).b[selBuild];
                ghostX = std::max(0, std::min(MAP_W - b.w, cx - b.w / 2)); ghostY = std::max(0, std::min(MAP_H - b.h, cy - b.h / 2)); sfx::play(sfx::S_TAP);
            } else {
                const Building* bl = (cx >= 0 && cx < MAP_W && cy >= 0 && cy < MAP_H) ? world.bldAt(cx, cy) : nullptr;
                selPlaced = (bl && bl->owner == 0) ? bl->id : 0; if (selPlaced) { sfx::play(sfx::S_TAP); tab = 0; }
            }
        }
    }
}

static void unitInfo(const UnitDef& u, const BuildDef& b, int x, int y, Color c1, Color c2) {
    char l[96];
    snprintf(l, sizeof l, "ЦЕНА %d  ЗДР %d  УРОН %d", b.cost, u.hp, u.dmg); font::draw(l, x, y, c1);
    snprintf(l, sizeof l, "%s / %s %d  ДАЛЬН %d", ATK_SHORT[u.atk], ARM_SHORT[u.arm], u.armor, u.range); font::draw(l, x, y + 9, c2);
    snprintf(l, sizeof l, "СКОР %d  СПАВН %ds%s%s%s", u.speed, b.spawn / 20, (u.flags & UF_FLY) ? "  ЛЕТАЕТ" : "", (u.flags & UF_HEAL) ? "  ЛЕЧИТ" : "", (u.flags & UF_SLOW) ? "  ЗАМЕДЛ" : "");
    font::draw(l, x, y + 18, c2);
    if (u.desc && u.desc[0]) { char d[80]; snprintf(d, sizeof d, "%s", u.desc); while (font::width(d) > 150 && strlen(d) > 3) d[strlen(d) - 1] = 0; font::draw(d, x, y + 27, K(150, 210, 255)); }
}

void App::drawBuildTab(int py) {
    const RaceDef& R = world.raceOf(0); int y0 = py + 22; const Player& P = world.pl[0];
    if (selBuild >= 0) {
        const BuildDef& b = R.b[selBuild]; const UnitDef& u = R.u[selBuild];
        DrawTexturePro(art::tex(), art::unit(myRace, selBuild, 0, 0), {(float)6, (float)(y0 + 2), 32, 32}, {0, 0}, 0, WHITE);
        font::draw(b.name, 44, y0 + 1, INK); char t[40]; snprintf(t, sizeof t, "%s", u.name); font::draw(t, 44, y0 + 10, GLD);
        char l[96];
        snprintf(l, sizeof l, "ЦЕНА %d  ЗДР %d  УРОН %d  %s", b.cost, u.hp, u.dmg, ATK_SHORT[u.atk]); font::draw(l, 6, y0 + 38, DIM);
        snprintf(l, sizeof l, "БРОНЯ %s %d  ДАЛЬН %d  СПАВН %ds", ARM_SHORT[u.arm], u.armor, u.range, b.spawn / 20); font::draw(l, 6, y0 + 47, DIM);
        bool ok = placeable(ghostX, ghostY); const char* why = nullptr; world.canBuild(0, selBuild, ghostX, ghostY, &why);
        if (button(6, y0 + 58, VW / 2 - 8, 24, ok ? "Построить" : (why ? why : "?"), ok ? 3 : 0, true)) {
            if (ok) { Command c; c.type = CMD_BUILD; c.player = 0; c.a = selBuild; c.b = ghostX; c.c = ghostY; world.submit(c); showMsg(""); findFree(selBuild); }
            else { sfx::play(sfx::S_ERROR); showMsg(why ? why : "Нельзя"); }
        }
        if (button(VW / 2 + 2, y0 + 58, VW / 2 - 8, 24, "Отмена", 2)) selBuild = -1;
        return;
    }
    if (selPlaced) {
        const Building* b = world.findBld(selPlaced);
        if (!b) { selPlaced = 0; return; }
        const BuildDef& bd = R.b[b->def]; const UnitDef& u = R.u[b->def];
        DrawTexturePro(art::tex(), art::unit(myRace, b->def, 0, 0), {(float)6, (float)(y0 + 2), 32, 32}, {0, 0}, 0, WHITE);
        font::draw(bd.name, 44, y0 + 1, INK); font::draw(u.name, 44, y0 + 10, GLD);
        char l[96]; snprintf(l, sizeof l, "ЗДАНИЕ %d/%d", b->hp, b->maxhp); font::draw(l, 6, y0 + 38, DIM);
        snprintf(l, sizeof l, "СЛЕДУЮЩИЙ БОЕЦ ЧЕРЕЗ %ds", std::max(0, b->spawnT) / 20); font::draw(l, 6, y0 + 47, DIM);
        char t[32]; snprintf(t, sizeof t, "Продать +%d", bd.cost * 6 / 10);
        if (button(6, y0 + 58, VW / 2 - 8, 24, t, 2)) { Command c; c.type = CMD_SELL; c.player = 0; c.a = selPlaced; world.submit(c); selPlaced = 0; }
        if (button(VW / 2 + 2, y0 + 58, VW / 2 - 8, 24, "Закрыть")) selPlaced = 0;
        return;
    }
    int cols = 6, cw = (VW - 8) / cols, ch = 26;
    for (int i = 0; i < R.nb; i++) {
        int x = 4 + (i % cols) * cw, y = y0 + (i / cols) * (ch + 2); const BuildDef& b = R.b[i];
        bool locked = b.req >= 0 && !world.hasBuilding(0, b.req), afford = P.gold >= b.cost;
        panel(x, y, cw - 2, ch, locked ? K(34, 30, 46) : K(56, 46, 92), locked ? K(70, 66, 86) : (afford ? GLD : K(110, 96, 150)));
        Rectangle ur = art::unit(myRace, i, 0, 0);
        DrawTexturePro(art::tex(), ur, {(float)(x + (cw - 2) / 2 - 8), (float)(y + 1), 16, 16}, {0, 0}, 0, locked ? K(90, 90, 100) : WHITE);
        char t[12]; snprintf(t, sizeof t, "%d", b.cost); font::drawCenter(t, x + (cw - 2) / 2, y + 18, locked ? K(100, 96, 120) : (afford ? GLD : REDC), false);
        if (locked) icon(art::IC_LOCK, x + cw - 11, y + 1, K(200, 200, 215));
        if (hit(x, y, cw - 2, ch) && in.released && in.dragTotal <= 6 && !pauseMenu) {
            if (locked) { sfx::play(sfx::S_ERROR); char m[48]; snprintf(m, sizeof m, "Нужно: %s", R.b[b.req].name); showMsg(m); }
            else { selBuild = i; selPlaced = 0; findFree(i); sfx::play(sfx::S_TAP); }
        }
    }
}

void App::drawUpgTab(int py) {
    int y0 = py + 22; const Player& P = world.pl[0];
    for (int i = 0; i < NUM_UPGRADES; i++) {
        int y = y0 + i * 14; const UpgradeDef& U = UPGRADES[i]; int lv = P.up[i];
        font::draw(U.name, 6, y + 3, INK);
        for (int k = 0; k < U.levels; k++) DrawRectangle(62 + k * 7, y + 3, 5, 6, k < lv ? GLD : K(60, 52, 90));
        char d[40]; snprintf(d, sizeof d, "%s", U.desc); if (VW > 260) font::draw(d, 62 + 5 * 7 + 6, y + 3, DIM);
        int cost = world.upgradeCost(0, i); char t[16]; if (cost) snprintf(t, sizeof t, "%d", cost); else snprintf(t, sizeof t, "МАКС");
        if (button(VW - 62, y, 56, 13, t, cost && P.gold >= cost ? 1 : 0, cost > 0) && cost) {
            if (P.gold >= cost) { Command c; c.type = CMD_UPGRADE; c.player = 0; c.a = i; world.submit(c); } else { sfx::play(sfx::S_ERROR); showMsg("Мало золота"); }
        }
    }
}

void App::drawInfoTab(int py) {
    int y = py + 24; char l[96]; const Player& P = world.pl[0];
    snprintf(l, sizeof l, "ДОХОД: +%d / 10С  (ЧЕРЕЗ %dС)", world.income(0), world.ticksToIncome() / 20); font::draw(l, 6, y, GLD); y += 10;
    snprintf(l, sizeof l, "ВОЙСК: %d   У ВРАГА: %d", world.countUnits(0), world.countUnits(1)); font::draw(l, 6, y, INK); y += 10;
    int nb = 0, ne = 0; for (auto& b : world.blds) { if (b.owner == 0) nb++; else ne++; }
    snprintf(l, sizeof l, "ЗДАНИЙ: %d   У ВРАГА: %d", nb, ne); font::draw(l, 6, y, INK); y += 10;
    snprintf(l, sizeof l, "УБИТО: %d   ЗАРАБОТАНО: %d", P.kills, P.earned); font::draw(l, 6, y, INK); y += 10;
    snprintf(l, sizeof l, "ВРАГ: %s   КАРТА: %s", RACE_RU[foeRace], world.map->name); font::draw(l, 6, y, DIM); y += 10;
    snprintf(l, sizeof l, "БОТ: %s", ai::LEVEL_NAMES[level]); font::draw(l, 6, y, DIM); y += 10;
    int cnt[MAX_BUILDS] = {0}; for (auto& u : world.units) if (u.owner == 1) cnt[u.def]++;
    int best = -1; for (int i = 0; i < MAX_BUILDS; i++) if (cnt[i] > 0 && (best < 0 || cnt[i] > cnt[best])) best = i;
    if (best >= 0) { snprintf(l, sizeof l, "ЧАЩЕ ВСЕГО У ВРАГА: %s x%d", world.raceOf(1).u[best].name, cnt[best]); font::draw(l, 6, y, K(255, 170, 150)); }
}

void App::drawPause() {
    DrawRectangle(0, 0, VW, VH, K(8, 6, 16, 170));
    int w = VW - 40, x = 20, y = VH / 2 - 84;
    panel(x - 6, y - 24, w + 12, 190, K(34, 26, 58), GLD);
    font::drawCenter("ПАУЗА", VW / 2, y - 14, GLD);
    if (button(x, y + 4, w, 24, "Продолжить", 3)) pauseMenu = false;
    char b[32]; snprintf(b, sizeof b, "ВИБРАЦИЯ: %s", sd.vibration ? "ВКЛ" : "ВЫКЛ");
    if (button(x, y + 32, w, 22, b)) { sd.vibration ^= 1; writeSave(sd); if (sd.vibration) plat::vibrate(60); }
    snprintf(b, sizeof b, "ЗВУК: %s", sd.sound ? "ВКЛ" : "ВЫКЛ");
    if (button(x, y + 58, w, 22, b)) { sd.sound ^= 1; sfx::setEnabled(sd.sound, sd.music); writeSave(sd); }
    snprintf(b, sizeof b, "МУЗЫКА: %s", sd.music ? "ВКЛ" : "ВЫКЛ");
    if (button(x, y + 84, w, 22, b)) { sd.music ^= 1; sfx::setEnabled(sd.sound, sd.music); writeSave(sd); }
    static int surr = 0; if (!pauseMenu) surr = 0;
    if (button(x, y + 112, w, 22, surr ? "ТОЧНО СДАТЬСЯ?" : "Сдаться", 2)) { if (surr) { Command c; c.type = CMD_SURRENDER; c.player = 0; world.submit(c); world.step(); handleEvents(); pauseMenu = false; surr = 0; } else surr = 1; }
    if (button(x, y + 138, w, 22, "В меню")) { pauseMenu = false; go(mission >= 0 ? SC_CAMP : SC_MENU); }
}

void App::gameUI() {
    gameInput();
    // верхняя панель
    panel(0, 0, VW, TOPI + 22, K(30, 22, 52), K(110, 90, 150));
    icon(art::IC_COIN, 6, TOPI + 3); char b[48]; snprintf(b, sizeof b, "%d", world.pl[0].gold); font::drawShadow(b, 17, TOPI + 4, GLD, K(10, 6, 20));
    snprintf(b, sizeof b, "+%d", world.income(0)); font::draw(b, 17 + font::width("00000"), TOPI + 4, K(150, 220, 150));
    float pr = 1.f - (float)world.ticksToIncome() / INCOME_PERIOD;
    DrawRectangle(6, TOPI + 15, 80, 3, K(20, 14, 32)); DrawRectangle(6, TOPI + 15, (int)(80 * pr), 3, GLD);
    int secs = world.tick / 20; snprintf(b, sizeof b, "%d:%02d", secs / 60, secs % 60); font::drawCenter(b, VW / 2 + 20, TOPI + 5, INK);
    snprintf(b, sizeof b, "x%d", speed); if (button(VW - 48, TOPI + 3, 22, 16, b, speed > 1 ? 1 : 0) && !pauseMenu) speed = speed == 1 ? 2 : 1;
    if (button(VW - 24, TOPI + 3, 18, 16, "") && !pauseMenu) pauseMenu = true;
    DrawRectangle(VW - 19, TOPI + 7, 3, 8, INK); DrawRectangle(VW - 13, TOPI + 7, 3, 8, INK);
    drawBars();
    // нижняя панель
    int py = mapBot; panel(0, py, VW, PANEL_H, K(34, 26, 58), K(110, 90, 150));
    const char* tabs[3] = {"Стройка", "Прокачка", "Инфо"}; int tw = (VW - 8) / 3;
    for (int i = 0; i < 3; i++) if (button(4 + i * tw, py + 3, tw - 2, 16, tabs[i], tab == i ? 1 : 0, !pauseMenu)) { tab = i; selBuild = -1; selPlaced = 0; }
    if (!pauseMenu) { if (tab == 0) drawBuildTab(py); else if (tab == 1) drawUpgTab(py); else drawInfoTab(py); }
    if (msgT > 0 && msg[0]) { int w = font::width(msg) + 12; panel((VW - w) / 2, mapTop + 4, w, 14, K(30, 22, 50, 235), GLD); font::drawCenter(msg, VW / 2, mapTop + 7, INK); }
    if (pauseMenu) drawPause();
    if (in.back && cur == SC_GAME) { if (selBuild >= 0 || selPlaced) { selBuild = -1; selPlaced = 0; } else pauseMenu = !pauseMenu; }
}

void App::testAdvance(int ticks) {   // только для автотестов: гонит симуляцию, второй бот играет за игрока
    static ai::Bot pb(0, 3, 5); std::vector<Command> c;
    for (int i = 0; i < ticks && world.winner < 0; i++) {
        prev.clear(); for (auto& u : world.units) prev[u.id] = {u.x, u.y};
        c.clear(); pb.think(world, c); bot.think(world, c); for (auto& x : c) world.submit(x);
        world.step(); handleEvents();
    }
    if (world.winner >= 0 && endTimer < 0) endTimer = 0.01f;
}
