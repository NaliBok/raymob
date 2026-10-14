#pragma once
// Deterministic fixed-step simulation. Integer maths only, no floats, no wall-clock access.
// The same code runs on the client (offline/bot), a future dedicated server and web/PC clients.
#include <cstdint>
#include <string>
#include <vector>
#include "../data/gamedata.h"
#include "config.h"

namespace sim {

enum CmdType : uint8_t { CMD_NONE, CMD_BUILD, CMD_UPGRADE, CMD_TECH, CMD_BUY, CMD_SELL };
constexpr int kTreasureDef = 1000;   // pseudo building id for the Treasure Box

struct Cmd {                 // 8 bytes on the wire; everything the net layer needs to ship
  CmdType type = CMD_NONE; uint8_t player = 0; int16_t a = 0, b = 0, c = 0;
};

enum Item : int { ITEM_DNB, ITEM_BASS, ITEM_DOUBLE, ITEM_QUAD, ITEM_SCROLL, ITEM_COUNT };
struct ItemDef { const char* name; const char* nameRu; int gold; const char* tip; };
const ItemDef& GetItem(int i);
int AtkMult(int atkType, int armType);   // standard WC3 damage table, percent

// ---------- map ----------
struct MapDef {
  const char* name; int w, h; std::string cells;   // '#' blocked, '.' open, 'a' team0 buildable, 'b' team1, 'C'/'D' castles
  char at(int x, int y) const { return cells[y * w + x]; }
};
int MapCount();
const MapDef& GetMap(int i);

// ---------- state ----------
struct Player { int team = 0, race = 0, gold = 0, lumber = 0, tech = 0, builds = 0, treasure = 0, income1000 = cfg::kBaseIncome1000; bool isBot = false; };

struct Building {
  int id = 0, def = -1, player = 0, team = 0, x = 0, y = 0, w = 2, h = 2, hp = 0, maxHp = 0;
  int buildLeft = 0, spawnLeft = 0, cd = 0, flash = 0;
  bool castle = false, treasure = false, alive = true;
};

struct Unit {
  int id = 0, def = 0, player = 0, team = 0, hp = 0, maxHp = 0;
  int x = 0, y = 0, tx = 0, ty = 0, mp = 0, mpNeed = 0, speed = 0;
  int cd = 0, stuck = 0, face = 1, flash = 0;
  bool moving = false, alive = true;
};

enum EvType : uint8_t { EV_SPAWN, EV_HIT, EV_SHOT, EV_DEATH, EV_BUILD, EV_UPGRADE, EV_CASTLE_HIT, EV_BUY, EV_TECH, EV_INCOME, EV_BLDG_DEATH, EV_ROUND_END };
struct Event { EvType type; int x, y, a, b; };   // x,y in pixels; a,b context

struct TeamBuff { bool dnb = false, bass = false; int doubleLeft = 0, quadLeft = 0, scrollLeft = 0; };

class Sim {
public:
  Sim(int mapIndex, const int* playerRaces, const int* playerTeams, int playerCount, uint32_t seed);

  void Push(const Cmd& c) { pending_.push_back(c); }
  void Step();                                   // advance exactly one tick

  // queries (also used by bots and the renderer)
  bool CanBuild(int player, int defId, int x, int y) const;
  bool CanUpgrade(int player, int buildingId, int toDef) const;
  bool CanTech(int player) const;
  bool CanBuy(int player, int item) const;
  int  BuildCostGold(int defId) const;
  int  BuildCostLumber(int defId) const;
  int  BuildSize(int defId) const;
  int  IncomeMultPermille(int player) const;     // treasure box multiplier, 1000 = x1.0
  int  NextIncomeIn() const { return cfg::kIncomeTicks - (tick % cfg::kIncomeTicks); }
  const Building* BuildingAt(int x, int y) const;
  const Building* FindBuilding(int id) const;
  bool Walkable(int x, int y) const;
  void UnitPx(const Unit& u, float alpha, float& px, float& py) const;
  uint32_t Hash() const;

  const MapDef* map; int w, h;
  std::vector<Player> players;
  std::vector<Building> buildings;
  std::vector<Unit> units;
  std::vector<Event> events;                     // cleared by the caller after consumption
  TeamBuff buff[2];
  int tick = 0, winner = -1;                     // winner: team index or -1
  int castleId[2] = {0, 0};

private:
  void Apply(const Cmd& c);
  void RebuildFields();
  void StepEconomy();
  void StepBuildings();
  void StepUnits();
  struct Hit { int unit; int bld; int dmg; int src; };   // simultaneous damage, applied after all actors moved
  void ApplyHits();
  int  CalcDamage(int dps, int atk, int armType, int armor, int srcTeam) const;
  void KillUnit(Unit& u, int killerPlayer);
  bool TrySpawn(Building& b);
  int  Dist(int team, int x, int y) const { return field_[team][y * w + x]; }
  int  UnitAt(int x, int y) const { return occ_[y * w + x]; }
  void Emit(EvType t, int px, int py, int a = 0, int b = 0) { events.push_back({t, px, py, a, b}); }
  int  RangeTiles(int rangeWc3) const;
  int  BuildingDistance(const Building& b, int x, int y) const;

  std::vector<Hit> hits_;
  std::vector<Cmd> pending_;
  std::vector<int> occ_, bocc_;                  // unit id / building index per tile
  std::vector<int> field_[2];                    // BFS distance to the enemy castle, per team
  bool fieldsDirty_ = true;
  int nextId_ = 1;
  uint32_t rng_;
};

} // namespace sim
