#include "sim.h"
#include <algorithm>
#include <climits>

namespace sim {

// ======================= items =======================
static const ItemDef kItems[ITEM_COUNT] = {
  {"Bassline Generator", "Генератор баса", 600, "+15% move speed, +30% attack speed for all allied units. Does not stack."},   // [map]
  {"Bass Drums",         "Басовые барабаны", 650, "+33% damage for all allied units. Does not stack."},                       // [map]
  {"Double Damage",      "Двойной урон", 600, "+100% damage for 30 s."},                                                     // [map]
  {"Quad Damage",        "Учетверённый урон", 1400, "+300% damage for 30 s."},                                              // [map]
  {"Scroll of Stone",    "Свиток камня", 150, "Heals 400 HP and grants +8 armor for 30 s to all allied units."},            // [map]
};
const ItemDef& GetItem(int i) { return kItems[i]; }

// ======================= maps =======================
// Top half is written for team 1 ('b', castle 'D'); the bottom half is mirrored for team 0 ('a', 'C').
static std::string Mirror(const char* const* top, int rows, int w) {
  std::string s;
  for (int i = 0; i < rows; i++) s += top[i];
  for (int i = rows - 1; i >= 0; i--) {
    std::string r = top[i];
    for (char& c : r) { if (c == 'b') c = 'a'; else if (c == 'D') c = 'C'; }
    s += r;
  }
  (void)w;
  return s;
}
static const char* kTop0[12] = {
  "#.....DDDD.....#", "#.....DDDD.....#", "#..............#",
  "#bbbbbb..bbbbbb#", "#bbbbbb..bbbbbb#", "#bbbbbb..bbbbbb#", "#bbbbbb..bbbbbb#",
  "#bbbbbb..bbbbbb#", "#bbbbbb..bbbbbb#", "#bbbbbb..bbbbbb#",
  "#..............#", "#...##....##...#"};
static const char* kTop1[12] = {
  "##....DDDD....##", "#.....DDDD.....#", "#....#....#....#",
  "#bbbbb....bbbbb#", "#bbbbb.##.bbbbb#", "#bbbbb....bbbbb#", "#bbbb#....#bbbb#",
  "#bbbbb....bbbbb#", "#bbbbb.##.bbbbb#", "#bbbbb....bbbbb#",
  "#..............#", "#..#...##...#..#"};
static MapDef* Maps() {
  static MapDef m[2] = {
    {"Crossing", 16, 24, Mirror(kTop0, 12, 16)},
    {"Ruins",    16, 24, Mirror(kTop1, 12, 16)},
  };
  return m;
}
int MapCount() { return 2; }
const MapDef& GetMap(int i) { return Maps()[i]; }

// ======================= damage tables (standard WC3) [base] =======================
//                          light medium heavy fort hero unarm divine
static const int kAtkVsArm[gd::ATK_COUNT][gd::ARM_COUNT] = {
  /*normal*/ {100, 150, 100,  70, 100, 100,   5},
  /*pierce*/ {200,  75, 100,  35,  50, 150,   5},
  /*siege */ {100,  50, 100, 150,  50, 150,   5},
  /*magic */ {125,  75, 200,  35,  50, 100,   5},
  /*chaos */ {100, 100, 100, 100, 100, 100, 100},
  /*hero  */ {100, 100, 100,  50, 100, 100, 100},
};

int AtkMult(int a, int d) { return kAtkVsArm[a][d]; }

static const int DX[8] = {0, 1, 0, -1, 1, 1, -1, -1};
static const int DY[8] = {1, 0, -1, 0, 1, -1, 1, -1};

// ======================= construction =======================
Sim::Sim(int mapIndex, const int* races, const int* teams, int n, uint32_t seed) : rng_(seed ? seed : 1) {
  map = &GetMap(mapIndex); w = map->w; h = map->h;
  occ_.assign(w * h, -1); bocc_.assign(w * h, -1);
  const auto& D = gd::Data();
  for (int i = 0; i < n; i++) {
    Player p; p.race = races[i]; p.team = teams[i]; p.gold = cfg::kStartGold; p.lumber = cfg::kStartLumber;
    players.push_back(p);
  }
  for (int t = 0; t < 2; t++) {            // castles
    char ch = t == 0 ? 'C' : 'D';
    int x0 = w, y0 = h, x1 = -1, y1 = -1;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) if (map->at(x, y) == ch) { x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y); }
    Building b; b.id = nextId_++; b.castle = true; b.team = t; b.player = -1; b.x = x0; b.y = y0; b.w = x1 - x0 + 1; b.h = y1 - y0 + 1;
    b.hp = b.maxHp = D.c.castleHp;
    castleId[t] = b.id;
    buildings.push_back(b);
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) bocc_[y * w + x] = (int)buildings.size() - 1;
  }
  RebuildFields();
}

// ======================= queries =======================
int Sim::BuildSize(int d) const { return d == kTreasureDef ? 1 : gd::Data().buildings[d].size; }
int Sim::BuildCostGold(int d) const { return d == kTreasureDef ? gd::Data().c.treasureGold : gd::Data().buildings[d].gold; }
int Sim::BuildCostLumber(int d) const { return d == kTreasureDef ? 0 : gd::Data().buildings[d].lumber; }

const Building* Sim::BuildingAt(int x, int y) const {
  if (x < 0 || y < 0 || x >= w || y >= h) return nullptr;
  int i = bocc_[y * w + x];
  return i < 0 ? nullptr : &buildings[i];
}
const Building* Sim::FindBuilding(int id) const {
  for (auto& b : buildings) if (b.id == id && b.alive) return &b;
  return nullptr;
}
bool Sim::Walkable(int x, int y) const {
  if (x < 0 || y < 0 || x >= w || y >= h) return false;
  return map->at(x, y) != '#' && bocc_[y * w + x] < 0;
}

bool Sim::CanBuild(int pl, int d, int x, int y) const {
  if (winner >= 0 || pl < 0 || pl >= (int)players.size()) return false;
  const Player& p = players[pl];
  const auto& D = gd::Data();
  int gold, lumber;
  if (d == kTreasureDef) { gold = D.c.treasureGold; lumber = 0; }
  else {
    if (d < 0 || d >= D.buildingCount) return false;
    const auto& bd = D.buildings[d];
    if (bd.race != p.race || bd.upgradeFrom >= 0 || bd.tech > p.tech) return false;
    gold = bd.gold; lumber = bd.lumber;
  }
  if (p.gold < gold || p.lumber < lumber) return false;
  int s = BuildSize(d);
  char want = p.team == 0 ? 'a' : 'b';
  for (int yy = y; yy < y + s; yy++) for (int xx = x; xx < x + s; xx++) {
    if (xx < 0 || yy < 0 || xx >= w || yy >= h) return false;
    if (map->at(xx, yy) != want || bocc_[yy * w + xx] >= 0 || occ_[yy * w + xx] >= 0) return false;
  }
  return true;
}

bool Sim::CanUpgrade(int pl, int bid, int to) const {
  if (winner >= 0 || pl < 0 || pl >= (int)players.size()) return false;
  const Building* b = FindBuilding(bid);
  const auto& D = gd::Data();
  if (!b || b->castle || b->treasure || b->player != pl || b->buildLeft > 0 || to < 0 || to >= D.buildingCount) return false;
  const auto& td = D.buildings[to];
  const Player& p = players[pl];
  return td.upgradeFrom == b->def && td.race == p.race && td.tech <= p.tech && p.gold >= td.gold && p.lumber >= td.lumber;
}
bool Sim::CanTech(int pl) const {
  const Player& p = players[pl]; const auto& c = gd::Data().c;
  return winner < 0 && p.tech < 3 && p.gold >= c.techGold[p.tech] && p.lumber >= c.techLumber[p.tech];
}
bool Sim::CanBuy(int pl, int it) const {
  if (winner >= 0 || it < 0 || it >= ITEM_COUNT) return false;
  const Player& p = players[pl]; const TeamBuff& tb = buff[p.team];
  if (p.gold < kItems[it].gold) return false;
  if (it == ITEM_DNB && tb.dnb) return false;
  if (it == ITEM_BASS && tb.bass) return false;
  return true;
}
int Sim::IncomeMultPermille(int pl) const {
  int m = 1000, add = cfg::kTreasureFirst1000;
  for (int i = 0; i < players[pl].treasure; i++) { m += add; add = add * cfg::kTreasureDecay100 / 100; }
  return m;
}
int Sim::RangeTiles(int r) const {
  if (r <= cfg::kMeleeMaxRange) return 1;
  return (r + cfg::kTileWc3 - 1) / cfg::kTileWc3;
}
int Sim::BuildingDistance(const Building& b, int x, int y) const {
  int dx = x < b.x ? b.x - x : (x > b.x + b.w - 1 ? x - (b.x + b.w - 1) : 0);
  int dy = y < b.y ? b.y - y : (y > b.y + b.h - 1 ? y - (b.y + b.h - 1) : 0);
  return std::max(dx, dy);
}
void Sim::UnitPx(const Unit& u, float alpha, float& px, float& py) const {
  float f = 0;
  if (u.moving) { f = (u.mp + alpha * u.speed) / (float)u.mpNeed; if (f > 1) f = 1; }
  px = (u.x + (u.tx - u.x) * f) * cfg::kTile;
  py = (u.y + (u.ty - u.y) * f) * cfg::kTile;
}

// ======================= pathing fields =======================
void Sim::RebuildFields() {
  fieldsDirty_ = false;
  for (int t = 0; t < 2; t++) {
    auto& f = field_[t]; f.assign(w * h, -1);
    const Building& enemy = buildings[1 - t];                 // buildings[0]/[1] are the castles
    std::vector<int> q;
    for (int y = enemy.y - 1; y <= enemy.y + enemy.h; y++) for (int x = enemy.x - 1; x <= enemy.x + enemy.w; x++)
      if (Walkable(x, y) && f[y * w + x] < 0) { f[y * w + x] = 0; q.push_back(y * w + x); }
    for (size_t qi = 0; qi < q.size(); qi++) {
      int cx = q[qi] % w, cy = q[qi] / w, cd = f[q[qi]];
      for (int k = 0; k < 8; k++) {
        int nx = cx + DX[k], ny = cy + DY[k];
        if (!Walkable(nx, ny) || f[ny * w + nx] >= 0) continue;
        if (k >= 4 && (!Walkable(cx + DX[k], cy) || !Walkable(cx, cy + DY[k]))) continue;
        f[ny * w + nx] = cd + 1; q.push_back(ny * w + nx);
      }
    }
  }
}

// ======================= commands =======================
void Sim::Apply(const Cmd& c) {
  const auto& D = gd::Data();
  if (c.player >= players.size()) return;
  Player& p = players[c.player];
  switch (c.type) {
    case CMD_BUILD: {
      if (!CanBuild(c.player, c.a, c.b, c.c)) return;
      Building b; b.id = nextId_++; b.player = c.player; b.team = p.team; b.x = c.b; b.y = c.c;
      int gold, lumber;
      if (c.a == kTreasureDef) { b.def = -1; b.treasure = true; b.w = b.h = 1; b.hp = b.maxHp = D.c.treasureHp; gold = D.c.treasureGold; lumber = 0; p.treasure++; }
      else {
        const auto& bd = D.buildings[c.a];
        b.def = c.a; b.w = b.h = bd.size; b.hp = b.maxHp = bd.hp; gold = bd.gold; lumber = bd.lumber;
        if (bd.unit >= 0) b.spawnLeft = D.units[bd.unit].trainDeci * cfg::kTickHz / 10;
        p.income1000 += gold * 1000 / cfg::kIncomePerCostDiv;
      }
      b.buildLeft = cfg::kBuildTicks;
      p.gold -= gold; p.lumber -= lumber;
      if (p.builds < 4) p.gold += cfg::kEarlyBonus[p.builds];
      p.builds++;
      buildings.push_back(b);
      int bi = (int)buildings.size() - 1;
      for (int y = b.y; y < b.y + b.h; y++) for (int x = b.x; x < b.x + b.w; x++) bocc_[y * w + x] = bi;
      fieldsDirty_ = true;
      Emit(EV_BUILD, b.x * cfg::kTile + b.w * 8, b.y * cfg::kTile + b.h * 8, c.player);
      break;
    }
    case CMD_UPGRADE: {
      if (!CanUpgrade(c.player, c.a, c.b)) return;
      Building* b = nullptr; for (auto& q : buildings) if (q.id == c.a) b = &q;
      const auto& td = D.buildings[c.b];
      p.gold -= td.gold; p.lumber -= td.lumber;
      b->def = c.b; b->hp = b->maxHp = td.hp; b->buildLeft = cfg::kBuildTicks / 2;
      if (td.unit >= 0) b->spawnLeft = D.units[td.unit].trainDeci * cfg::kTickHz / 10;
      p.income1000 += td.gold * 1000 / cfg::kIncomePerCostDiv;
      Emit(EV_UPGRADE, b->x * cfg::kTile + b->w * 8, b->y * cfg::kTile + b->h * 8, c.player);
      break;
    }
    case CMD_TECH:
      if (!CanTech(c.player)) return;
      p.gold -= D.c.techGold[p.tech]; p.lumber -= D.c.techLumber[p.tech]; p.tech++;
      Emit(EV_TECH, 0, 0, c.player, p.tech);
      break;
    case CMD_BUY: {
      if (!CanBuy(c.player, c.a)) return;
      p.gold -= kItems[c.a].gold; TeamBuff& tb = buff[p.team];
      switch (c.a) {
        case ITEM_DNB: tb.dnb = true; break;
        case ITEM_BASS: tb.bass = true; break;
        case ITEM_DOUBLE: tb.doubleLeft = cfg::kBuffDurationTicks; break;
        case ITEM_QUAD: tb.quadLeft = cfg::kBuffDurationTicks; break;
        case ITEM_SCROLL: tb.scrollLeft = cfg::kBuffDurationTicks; for (auto& u : units) if (u.alive && u.team == p.team) u.hp = std::min(u.maxHp, u.hp + 400); break;
      }
      Emit(EV_BUY, 0, 0, c.player, c.a);
      break;
    }
    default: break;
  }
}

// ======================= tick =======================
void Sim::Step() {
  if (winner >= 0) return;
  tick++;
  for (auto& c : pending_) Apply(c);
  pending_.clear();
  if (fieldsDirty_) RebuildFields();
  for (int t = 0; t < 2; t++) {
    TeamBuff& b = buff[t];
    if (b.doubleLeft > 0) b.doubleLeft--;
    if (b.quadLeft > 0) b.quadLeft--;
    if (b.scrollLeft > 0) b.scrollLeft--;
  }
  StepEconomy();
  StepBuildings();
  StepUnits();
  ApplyHits();
  units.erase(std::remove_if(units.begin(), units.end(), [](const Unit& u) { return !u.alive; }), units.end());
  if (fieldsDirty_) RebuildFields();
  for (int t = 0; t < 2; t++) if (buildings[t].hp <= 0 && winner < 0) { winner = 1 - t; Emit(EV_ROUND_END, 0, 0, winner); }
}

void Sim::StepEconomy() {
  if (tick % cfg::kIncomeTicks != 0) return;
  for (size_t i = 0; i < players.size(); i++) {
    Player& p = players[i];
    int gain = (int)((int64_t)p.income1000 * IncomeMultPermille((int)i) / 1000000);
    p.gold += gain; p.lumber += cfg::kLumberPerCycle;
    Emit(EV_INCOME, 0, 0, (int)i, gain);
  }
}

int Sim::CalcDamage(int dps, int atk, int armType, int armor, int srcTeam) const {
  const TeamBuff& tb = buff[srcTeam];
  int pct = 100 + (tb.bass ? 33 : 0);
  int mult = tb.quadLeft > 0 ? 4 : (tb.doubleLeft > 0 ? 2 : 1);
  int64_t d = (int64_t)dps * pct / 100 * mult;
  d = d * kAtkVsArm[atk][armType] / 100;
  if (armor > 0) d = d * 100 / (100 + 6 * armor);
  return d < 1 ? 1 : (int)d;
}

bool Sim::TrySpawn(Building& b) {
  const auto& D = gd::Data();
  const auto& bd = D.buildings[b.def];
  int bestX = -1, bestY = -1, bestD = INT_MAX;
  for (int y = b.y - 1; y <= b.y + b.h; y++) for (int x = b.x - 1; x <= b.x + b.w; x++) {
    bool inside = x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h;
    if (inside || !Walkable(x, y) || occ_[y * w + x] >= 0) continue;
    int d = Dist(b.team, x, y);
    if (d >= 0 && d < bestD) { bestD = d; bestX = x; bestY = y; }
  }
  if (bestX < 0) return false;
  const auto& ud = D.units[bd.unit];
  Unit u; u.id = nextId_++; u.def = bd.unit; u.player = b.player; u.team = b.team; u.hp = u.maxHp = ud.hp;
  u.x = u.tx = bestX; u.y = u.ty = bestY; u.face = b.team == 0 ? 1 : -1;
  units.push_back(u);
  occ_[bestY * w + bestX] = u.id;
  Emit(EV_SPAWN, bestX * cfg::kTile + 8, bestY * cfg::kTile + 8, b.team);
  return true;
}

void Sim::ApplyHits() {
  for (auto& ht : hits_) {
    if (ht.unit >= 0) {
      Unit& t = units[ht.unit];
      if (!t.alive) continue;
      t.hp -= ht.dmg; t.flash = 4;
      if (t.hp <= 0) KillUnit(t, ht.src);
    } else {
      Building& b = buildings[ht.bld];
      if (!b.alive) continue;
      b.hp -= ht.dmg; b.flash = 4;
      if (b.hp <= 0 && !b.castle) {
        b.alive = false;
        for (int y = b.y; y < b.y + b.h; y++) for (int x = b.x; x < b.x + b.w; x++) bocc_[y * w + x] = -1;
        fieldsDirty_ = true;
        Emit(EV_BLDG_DEATH, b.x * cfg::kTile + b.w * 8, b.y * cfg::kTile + b.h * 8, b.team);
      }
    }
  }
  hits_.clear();
}

void Sim::StepBuildings() {
  const auto& D = gd::Data();
  for (size_t bi = 0; bi < buildings.size(); bi++) {
    Building& b = buildings[bi];
    if (!b.alive) continue;
    if (b.flash > 0) b.flash--;
    if (b.castle || b.treasure) continue;
    if (b.buildLeft > 0) { b.buildLeft--; continue; }
    const auto& bd = D.buildings[b.def];
    if (bd.isTower) {
      if (b.cd > 0) { b.cd--; continue; }
      int rt = RangeTiles(bd.towRange), best = INT_MAX, bu = -1;
      for (size_t k = 0; k < units.size(); k++) {
        const Unit& u = units[k];
        if (!u.alive || u.team == b.team) continue;
        int d = std::max(std::abs(u.x - b.x), std::abs(u.y - b.y));
        if (d <= rt && d < best) { best = d; bu = (int)k; }
      }
      if (bu >= 0) {
        Unit& t = units[bu]; const auto& ud = D.units[t.def];
        int armor = ud.armor + (buff[t.team].scrollLeft > 0 ? 8 : 0);
        hits_.push_back({bu, -1, CalcDamage(bd.towDps, bd.towAtk, ud.armType, armor, b.team), b.player});
        Emit(EV_SHOT, b.x * cfg::kTile + 8, b.y * cfg::kTile + 8, t.x * cfg::kTile + 8, t.y * cfg::kTile + 8);
        b.cd = cfg::kAtkIntervalTicks;
      }
      continue;
    }
    if (--b.spawnLeft <= 0) {
      if (TrySpawn(b)) b.spawnLeft = D.units[bd.unit].trainDeci * cfg::kTickHz / 10;
      else b.spawnLeft = 0;                                    // retry next tick
    }
  }
}

void Sim::KillUnit(Unit& u, int killerPlayer) {
  if (!u.alive) return;
  u.alive = false;
  occ_[u.y * w + u.x] = occ_[u.y * w + u.x] == u.id ? -1 : occ_[u.y * w + u.x];
  if (u.moving) occ_[u.ty * w + u.tx] = occ_[u.ty * w + u.tx] == u.id ? -1 : occ_[u.ty * w + u.tx];
  if (killerPlayer >= 0 && killerPlayer < (int)players.size()) players[killerPlayer].gold += std::max(2, u.maxHp / cfg::kBountyHpDiv);
  float px, py; UnitPx(u, 0.f, px, py);
  Emit(EV_DEATH, (int)px + 8, (int)py + 8, u.team, u.def);
}

void Sim::StepUnits() {
  const auto& D = gd::Data();
  const size_t n = units.size();
  for (size_t k = 0; k < n; k++) {
    size_t i = (tick & 1) ? n - 1 - k : k;                     // alternate order so neither side always acts first
    Unit& u = units[i];
    if (!u.alive) continue;
    if (u.flash > 0) u.flash--;
    if (u.cd > 0) u.cd--;
    const auto& ud = D.units[u.def];
    if (u.moving) {
      u.mp += u.speed;
      if (u.mp < u.mpNeed) continue;
      u.x = u.tx; u.y = u.ty; u.moving = false; u.mp = 0;
    }
    // --- acquire target ---
    int rt = RangeTiles(ud.rangeWc3);
    int bestD = INT_MAX; Unit* tu = nullptr; Building* tb = nullptr;
    for (auto& e : units) {
      if (!e.alive || e.team == u.team) continue;
      int d = std::max(std::abs(e.x - u.x), std::abs(e.y - u.y));
      if (d <= rt && d < bestD) { bestD = d; tu = &e; }
    }
    if (!tu) {
      for (auto& b : buildings) {
        if (!b.alive || b.team == u.team) continue;
        int d = BuildingDistance(b, u.x, u.y);
        if (d <= rt && d < bestD) { bestD = d; tb = &b; }
      }
    }
    if (tu || tb) {
      if (u.cd > 0) continue;
      int tx = tu ? tu->x : tb->x + tb->w / 2;
      if (tx != u.x) u.face = tx > u.x ? 1 : -1;
      int interval = cfg::kAtkIntervalTicks;
      if (buff[u.team].dnb) interval = interval * 100 / 130;
      u.cd = interval;
      if (tu) {
        const auto& td = D.units[tu->def];
        int armor = td.armor + (buff[tu->team].scrollLeft > 0 ? 8 : 0);
        hits_.push_back({(int)(tu - units.data()), -1, CalcDamage(ud.dps, ud.atkType, td.armType, armor, u.team), u.player});
        Emit(rt > 1 ? EV_SHOT : EV_HIT, u.x * cfg::kTile + 8, u.y * cfg::kTile + 8, tu->x * cfg::kTile + 8, tu->y * cfg::kTile + 8);
      } else {
        int armor = tb->castle ? cfg::kCastleArmor : cfg::kBuildingArmor;
        hits_.push_back({-1, (int)(tb - buildings.data()), CalcDamage(ud.dps, ud.atkType, gd::ARM_FORT, armor, u.team), u.player});
        if (tb->castle) Emit(EV_CASTLE_HIT, u.x * cfg::kTile + 8, u.y * cfg::kTile + 8, tb->team, rt > 1);
        else Emit(rt > 1 ? EV_SHOT : EV_HIT, u.x * cfg::kTile + 8, u.y * cfg::kTile + 8,
                  tb->x * cfg::kTile + tb->w * 8, tb->y * cfg::kTile + tb->h * 8);
      }
      continue;
    }
    // --- move along the flow field ---
    int cur = Dist(u.team, u.x, u.y);
    if (cur < 0) continue;
    int bx = -1, by = -1, bd = INT_MAX, bdiag = 0;
    for (int k = 0; k < 8; k++) {
      int nx = u.x + DX[k], ny = u.y + DY[k];
      if (!Walkable(nx, ny) || occ_[ny * w + nx] >= 0) continue;
      if (k >= 4 && (!Walkable(u.x + DX[k], u.y) || !Walkable(u.x, u.y + DY[k]))) continue;
      int d = Dist(u.team, nx, ny);
      if (d < 0) continue;
      int score = d * 4 + (k >= 4 ? 1 : 0);
      if (score < bd) { bd = score; bx = nx; by = ny; bdiag = k >= 4; }
    }
    if (bx < 0) { u.stuck++; continue; }
    int nd = Dist(u.team, bx, by);
    bool ok = nd < cur || (nd == cur && u.stuck >= 2) || (nd == cur + 1 && u.stuck >= 8);
    if (!ok) { u.stuck++; continue; }
    u.stuck = 0;
    occ_[u.y * w + u.x] = -1; occ_[by * w + bx] = u.id;
    u.tx = bx; u.ty = by; u.moving = true; u.mp = 0;
    u.mpNeed = bdiag ? cfg::kMoveMilliDiag : cfg::kMoveMilliPerTile;
    u.speed = ud.speedWc3 * 50 / cfg::kTileWc3;
    if (buff[u.team].dnb) u.speed = u.speed * 115 / 100;
    if (bx != u.x) u.face = bx > u.x ? 1 : -1;
  }
}

uint32_t Sim::Hash() const {
  uint32_t h = 2166136261u;
  auto mix = [&](uint32_t v) { h = (h ^ v) * 16777619u; };
  mix(tick);
  for (auto& p : players) { mix(p.gold); mix(p.lumber); mix(p.tech); }
  for (auto& u : units) { mix(u.id); mix(u.x); mix(u.y); mix(u.hp); mix(u.mp); }
  for (auto& b : buildings) { mix(b.id); mix(b.hp); mix(b.alive); }
  return h;
}

} // namespace sim
