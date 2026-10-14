// Игровые данные: расы, здания, юниты, карты. Всё целочисленное и детерминированное.
#pragma once
#include <cstdint>
#include <vector>

namespace sim {

typedef int32_t i32; typedef int64_t i64; typedef uint32_t u32; typedef uint16_t u16; typedef uint8_t u8;

constexpr int TICKS_PER_SEC = 20;
constexpr int CELL = 16;              // пикселей в клетке
constexpr int SUB = 16;               // подпиксельная точность (1/16 px)
constexpr int CELLU = CELL * SUB;     // единиц в клетке
constexpr int MAP_W = 15;
constexpr int MAP_H = 44;
constexpr int NUM_PLAYERS = 2;
constexpr int INCOME_PERIOD = 200;    // 10 секунд
constexpr int UNIT_CAP = 100;
constexpr int MAX_BUILDS = 16;
constexpr int NUM_UPGRADES = 6;
constexpr int OVERTIME_TICKS = 20 * 60 * 16;
constexpr int SUDDEN_TICKS = 20 * 60 * 9;   // после этого замки начинают рушиться

constexpr int CASTLE_W = 5, CASTLE_H = 3, CASTLE_CX = 5;
constexpr int DAMAGE_SCALE = 100;          // DPS берётся из карты как есть
constexpr int CASTLE_HP = 5000, CASTLE_DMG = 40, CASTLE_RANGE = 96, CASTLE_CD = 30;

enum { ATK_NORMAL, ATK_PIERCE, ATK_SIEGE, ATK_MAGIC, ATK_CHAOS, ATK_HERO, ATK_COUNT };
enum { ARM_LIGHT, ARM_MEDIUM, ARM_HEAVY, ARM_FORT, ARM_HERO, ARM_UNARMED, ARM_DIVINE, ARM_COUNT };
enum { UF_FLY = 1, UF_HEAL = 2, UF_SLOW = 4, UF_REGEN = 8 };
enum { ART_HUMAN, ART_KNIGHT, ART_SIEGE, ART_FLYER, ART_GOLEM };
enum { W_SWORD, W_BOW, W_STAFF, W_CLAW, W_CLUB, W_LANCE, W_NONE };
enum { UP_ATK, UP_ARM, UP_HP, UP_INCOME, UP_SPAWN, UP_CASTLE };

struct UnitDef {
    const char* name; const char* desc;
    u8 art, weapon, atk, arm, flags;
    i32 armor, hp, dmg, range, cd, speed, splash, bounty, radius;
};
struct BuildDef {
    const char* name;
    u8 w, h; i32 cost, hp, spawn; int req; u8 shape;
};
struct RaceDef {
    const char* name; const char* desc; int nb; int unlockMission;
    BuildDef b[MAX_BUILDS]; UnitDef u[MAX_BUILDS];
};
struct UpgradeDef { const char* name; const char* desc; int levels; i32 cost[5]; };

extern const char* ATK_NAMES[ATK_COUNT];
extern const char* ARM_NAMES[ARM_COUNT];
extern const int DMG_MULT[ATK_COUNT][ARM_COUNT];
extern const UpgradeDef UPGRADES[NUM_UPGRADES];

constexpr int RACE_COUNT = 12;
const RaceDef& race(int id);

struct Map {
    int id = 0; char name[24] = {};
    std::vector<u8> tile;   // MAP_W*MAP_H
};
enum { T_GROUND, T_ROCK, T_WATER, T_TREE, T_ZONE };
constexpr int MAP_COUNT = 3;
const char* mapName(int id);
Map makeMap(int id);
inline bool tileBlocked(u8 t) { return t == T_ROCK || t == T_WATER || t == T_TREE; }
inline int zoneOwner(int cy) { return cy < MAP_H / 2 ? 1 : 0; }

i64 isqrt64(i64 v);
}
