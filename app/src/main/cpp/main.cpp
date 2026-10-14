// Client: portrait pixel-art UI rendered at 270 x (480..640) virtual pixels and scaled to the screen.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "raylib.h"
#include "ai/bot.h"
#include "audio/synth.h"
#include "core/save.h"
#include "platform/platform.h"
#include "sim/sim.h"
#include "ui/art.h"

// ============================================================ globals / helpers
static const int gW = 270;
static int gH = 480;
static RenderTexture2D gRT;
static art::Atlas A;
static Font gFont; static int gFontPx = 10; static bool gCyr = false;
static float gScale = 1, gOffX = 0, gOffY = 0;

static const Color cBg = {20, 16, 28, 255}, cPanel = {36, 28, 51, 255}, cPanel2 = {52, 42, 74, 255}, cEdge = {96, 78, 130, 255};
static const Color cGold = {240, 200, 80, 255}, cText = {236, 228, 244, 255}, cDim = {150, 140, 170, 255}, cRed = {230, 80, 80, 255}, cGreen = {110, 220, 120, 255}, cBlue = {90, 150, 255, 255};

static const char* Tr(const char* en, const char* ru) { return (gSettings.lang == 1 && gCyr) ? ru : en; }

static int TextW(const char* s) { return (int)MeasureTextEx(gFont, s, (float)gFontPx, 1).x; }
static void Text(const char* s, int x, int y, Color c, int align = 0) {      // 0 left, 1 center, 2 right
  int w = TextW(s); if (align == 1) x -= w / 2; else if (align == 2) x -= w;
  DrawTextEx(gFont, s, {(float)x, (float)y}, (float)gFontPx, 1, c);
}
static int Wrapped(const char* text, int x, int y, int w, Color c, int maxLines) {   // returns lines drawn
  std::string line, word; int lines = 0; std::string t = text; t += ' ';
  for (char ch : t) {
    if (ch != ' ') { word += ch; continue; }
    std::string trial = line.empty() ? word : line + " " + word;
    if (TextW(trial.c_str()) > w && !line.empty()) {
      if (lines >= maxLines) return lines;
      Text(line.c_str(), x, y + lines * (gFontPx + 2), c); lines++; line = word;
    } else line = trial;
    word.clear();
  }
  if (!line.empty() && lines < maxLines) { Text(line.c_str(), x, y + lines * (gFontPx + 2), c); lines++; }
  return lines;
}
static void Box(Rectangle r, Color fill, Color edge) {
  DrawRectangleRec(r, fill); DrawRectangleLinesEx(r, 1, edge);
  DrawRectangle((int)r.x + 1, (int)r.y + 1, (int)r.width - 2, 1, {255, 255, 255, 18});
}
static void Bar(int x, int y, int w, int h, float f, Color fill) {
  DrawRectangle(x, y, w, h, {10, 8, 16, 255}); DrawRectangle(x, y, (int)(w * std::max(0.f, std::min(1.f, f))), h, fill);
}

// ---------------- input in virtual coordinates
struct Input { Vector2 pos{0, 0}, press{0, 0}; bool down = false, pressed = false, released = false, tap = false; Vector2 delta{0, 0}; } In;
static void ReadInput() {
  Vector2 m = GetMousePosition(); Vector2 p = {(m.x - gOffX) / gScale, (m.y - gOffY) / gScale};
  In.delta = In.down ? Vector2{p.x - In.pos.x, p.y - In.pos.y} : Vector2{0, 0};
  In.pos = p; In.pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT); In.released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
  if (In.pressed) In.press = p;
  In.down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
  In.tap = In.released && std::fabs(p.x - In.press.x) < 5 && std::fabs(p.y - In.press.y) < 5;
}
static bool Inside(Rectangle r, Vector2 p) { return p.x >= r.x && p.y >= r.y && p.x < r.x + r.width && p.y < r.y + r.height; }
static void Haptic(int ms, bool allowed) { if (gSettings.vib && allowed) plat::Vibrate(ms); }
static bool Button(Rectangle r, const char* label, bool enabled = true, Color accent = {0, 0, 0, 0}) {
  bool hot = enabled && In.down && Inside(r, In.press) && Inside(r, In.pos);
  Color f = !enabled ? Color{30, 24, 40, 255} : hot ? cEdge : (accent.a ? accent : cPanel2);
  Box({r.x, r.y + (hot ? 1.f : 0.f), r.width, r.height}, f, enabled ? cEdge : Color{60, 50, 80, 255});
  Text(label, (int)(r.x + r.width / 2), (int)(r.y + r.height / 2 - gFontPx / 2 + (hot ? 1 : 0)), enabled ? cText : cDim, 1);
  if (enabled && In.tap && Inside(r, In.press)) { synth::Play(synth::CLICK); Haptic(8, gSettings.vibUi); return true; }
  return false;
}

// ============================================================ campaign data
struct Stage { const char *name, *nameRu; int map, myRace, oppRace; ai::Level lvl; int rewardRace; };
static const Stage kStages[] = {
  {"First Blood", "Первая кровь", 0, 0, 1, ai::Level::Easy, 1},
  {"Hollow Road", "Пустая дорога", 1, 0, 0, ai::Level::Easy, -1},
  {"Rise of the Dead", "Восстание мёртвых", 0, 1, 0, ai::Level::Normal, -1},
  {"Bone Mirror", "Костяное зеркало", 1, 1, 1, ai::Level::Normal, -1},
  {"Crown of Thorns", "Терновый венец", 0, 0, 1, ai::Level::Hard, -1},
  {"The Last Gate", "Последние врата", 1, 1, 0, ai::Level::Genius, -1},
};
static const int kStageCount = 6;
static const char* LevelName(ai::Level l) {
  switch (l) { case ai::Level::Easy: return Tr("Easy", "Лёгкий"); case ai::Level::Normal: return Tr("Normal", "Средний"); case ai::Level::Hard: return Tr("Hard", "Сложный"); default: return Tr("Genius", "Гений"); }
}

// ============================================================ match state
enum Scr { S_MENU, S_CAMPAIGN, S_SKIRMISH, S_RACES, S_SETTINGS, S_GAME, S_RESULT };
static Scr gScr = S_MENU;

struct Particle { float x, y, vx, vy; int life, max; Color c; int kind; float tx, ty; };   // kind 0 spark, 1 projectile, 2 floating text(gold)
struct Match {
  sim::Sim* s = nullptr; ai::Bot* bot = nullptr;
  int map = 0, myRace = 0, oppRace = 1, winsNeeded = 1, score[2] = {0, 0}, stage = -1, round = 1; ai::Level lvl = ai::Level::Normal;
  float acc = 0; bool paused = false; int tab = 0; int selDef = -1, selBld = -1; float scroll = 0; int shake = 0, lastVib = -999;
  int ghostX = -1, ghostY = -1; bool matchOver = false, iWonMatch = false;
  std::vector<Particle> parts;
} M;
static float gTime = 0;

static void NewRound() {
  delete M.s; delete M.bot;
  int races[2] = {M.myRace, M.oppRace}, teams[2] = {0, 1};
  M.s = new sim::Sim(M.map, races, teams, 2, 1234u + (uint32_t)(GetTime() * 1000));
  M.s->players[1].isBot = true;
  M.bot = new ai::Bot(1, M.lvl, 777u + M.round);
  M.acc = 0; M.paused = false; M.selDef = -1; M.selBld = -1; M.parts.clear(); M.tab = 0; M.scroll = 0;
}
static void StartMatch(int map, int my, int opp, ai::Level lvl, int stage) {
  M.map = map; M.myRace = my; M.oppRace = opp; M.lvl = lvl; M.stage = stage; M.winsNeeded = gSettings.wins;
  M.score[0] = M.score[1] = 0; M.round = 1; M.matchOver = false;
  NewRound(); gScr = S_GAME;
}

// ============================================================ layout
static const int kHudH = 24, kPanelH = 72;
static int MapX() { return (gW - 16 * 16) / 2; }
static int MapY() { int avail = gH - kHudH - kPanelH; return kHudH + std::max(0, (avail - 24 * 16) / 2); }

// ============================================================ effects from sim events
static void Burst(float x, float y, Color c, int n, float speed = 1.2f) {
  for (int i = 0; i < n; i++) { float a = (float)GetRandomValue(0, 628) / 100.f, v = speed * GetRandomValue(30, 100) / 100.f; M.parts.push_back({x, y, cosf(a) * v, sinf(a) * v, 12 + GetRandomValue(0, 8), 20, c, 0, 0, 0}); }
}
static void HandleEvents() {
  auto& ev = M.s->events; static int lastHitSnd = 0;
  for (auto& e : ev) {
    switch (e.type) {
      case sim::EV_SPAWN: Burst((float)e.x, (float)e.y, {250, 240, 160, 255}, 5, 0.6f); break;
      case sim::EV_HIT: Burst((float)e.a, (float)e.b, {255, 255, 255, 255}, 3); if (gTime - lastHitSnd > 0.12f) { synth::Play(synth::HIT); lastHitSnd = (int)gTime; } break;
      case sim::EV_SHOT: M.parts.push_back({(float)e.x, (float)e.y, 0, 0, 7, 7, {255, 235, 140, 255}, 1, (float)e.a, (float)e.b}); break;
      case sim::EV_DEATH: Burst((float)e.x, (float)e.y, e.a == 0 ? Color{120, 170, 255, 255} : Color{255, 120, 120, 255}, 9, 1.5f); break;
      case sim::EV_BUILD: Burst((float)e.x, (float)e.y, {200, 190, 170, 255}, 8, 0.8f); if (e.a == 0) synth::Play(synth::BUILD); break;
      case sim::EV_UPGRADE: Burst((float)e.x, (float)e.y, cGold, 12, 1.4f); synth::Play(synth::BUILD); break;
      case sim::EV_TECH: if (e.a == 0) synth::Play(synth::WIN); break;
      case sim::EV_BUY: if (e.a == 0) synth::Play(synth::COIN); break;
      case sim::EV_CASTLE_HIT:
        if (e.a == 0) { M.shake = 5; if (M.s->tick - M.lastVib > 10) { Haptic(35, gSettings.vibCastle); M.lastVib = M.s->tick; synth::Play(synth::CASTLE); } }
        Burst((float)e.x, (float)e.y, {255, 200, 120, 255}, 2); break;
      case sim::EV_INCOME: if (e.a == 0) { synth::Play(synth::COIN); M.parts.push_back({(float)(gW / 2), 0, 0, -0.3f, 40, 40, cGold, 2, (float)e.b, 0}); } break;
      default: break;
    }
  }
  ev.clear();
}

// ============================================================ drawing the world
struct Drawable { float y; int kind; int idx; };
static void DrawHpBar(int x, int y, int w, int hp, int mx, Color c) { if (hp < mx) Bar(x, y, w, 2, (float)hp / mx, c); }

static void DrawWorld(float alpha) {
  const sim::Sim& s = *M.s;
  int ox = MapX(), oy = MapY();
  if (M.shake > 0) { ox += GetRandomValue(-1, 1); oy += GetRandomValue(-1, 1); M.shake--; }
  for (int y = 0; y < s.h; y++) for (int x = 0; x < s.w; x++) {
    char c = s.map->at(x, y); int px = ox + x * 16, py = oy + y * 16;
    uint32_t hsh = (uint32_t)(x * 73856093 ^ y * 19349663);
    if (c == 'a' || c == 'b') DrawTexture(A.zone[c == 'a' ? 0 : 1], px, py, WHITE);
    else DrawTexture(c == '#' ? A.rock : A.grass[hsh % 3], px, py, WHITE);
  }
  if (M.selDef >= 0) for (int y = 0; y < s.h; y++) for (int x = 0; x < s.w; x++)       // placement hints
    if (s.map->at(x, y) == 'a' && !s.BuildingAt(x, y)) DrawRectangle(ox + x * 16 + 7, oy + y * 16 + 7, 2, 2, {255, 255, 255, 60});

  std::vector<Drawable> dr;
  for (size_t i = 0; i < s.buildings.size(); i++) if (s.buildings[i].alive) dr.push_back({(float)(s.buildings[i].y + s.buildings[i].h) * 16, 0, (int)i});
  for (size_t i = 0; i < s.units.size(); i++) { float px, py; s.UnitPx(s.units[i], alpha, px, py); dr.push_back({py + 16, 1, (int)i}); }
  std::sort(dr.begin(), dr.end(), [](const Drawable& a, const Drawable& b) { return a.y < b.y; });
  for (auto& d : dr) {
    if (d.kind == 0) {
      const sim::Building& b = s.buildings[d.idx]; int px = ox + b.x * 16, py = oy + b.y * 16;
      Color tint = b.flash > 0 ? Color{255, 170, 170, 255} : WHITE;
      if (b.castle) { DrawTexture(A.castle[b.team], px, py, tint); Bar(px + 2, b.team == 1 ? py + 33 : py - 5, 60, 3, (float)b.hp / b.maxHp, b.team == 0 ? cBlue : cRed); continue; }
      Texture2D t = b.treasure ? A.treasure : A.building[b.def];
      if (b.buildLeft > 0) { tint.a = 130; }
      DrawTexture(t, px, py, tint);
      DrawRectangle(px, py + b.h * 16 - 2, b.w * 16, 2, b.team == 0 ? cBlue : cRed);            // team strip
      if (b.buildLeft > 0) Bar(px + 1, py + b.h * 8 - 1, b.w * 16 - 2, 3, 1.f - (float)b.buildLeft / cfg::kBuildTicks, cGold);
      DrawHpBar(px + 1, py - 3, b.w * 16 - 2, b.hp, b.maxHp, b.team == 0 ? cBlue : cRed);
      if (b.id == M.selBld) DrawRectangleLines(px - 1, py - 1, b.w * 16 + 2, b.h * 16 + 2, cGold);
    } else {
      const sim::Unit& u = s.units[d.idx]; float fx, fy; s.UnitPx(u, alpha, fx, fy);
      int px = ox + (int)std::floor(fx + 0.5f), py = oy + (int)std::floor(fy + 0.5f);       // whole-pixel positions: crisp pixel-by-pixel motion
      int frame = u.moving ? (((int)(fx + fy)) / 4) & 1 : 0;
      Rectangle src = {0, 0, (float)(u.face < 0 ? -16 : 16), 16};
      Color tint = u.flash > 0 ? Color{255, 255, 255, 255} : WHITE;
      if (u.flash > 0) tint = {255, 200, 200, 255};
      DrawTexturePro(A.unit[u.def][frame], src, {(float)px, (float)py, 16, 16}, {0, 0}, 0, tint);
      DrawRectangle(px + 1, py + 1, 2, 2, u.team == 0 ? cBlue : cRed);
      DrawHpBar(px + 3, py - 2, 10, u.hp, u.maxHp, u.team == 0 ? cGreen : cRed);
    }
  }
  // ghost
  if (M.selDef >= 0 && In.down && M.ghostX >= 0) {
    int sz = s.BuildSize(M.selDef); bool ok = s.CanBuild(0, M.selDef, M.ghostX, M.ghostY);
    int px = ox + M.ghostX * 16, py = oy + M.ghostY * 16;
    DrawRectangle(px, py, sz * 16, sz * 16, ok ? Color{80, 255, 120, 90} : Color{255, 70, 70, 100});
    DrawTexture(M.selDef == sim::kTreasureDef ? A.treasure : A.building[M.selDef], px, py, {255, 255, 255, 170});
  }
  for (auto& p : M.parts) {
    if (p.kind == 0) DrawRectangle(ox + (int)p.x, oy + (int)p.y, 2, 2, {p.c.r, p.c.g, p.c.b, (unsigned char)(255 * p.life / p.max)});
    else if (p.kind == 1) { float t = 1.f - (float)p.life / p.max; DrawRectangle(ox + (int)(p.x + (p.tx - p.x) * t), oy + (int)(p.y + (p.ty - p.y) * t), 3, 3, p.c); }
  }
}
static void UpdateParts() {
  for (auto& p : M.parts) { p.life--; if (p.kind == 0 || p.kind == 2) { p.x += p.vx; p.y += p.vy; p.vy += p.kind == 0 ? 0.03f : 0; } }
  M.parts.erase(std::remove_if(M.parts.begin(), M.parts.end(), [](const Particle& p) { return p.life <= 0; }), M.parts.end());
}

// ============================================================ in-game HUD and panel
static const char* ShopAbbr[sim::ITEM_COUNT] = {"SPD", "DMG", "x2", "x4", "HP"};
static void DrawCard(Rectangle r, bool sel, bool ok) { Box(r, sel ? Color{80, 64, 110, 255} : (ok ? cPanel2 : Color{28, 22, 36, 255}), sel ? cGold : cEdge); }

static void DrawBuildingInfoLine(int def, int y) {
  const auto& D = gd::Data();
  if (def == sim::kTreasureDef) { Text(Tr("Treasure Box: +25% income, each next gives 15% less", "Ящик сокровищ: +25% дохода, каждый следующий на 15% меньше"), 4, y, cDim); return; }
  const auto& b = D.buildings[def]; char buf[200];
  if (b.isTower) snprintf(buf, sizeof buf, "%s  HP%d  %d dps  rng%d", b.name, b.hp, b.towDps, (b.towRange + 255) / 256);
  else { const auto& u = D.units[b.unit]; snprintf(buf, sizeof buf, "%s: %s HP%d %ddps r%d A%d", b.name, u.name, u.hp, u.dps, u.rangeWc3 <= 150 ? 1 : (u.rangeWc3 + 255) / 256, u.armor); }
  Text(buf, 4, y, cText);
}

static void DrawGameUI() {
  sim::Sim& s = *M.s; const auto& D = gd::Data(); sim::Player& me = s.players[0];
  char buf[96];
  // ---- HUD
  DrawRectangle(0, 0, gW, kHudH, cPanel); DrawRectangle(0, kHudH - 1, gW, 1, cEdge);
  DrawTexture(A.coin, 3, 2, WHITE); snprintf(buf, sizeof buf, "%d", me.gold); Text(buf, 14, 2, cGold);
  DrawTexture(A.wood, 58, 2, WHITE); snprintf(buf, sizeof buf, "%d", me.lumber); Text(buf, 69, 2, {200, 160, 110, 255});
  snprintf(buf, sizeof buf, "T%d", me.tech); Text(buf, 108, 2, cBlue);
  snprintf(buf, sizeof buf, "%d : %d", M.score[0], M.score[1]); Text(buf, 150, 2, cText);
  int gain = (int)((int64_t)me.income1000 * s.IncomeMultPermille(0) / 1000000);
  snprintf(buf, sizeof buf, "+%d", gain);
  Bar(3, 14, 100, 6, 1.f - (float)s.NextIncomeIn() / cfg::kIncomeTicks, {200, 160, 50, 255}); Text(buf, 108, 12, cGold);
  Text(Tr("rnd", "раунд"), 150, 12, cDim); snprintf(buf, sizeof buf, "%d", M.round); Text(buf, 150 + TextW(Tr("rnd", "раунд")) + 4, 12, cDim);
  if (Button({gW - 24.f, 3, 20, 18}, "II")) M.paused = true;

  // ---- bottom panel
  int py = gH - kPanelH; DrawRectangle(0, py, gW, kPanelH, cPanel); DrawRectangle(0, py, gW, 1, cEdge);
  const sim::Building* sb = M.selBld >= 0 ? s.FindBuilding(M.selBld) : nullptr;
  if (M.selBld >= 0 && !sb) M.selBld = -1;
  if (sb) {                                                 // selected building: info + upgrades
    const auto& bd = D.buildings[sb->def];
    snprintf(buf, sizeof buf, "%s  HP %d/%d", bd.name, sb->hp, sb->maxHp); Text(buf, 4, py + 4, cGold);
    if (!bd.isTower) { const auto& u = D.units[bd.unit]; snprintf(buf, sizeof buf, "%s  HP%d  %d dps  armor %d", u.name, u.hp, u.dps, u.armor); Text(buf, 4, py + 16, cText);
      Wrapped(u.abilities, 4, py + 28, gW - 8, cDim, 2); }
    int bx = 4; bool any = false;
    for (int to = 0; to < D.buildingCount; to++) if (D.buildings[to].upgradeFrom == sb->def) {
      any = true; bool ok = s.CanUpgrade(0, sb->id, to);
      snprintf(buf, sizeof buf, "%s %d", D.buildings[to].name, D.buildings[to].gold);
      if (D.buildings[to].tech > me.tech) snprintf(buf, sizeof buf, "T%d %s", D.buildings[to].tech, D.buildings[to].name);
      int w = std::min(150, TextW(buf) + 10);
      if (Button({(float)bx, (float)(py + kPanelH - 18), (float)w, 15}, buf, ok, ok ? Color{70, 90, 50, 255} : Color{0, 0, 0, 0})) { sim::Cmd c; c.type = sim::CMD_UPGRADE; c.player = 0; c.a = (int16_t)sb->id; c.b = (int16_t)to; s.Push(c); }
      bx += w + 4;
    }
    if (!any) Text(Tr("No upgrades", "Нет улучшений"), 4, py + kPanelH - 14, cDim);
    if (Button({gW - 24.f, (float)(py + kPanelH - 18), 20, 15}, "X")) M.selBld = -1;
    return;
  }
  const char* tabs[3] = {Tr("Build", "Строить"), Tr("Shop", "Магазин"), Tr("Tech", "Тех")};
  for (int i = 0; i < 3; i++) { Rectangle r = {3.f + i * 60, (float)py + 2, 58, 11}; bool act = M.tab == i;
    Box(r, act ? cPanel2 : cPanel, act ? cGold : cEdge); Text(tabs[i], (int)(r.x + 29), (int)r.y + 1, act ? cGold : cDim, 1);
    if (In.tap && Inside(r, In.press)) { M.tab = i; M.selDef = -1; synth::Play(synth::CLICK); } }
  int cy = py + 16;
  if (M.tab == 0) {
    std::vector<int> defs; defs.push_back(sim::kTreasureDef);
    for (int d = 0; d < D.buildingCount; d++) if (D.buildings[d].race == me.race && D.buildings[d].upgradeFrom < 0) defs.push_back(d);
    float total = (float)defs.size() * 41, view = (float)gW; M.scroll = std::max(0.f, std::min(M.scroll, std::max(0.f, total - view + 4)));
    if (In.down && Inside({0, (float)cy, (float)gW, 44}, In.press) && std::fabs(In.pos.x - In.press.x) > 5) M.scroll -= In.delta.x;
    BeginScissorMode(0, cy, gW, 44);
    for (size_t i = 0; i < defs.size(); i++) {
      int d = defs[i]; Rectangle r = {3 + i * 41 - M.scroll, (float)cy, 38, 44};
      bool tech = d == sim::kTreasureDef || D.buildings[d].tech <= me.tech;
      bool afford = me.gold >= s.BuildCostGold(d) && me.lumber >= s.BuildCostLumber(d);
      DrawCard(r, M.selDef == d, tech && afford);
      Texture2D t = d == sim::kTreasureDef ? A.treasure : A.building[d]; int sz = s.BuildSize(d);
      Color tint = tech ? WHITE : Color{70, 70, 90, 255};
      if (sz == 1) DrawTexturePro(t, {0, 0, 16, 16}, {r.x + 3, r.y + 2, 32, 32}, {0, 0}, 0, tint); else DrawTexture(t, (int)r.x + 3, (int)r.y + 2, tint);
      snprintf(buf, sizeof buf, "%d", s.BuildCostGold(d)); Text(buf, (int)r.x + 19, (int)r.y + 34, afford ? cGold : cRed, 1);
      if (!tech) { snprintf(buf, sizeof buf, "T%d", D.buildings[d].tech); Text(buf, (int)r.x + 4, (int)r.y + 3, cRed); }
      else if (s.BuildCostLumber(d) > 0) { snprintf(buf, sizeof buf, "%dL", s.BuildCostLumber(d)); Text(buf, (int)r.x + 3, (int)r.y + 3, {200, 160, 110, 255}); }
      if (In.tap && Inside(r, In.press) && Inside({0, (float)cy, (float)gW, 44}, In.press)) { M.selDef = (M.selDef == d) ? -1 : d; synth::Play(synth::CLICK); }
    }
    EndScissorMode();
    if (M.selDef >= 0) DrawBuildingInfoLine(M.selDef, py + kPanelH - 11);
    else Text(Tr("Pick a building, then tap your area", "Выбери здание и нажми на свою зону"), 4, py + kPanelH - 11, cDim);
  } else if (M.tab == 1) {
    for (int i = 0; i < sim::ITEM_COUNT; i++) {
      Rectangle r = {3.f + i * 53, (float)cy, 50, 44}; bool can = s.CanBuy(0, i); bool owned = (i == sim::ITEM_DNB && s.buff[0].dnb) || (i == sim::ITEM_BASS && s.buff[0].bass);
      DrawCard(r, false, can);
      Text(ShopAbbr[i], (int)r.x + 25, (int)r.y + 6, owned ? cGreen : cText, 1);
      snprintf(buf, sizeof buf, "%d", sim::GetItem(i).gold); Text(buf, (int)r.x + 25, (int)r.y + 30, can ? cGold : cRed, 1);
      int left = i == sim::ITEM_DOUBLE ? s.buff[0].doubleLeft : i == sim::ITEM_QUAD ? s.buff[0].quadLeft : i == sim::ITEM_SCROLL ? s.buff[0].scrollLeft : (owned ? cfg::kBuffDurationTicks : 0);
      if (left > 0) Bar((int)r.x + 4, (int)r.y + 20, 42, 3, (float)left / cfg::kBuffDurationTicks, cGreen);
      if (In.tap && Inside(r, In.press)) { if (can) { sim::Cmd c; c.type = sim::CMD_BUY; c.player = 0; c.a = (int16_t)i; s.Push(c); } else synth::Play(synth::DENY); }
      if (Inside(r, In.pos) && In.down) Wrapped(gSettings.lang == 1 && gCyr ? sim::GetItem(i).nameRu : sim::GetItem(i).name, 4, py + kPanelH - 11, gW - 8, cText, 1);
    }
    Text(Tr("Hold an item to see its name", "Удерживай предмет для названия"), 4, py + kPanelH - 11, cDim);
  } else {
    int t = me.tech; bool can = s.CanTech(0);
    if (t < 3) { snprintf(buf, sizeof buf, "%s %d  (%dg %dL)", Tr("Research tech level", "Исследовать уровень"), t + 1, D.c.techGold[t], D.c.techLumber[t]);
      if (Button({6, (float)cy, gW - 12.f, 20}, buf, can, Color{50, 80, 120, 255})) { sim::Cmd c; c.type = sim::CMD_TECH; c.player = 0; s.Push(c); } }
    else Text(Tr("Max tech level reached", "Максимальный уровень технологий"), 6, cy + 4, cGreen);
    snprintf(buf, sizeof buf, "%s x%.2f   (%s: %d)", Tr("Income", "Доход"), s.IncomeMultPermille(0) / 1000.f, Tr("buildings", "зданий"), me.builds); Text(buf, 6, cy + 26, cText);
    Text(Tr("Higher tech unlocks stronger buildings", "Новые уровни открывают сильные здания"), 6, cy + 38, cDim);
  }
}

static void HandleMapInput() {
  sim::Sim& s = *M.s; int ox = MapX(), oy = MapY();
  Rectangle mapArea = {0, (float)kHudH, (float)gW, (float)(gH - kHudH - kPanelH)};
  int tx = (int)std::floor((In.pos.x - ox) / 16.f), ty = (int)std::floor((In.pos.y - oy) / 16.f);
  if (M.selDef >= 0) { int sz = s.BuildSize(M.selDef); M.ghostX = tx - (sz - 1) / 2; M.ghostY = ty - (sz - 1) / 2; }
  if (!In.tap || !Inside(mapArea, In.press)) return;
  if (M.selDef >= 0) {
    if (s.CanBuild(0, M.selDef, M.ghostX, M.ghostY)) { sim::Cmd c; c.type = sim::CMD_BUILD; c.player = 0; c.a = (int16_t)M.selDef; c.b = (int16_t)M.ghostX; c.c = (int16_t)M.ghostY; s.Push(c); }
    else synth::Play(synth::DENY);
  } else {
    const sim::Building* b = s.BuildingAt(tx, ty);
    M.selBld = (b && !b->castle && !b->treasure && b->player == 0) ? b->id : -1;
  }
}

// ============================================================ screens
static std::vector<int> gFly;   // menu butterflies
static void DrawBackdrop(bool fireflies) {
  ClearBackground(cBg);
  for (int y = 0; y < gH; y += 16) for (int x = 0; x < gW; x += 16) { uint32_t h = (uint32_t)(x * 73856093 ^ y * 19349663); if (h % 5 == 0) DrawRectangle(x + (int)(h >> 8) % 14, y + (int)(h >> 12) % 14, 2, 2, {30, 24, 44, 255}); }
  if (!fireflies) return;
  for (int i = 0; i < 14; i++) {
    float t = gTime * (0.25f + i * 0.03f) + i * 1.7f; int x = (int)(gW / 2 + sinf(t * 0.9f + i) * (gW / 2 - 10) * (0.4f + 0.04f * i)), y = (int)(60 + fmodf(i * 53.f + t * 9.f, (float)gH - 80));
    if (i % 3 == 0) DrawTexture(A.butterfly[((int)(gTime * 6 + i)) & 1], x, y, WHITE);
    else DrawRectangle(x, y, 2, 2, {250, 240, 150, (unsigned char)(120 + 100 * sinf(gTime * 3 + i))});
  }
}
static void Header(const char* title, Scr back) {
  DrawRectangle(0, 0, gW, 22, cPanel); DrawRectangle(0, 21, gW, 1, cEdge); Text(title, gW / 2, 6, cGold, 1);
  if (Button({3, 3, 26, 16}, "<") || IsKeyPressed(KEY_BACK) || IsKeyPressed(KEY_ESCAPE)) { gScr = back; WriteSave(); }
}
static bool RaceUnlocked(int r) { return (gProgress.unlockedRaces >> r) & 1; }
static const char* RaceName(int r) { const auto& d = gd::Data().races[r]; return (gSettings.lang == 1 && gCyr) ? d.nameRu : d.name; }

static void ScreenMenu() {
  DrawBackdrop(true);
  int cx = gW / 2; float bob = sinf(gTime * 1.5f) * 2;
  DrawTexturePro(A.castle[0], {0, 0, 64, 32}, {(float)(cx - 64), 40 + bob, 128, 64}, {0, 0}, 0, WHITE);
  int y = 150; float w = 170, h = 26;
  if (Button({cx - w / 2, (float)y, w, h}, Tr("Campaign", "Кампания"))) gScr = S_CAMPAIGN; y += 34;
  if (Button({cx - w / 2, (float)y, w, h}, Tr("Quick match", "Быстрый матч"))) gScr = S_SKIRMISH; y += 34;
  if (Button({cx - w / 2, (float)y, w, h}, Tr("Races", "Расы"))) gScr = S_RACES; y += 34;
  Button({cx - w / 2, (float)y, w, h}, Tr("LAN game (soon)", "По сети (скоро)"), false); y += 34;
  if (Button({cx - w / 2, (float)y, w, h}, Tr("Settings", "Настройки"))) gScr = S_SETTINGS;
  char buf[64]; snprintf(buf, sizeof buf, "%s %d   %s %d", Tr("Won", "Побед"), gProgress.matchesWon, Tr("Lost", "Поражений"), gProgress.matchesLost);
  Text(buf, cx, gH - 14, cDim, 1);
}

static void ScreenCampaign() {
  DrawBackdrop(false); Header(Tr("Campaign", "Кампания"), S_MENU);
  for (int i = 0; i < kStageCount; i++) {
    const Stage& st = kStages[i]; bool done = (gProgress.campaignDone >> i) & 1;
    bool open = i == 0 || ((gProgress.campaignDone >> (i - 1)) & 1);
    bool raceOk = RaceUnlocked(st.myRace); Rectangle r = {8, 32.f + i * 52, gW - 16.f, 46};
    Box(r, open ? cPanel2 : Color{28, 22, 36, 255}, done ? cGreen : cEdge);
    char buf[128]; snprintf(buf, sizeof buf, "%d. %s", i + 1, (gSettings.lang == 1 && gCyr) ? st.nameRu : st.name); Text(buf, 14, (int)r.y + 4, open ? cGold : cDim);
    snprintf(buf, sizeof buf, "%s vs %s - %s", RaceName(st.myRace), RaceName(st.oppRace), LevelName(st.lvl)); Text(buf, 14, (int)r.y + 17, cText);
    if (st.rewardRace >= 0) { snprintf(buf, sizeof buf, "%s: %s", Tr("Reward", "Награда"), RaceName(st.rewardRace)); Text(buf, 14, (int)r.y + 30, cBlue); }
    else Text(done ? Tr("Cleared", "Пройдено") : Tr("Cleared stages unlock more", "Дальше — новые расы"), 14, (int)r.y + 30, done ? cGreen : cDim);
    if (Button({r.x + r.width - 52, r.y + 12, 46, 22}, open && raceOk ? Tr("Play", "Играть") : "?", open && raceOk)) StartMatch(st.map, st.myRace, st.oppRace, st.lvl, i);
  }
}

static int sk_my = 0, sk_opp = 1, sk_map = 0, sk_lvl = 1;
static void ScreenSkirmish() {
  DrawBackdrop(false); Header(Tr("Quick match", "Быстрый матч"), S_MENU);
  const auto& D = gd::Data(); char buf[96]; int y = 34;
  Text(Tr("Your race", "Твоя раса"), 10, y, cDim); y += 12;
  for (int r = 0; r < D.raceCount; r++) {
    bool un = RaceUnlocked(r); Rectangle b = {10.f + r * 125, (float)y, 120, 22};
    Box(b, sk_my == r ? Color{70, 56, 100, 255} : cPanel2, sk_my == r ? cGold : cEdge);
    DrawRectangle((int)b.x + 4, (int)b.y + 6, 10, 10, {D.races[r].color.r, D.races[r].color.g, D.races[r].color.b, 255});
    Text(un ? RaceName(r) : Tr("Locked", "Закрыто"), (int)b.x + 18, (int)b.y + 6, un ? cText : cDim);
    if (In.tap && Inside(b, In.press) && un) { sk_my = r; synth::Play(synth::CLICK); }
  }
  y += 30; Text(Tr("Opponent race", "Раса соперника"), 10, y, cDim); y += 12;
  for (int r = 0; r < D.raceCount; r++) {
    Rectangle b = {10.f + r * 125, (float)y, 120, 22}; Box(b, sk_opp == r ? Color{70, 56, 100, 255} : cPanel2, sk_opp == r ? cGold : cEdge);
    DrawRectangle((int)b.x + 4, (int)b.y + 6, 10, 10, {D.races[r].color.r, D.races[r].color.g, D.races[r].color.b, 255}); Text(RaceName(r), (int)b.x + 18, (int)b.y + 6, cText);
    if (In.tap && Inside(b, In.press)) { sk_opp = r; synth::Play(synth::CLICK); }
  }
  y += 32; snprintf(buf, sizeof buf, "%s: %s", Tr("Map", "Карта"), sim::GetMap(sk_map).name); if (Button({10, (float)y, gW - 20.f, 20}, buf)) sk_map = (sk_map + 1) % sim::MapCount();
  y += 26; snprintf(buf, sizeof buf, "%s: %s", Tr("Bot", "Бот"), LevelName((ai::Level)sk_lvl)); if (Button({10, (float)y, gW - 20.f, 20}, buf)) sk_lvl = (sk_lvl + 1) % 4;
  y += 26; snprintf(buf, sizeof buf, "%s: %d", Tr("Rounds to win", "Раундов до победы"), gSettings.wins); if (Button({10, (float)y, gW - 20.f, 20}, buf)) gSettings.wins = gSettings.wins % 3 + 1;
  y += 34; if (Button({30, (float)y, gW - 60.f, 28}, Tr("Start", "Старт"), true, Color{60, 100, 50, 255})) StartMatch(sk_map, sk_my, sk_opp, (ai::Level)sk_lvl, -1);
}

static int rc_sel = 0; static float rc_scroll = 0;
static void ScreenRaces() {
  DrawBackdrop(false); Header(Tr("Races", "Расы"), S_MENU);
  const auto& D = gd::Data();
  for (int r = 0; r < D.raceCount; r++) { Rectangle b = {6.f + r * 130, 26, 126, 16}; Box(b, rc_sel == r ? cPanel2 : cPanel, rc_sel == r ? cGold : cEdge); Text(RaceName(r), (int)b.x + 63, (int)b.y + 3, RaceUnlocked(r) ? cText : cDim, 1); if (In.tap && Inside(b, In.press)) { rc_sel = r; rc_scroll = 0; } }
  if (!RaceUnlocked(rc_sel)) { Text(Tr("Locked: clear the campaign to unlock", "Закрыто: пройди кампанию"), gW / 2, 80, cDim, 1); return; }
  Wrapped((gSettings.lang == 1 && gCyr) ? D.races[rc_sel].blurbRu : D.races[rc_sel].blurb, 8, 48, gW - 16, cDim, 2);
  std::vector<int> list; for (int b = 0; b < D.buildingCount; b++) if (D.buildings[b].race == rc_sel) list.push_back(b);
  float total = (float)list.size() * 58, view = (float)(gH - 74); rc_scroll = std::max(0.f, std::min(rc_scroll, std::max(0.f, total - view)));
  if (In.down && In.press.y > 70 && std::fabs(In.pos.y - In.press.y) > 4) rc_scroll -= In.delta.y;
  BeginScissorMode(0, 70, gW, (int)view);
  for (size_t i = 0; i < list.size(); i++) {
    const auto& b = D.buildings[list[i]]; float y = 72 + i * 58 - rc_scroll; Box({6, y, gW - 12.f, 54}, cPanel2, cEdge);
    if (b.size == 1) DrawTexturePro(A.building[list[i]], {0, 0, 16, 16}, {10, y + 10, 32, 32}, {0, 0}, 0, WHITE); else DrawTexture(A.building[list[i]], 10, (int)y + 10, WHITE);
    char buf[160]; snprintf(buf, sizeof buf, "%s  %dg%s", b.name, b.gold, b.lumber ? " +lumber" : ""); Text(buf, 48, (int)y + 3, cGold);
    if (!b.isTower) { const auto& u = D.units[b.unit]; snprintf(buf, sizeof buf, "%s HP%d %ddps rng%d arm%d T%d", u.name, u.hp, u.dps, u.rangeWc3 <= 150 ? 1 : (u.rangeWc3 + 255) / 256, u.armor, b.tech); Text(buf, 48, (int)y + 15, cText);
      Wrapped(u.abilities, 48, (int)y + 27, gW - 60, cDim, 2); }
    else { snprintf(buf, sizeof buf, "HP%d %ddps rng%d T%d", b.hp, b.towDps, (b.towRange + 255) / 256, b.tech); Text(buf, 48, (int)y + 15, cText); }
  }
  EndScissorMode();
}

static void ScreenSettings() {
  DrawBackdrop(false); Header(Tr("Settings", "Настройки"), S_MENU);
  struct Row { const char* en; const char* ru; bool* v; };
  Row rows[] = {{"Sound effects", "Звуковые эффекты", &gSettings.sound}, {"Music", "Музыка", &gSettings.music}, {"Vibration", "Вибрация", &gSettings.vib},
                {"  on castle hit", "  при ударе по замку", &gSettings.vibCastle}, {"  on win / defeat", "  при победе / поражении", &gSettings.vibEnd}, {"  on button taps", "  при нажатиях", &gSettings.vibUi}};
  int y = 34;
  for (auto& r : rows) {
    char buf[96]; snprintf(buf, sizeof buf, "%s: %s", Tr(r.en, r.ru), *r.v ? Tr("ON", "ВКЛ") : Tr("OFF", "ВЫКЛ"));
    if (Button({10, (float)y, gW - 20.f, 22}, buf, true, *r.v ? Color{50, 80, 50, 255} : Color{0, 0, 0, 0})) { *r.v = !*r.v; if (r.v == &gSettings.vib && *r.v) plat::Vibrate(60); }
    y += 28;
  }
  char buf[96]; snprintf(buf, sizeof buf, "%s: %d", Tr("Rounds to win", "Раундов до победы"), gSettings.wins); if (Button({10, (float)y, gW - 20.f, 22}, buf)) gSettings.wins = gSettings.wins % 3 + 1; y += 28;
  snprintf(buf, sizeof buf, "%s: %s", Tr("Language", "Язык"), gSettings.lang == 0 ? "English" : "Russian");
  if (Button({10, (float)y, gW - 20.f, 22}, buf, gCyr || gSettings.lang == 1)) gSettings.lang = 1 - gSettings.lang; y += 28;
  if (!gCyr) Wrapped("Russian needs a pixel font with Cyrillic: put font.ttf into the app assets (see README).", 10, y + 2, gW - 20, cDim, 3);
}

static void FinishRound() {
  sim::Sim& s = *M.s; int w = s.winner; M.score[w]++;
  M.matchOver = M.score[w] >= M.winsNeeded; M.iWonMatch = M.matchOver && w == 0;
  if (M.matchOver) {
    if (M.iWonMatch) { gProgress.matchesWon++; if (M.stage >= 0) { gProgress.campaignDone |= 1u << M.stage; if (kStages[M.stage].rewardRace >= 0) gProgress.unlockedRaces |= 1u << kStages[M.stage].rewardRace; } }
    else gProgress.matchesLost++;
    WriteSave();
  }
  bool iWon = w == 0; synth::Play(iWon ? synth::WIN : synth::LOSE); Haptic(iWon ? 250 : 450, gSettings.vibEnd);
  gScr = S_RESULT;
}

static void ScreenGame(float dt) {
  sim::Sim& s = *M.s;
  if (IsKeyPressed(KEY_BACK) || IsKeyPressed(KEY_ESCAPE)) M.paused = !M.paused;
  if (!IsWindowFocused()) M.paused = true;                      // app minimised: freeze the match
  if (!M.paused && s.winner < 0) {
    M.acc += std::min(dt, 0.1f);
    const float step = 1.f / cfg::kTickHz;
    while (M.acc >= step && s.winner < 0) { M.bot->Think(s); s.Step(); M.acc -= step; HandleEvents(); }
  }
  UpdateParts();
  float alpha = M.paused ? 0.f : M.acc * cfg::kTickHz;
  DrawBackdrop(false);
  DrawWorld(alpha);
  if (!M.paused) HandleMapInput();
  DrawGameUI();
  if (M.paused) {
    DrawRectangle(0, 0, gW, gH, {0, 0, 0, 170}); Text(Tr("Paused", "Пауза"), gW / 2, gH / 2 - 50, cGold, 1);
    if (Button({gW / 2 - 70.f, (float)gH / 2 - 30, 140, 24}, Tr("Resume", "Продолжить"))) M.paused = false;
    if (Button({gW / 2 - 70.f, (float)gH / 2 + 2, 140, 24}, Tr("Surrender", "Сдаться"))) { s.winner = 1; FinishRound(); }
  }
  if (s.winner >= 0 && gScr == S_GAME) FinishRound();
}

static void ScreenResult() {
  DrawBackdrop(true); sim::Sim& s = *M.s; bool iWon = s.winner == 0;
  Text(iWon ? Tr("VICTORY", "ПОБЕДА") : Tr("DEFEAT", "ПОРАЖЕНИЕ"), gW / 2, 90, iWon ? cGold : cRed, 1);
  char buf[96]; snprintf(buf, sizeof buf, "%d : %d", M.score[0], M.score[1]); Text(buf, gW / 2, 112, cText, 1);
  snprintf(buf, sizeof buf, "%s %d:%02d", Tr("Time", "Время"), s.tick / cfg::kTickHz / 60, s.tick / cfg::kTickHz % 60); Text(buf, gW / 2, 130, cDim, 1);
  if (M.matchOver && M.iWonMatch && M.stage >= 0 && kStages[M.stage].rewardRace >= 0) { snprintf(buf, sizeof buf, "%s: %s", Tr("Unlocked", "Открыто"), RaceName(kStages[M.stage].rewardRace)); Text(buf, gW / 2, 150, cBlue, 1); }
  float y = 190;
  if (!M.matchOver) { if (Button({gW / 2 - 70.f, y, 140, 26}, Tr("Next round", "Следующий раунд"), true, Color{60, 100, 50, 255})) { M.round++; NewRound(); gScr = S_GAME; } y += 34; }
  else { if (Button({gW / 2 - 70.f, y, 140, 26}, Tr("Rematch", "Реванш"), true, Color{60, 100, 50, 255})) StartMatch(M.map, M.myRace, M.oppRace, M.lvl, M.stage); y += 34; }
  if (Button({gW / 2 - 70.f, y, 140, 26}, Tr("Menu", "Меню"))) { gScr = M.stage >= 0 ? S_CAMPAIGN : S_MENU; }
}

// ============================================================ boot
static void LoadFontIfAny() {
  gFont = GetFontDefault(); gFontPx = 10;
  if (!FileExists("font.ttf")) return;
  std::vector<int> cp; for (int c = 32; c < 127; c++) cp.push_back(c);
  for (int c = 0x410; c <= 0x44F; c++) cp.push_back(c); cp.push_back(0x401); cp.push_back(0x451);
  Font f = LoadFontEx("font.ttf", 8, cp.data(), (int)cp.size());
  if (f.texture.id) { gFont = f; gFontPx = 8; SetTextureFilter(f.texture, TEXTURE_FILTER_POINT); gCyr = true; }
}
static void ResizeTarget() {
  int sw = GetScreenWidth(), sh = GetScreenHeight();
  int h = (int)std::lround(gW * (double)sh / sw); h = std::max(480, std::min(640, h));
  if (!gRT.id || h != gH) { if (gRT.id) UnloadRenderTexture(gRT); gH = h; gRT = LoadRenderTexture(gW, gH); SetTextureFilter(gRT.texture, TEXTURE_FILTER_POINT); }
  float f = std::min((float)sw / gW, (float)sh / gH), i = std::floor(f);
  gScale = (i >= 1 && (f - i) / f < 0.08f) ? i : f;
  gOffX = std::floor((sw - gW * gScale) / 2); gOffY = std::floor((sh - gH * gScale) / 2);
}

int main() {
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
  InitWindow(405, 720, "");
  SetExitKey(0);
  SetTargetFPS(60);
  LoadSave(); LoadFontIfAny(); synth::Init(); art::Build(A);
  while (!WindowShouldClose()) {
    float dt = GetFrameTime(); gTime += dt; ResizeTarget(); ReadInput();
    synth::Update(gSettings.sound, gSettings.music, gScr == S_GAME);
    BeginTextureMode(gRT);
    switch (gScr) {
      case S_MENU: ScreenMenu(); break;
      case S_CAMPAIGN: ScreenCampaign(); break;
      case S_SKIRMISH: ScreenSkirmish(); break;
      case S_RACES: ScreenRaces(); break;
      case S_SETTINGS: ScreenSettings(); break;
      case S_GAME: ScreenGame(dt); break;
      case S_RESULT: ScreenResult(); break;
    }
    EndTextureMode();
    BeginDrawing(); ClearBackground(BLACK);
    DrawTexturePro(gRT.texture, {0, 0, (float)gW, -(float)gH}, {gOffX, gOffY, gW * gScale, gH * gScale}, {0, 0}, 0, WHITE);
    EndDrawing();
  }
  WriteSave(); synth::Shutdown(); art::Free(A); UnloadRenderTexture(gRT); CloseWindow();
  return 0;
}
