// Детерминированная симуляция боя. Без float, без внешних зависимостей (годится для сервера/веба/WASM).
#pragma once
#include "data.h"
#include <memory>

namespace sim {

enum CmdType : u8 { CMD_NONE, CMD_BUILD, CMD_SELL, CMD_UPGRADE, CMD_SURRENDER };
struct Command {            // всё, что игрок может сделать (это и передаётся по сети)
    u8 type = 0, player = 0; i32 a = 0, b = 0, c = 0;   // BUILD: a=def b=cx c=cy; SELL: a=id; UPGRADE: a=idx
};

enum EvType : u8 { EV_SHOT, EV_HIT, EV_DEATH, EV_SPAWN, EV_BUILD, EV_SELL, EV_BDEATH, EV_CASTLE_HIT,
                   EV_INCOME, EV_HEAL, EV_UPGRADE, EV_END, EV_BOUNTY };
struct Event { u8 type, owner, def, aux; i32 x, y, x2, y2, val; };   // только для эффектов/звука

struct Unit {
    i32 id; u8 owner, def; bool dead;
    i32 x, y, hp, maxhp, cd, slow;
    i32 anim; i32 atkAnim;
};
struct Building {
    i32 id; u8 owner, def; bool dead;
    i32 cx, cy, hp, maxhp, spawnT;
};
struct Player {
    u8 race; i32 gold; i32 castleHp, castleMax, castleCd; u8 up[NUM_UPGRADES];
    i32 kills, earned, built;
};
struct Flow { std::vector<u16> d[2]; };

struct World {
    std::shared_ptr<const Map> map;
    std::shared_ptr<const Flow> flow;
    i32 tick = 0; u32 rng = 1; i32 nextId = 1; int winner = -1;   // -1 идёт, 0/1 победитель, 2 ничья
    Player pl[NUM_PLAYERS];
    std::vector<Unit> units;
    std::vector<Building> blds;
    std::vector<i32> occ;           // id здания (>0) или -1/-2 для замков
    std::vector<u8> occOwn;         // 0 пусто, 1/2 - владелец+1
    std::vector<Command> pending;
    std::vector<Event> events;
    bool rec = true;

    void init(int mapId, int race0, int race1, u32 seed);
    void submit(const Command& c) { pending.push_back(c); }
    void step();
    u32 hash() const;

    // запросы (для UI и ИИ)
    bool canBuild(int p, int def, int cx, int cy, const char** why = nullptr) const;
    bool hasBuilding(int p, int def) const;
    int countBuildings(int p, int def) const;
    int countUnits(int p) const;
    int income(int p) const;
    int upgradeCost(int p, int idx) const;       // 0 если нельзя
    const Building* bldAt(int cx, int cy) const;
    const Building* findBld(i32 id) const;
    int castleCenterX(int owner) const;
    int castleCenterY(int owner) const;
    const RaceDef& raceOf(int p) const { return race(pl[p].race); }
    int ticksToIncome() const { return INCOME_PERIOD - (tick % INCOME_PERIOD); }

private:
    void apply(const Command& c);
    u32 rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
    void emit(u8 t, u8 owner, u8 def, i32 x, i32 y, i32 x2 = 0, i32 y2 = 0, i32 val = 0, u8 aux = 0);
    bool blockedPt(i32 x, i32 y, int owner, bool fly) const;
    bool blockedBox(i32 x, i32 y, int owner, bool fly) const;
    bool tryMove(Unit& u, i32 dx, i32 dy, bool fly);
    void stepToward(Unit& u, i32 tx, i32 ty, i32 speed, bool fly);
    void followFlow(Unit& u, const UnitDef& d, i32 speed);
    void updateUnit(Unit& u);
    void doAttack(Unit& u, const UnitDef& d);
    void hurtUnit(Unit& v, i32 dmg, int killerOwner, int atk, int fromX, int fromY);
    void hurtCastle(int owner, i32 dmg, int atk);
    i32 calcDamage(int attOwner, int atk, i32 base, int arm, int armorVal, int defOwner) const;
    void spawnUnit(const Building& b);
    void separate();
    void checkEnd();
};
}
