#pragma once
// Static game data. Races/buildings/units are generated from the map catalog (gamedata_gen.cpp).
// Tuning constants that are NOT in the catalog live in config.h and are tagged map/base/guess.
namespace gd {

enum AtkType { ATK_NORMAL, ATK_PIERCE, ATK_SIEGE, ATK_MAGIC, ATK_CHAOS, ATK_HERO, ATK_COUNT };
enum ArmType { ARM_LIGHT, ARM_MEDIUM, ARM_HEAVY, ARM_FORT, ARM_HERO, ARM_UNARMORED, ARM_DIVINE, ARM_COUNT };

struct UnitDef {
  const char* name; int race;
  int hp, armor, armType, atkType, dps, rangeWc3, speedWc3;
  int trainDeci;                 // training/spawn time, tenths of a second
  const char* abilities;         // raw tooltip text (abilities not simulated yet)
  const char *srcStats, *srcSpeed, *srcTrain;
};

struct BuildingDef {
  const char* name; int race, gold, lumber, hp, tech, upgradeFrom, size;
  int unit;                      // unit trained, -1 for towers
  int isTower, towAtk, towRange, towDps, legendary;
  const char* tip;
  const char *srcCost, *srcHp;
};

struct RaceColor { unsigned char r, g, b; };
struct RaceDef { const char *id, *name, *nameRu, *blurb, *blurbRu; RaceColor color; };

struct CatalogConsts {
  int castleHp, treasureGold, treasureHp;
  int techGold[3], techLumber[3];
};

struct GameData {
  const UnitDef* units; int unitCount;
  const BuildingDef* buildings; int buildingCount;
  const RaceDef* races; int raceCount;
  CatalogConsts c;
};

const GameData& Data();

} // namespace gd
