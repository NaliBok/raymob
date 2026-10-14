#pragma once
// Tuning constants. Provenance: [map] from the catalog/map script, [base] standard WC3, [guess] reconstruction.
namespace cfg {

constexpr int kTickHz = 20;                 // simulation rate (fixed step)
constexpr int kTile = 16;                   // sprite size / tile size in pixels

// --- economy ---
constexpr int kIncomeTicks   = 10 * kTickHz; // [map] income every 10 s
constexpr int kBaseIncome1000 = 5000;        // [map] base 5 gold per cycle (x1000 fixed point)
constexpr int kEarlyBonus[4] = {20, 15, 10, 5}; // [map] bonus for the first four buildings
constexpr int kTreasureFirst1000 = 250;      // [map] first Treasure Box +25%
constexpr int kTreasureDecay100  = 85;       // [map] each next box gives 15% less
constexpr int kIncomePerCostDiv  = 25;       // [guess] every building adds cost/25 gold per cycle
constexpr int kStartGold   = 300;            // [guess]
constexpr int kStartLumber = 0;              // [guess]
constexpr int kLumberPerCycle = 12;          // [guess]
constexpr int kBountyHpDiv = 40;             // [guess] kill bounty = unit hp / 40 (min 2)

// --- buildings ---
constexpr int kBuildTicks  = 8 * kTickHz;    // [guess] construction time
constexpr int kBuildingArmor = 5;            // [guess] fortified
constexpr int kCastleArmor = 20;              // [guess]

// --- units ---
constexpr int kAtkIntervalTicks = kTickHz;   // attacks once per second, damage = dps (tooltip values)
constexpr int kMoveMilliPerTile = 1000;
constexpr int kMoveMilliDiag    = 1414;
constexpr int kTileWc3 = 256;                // [guess] one tile = 256 WC3 range units (arena is compact)
constexpr int kMeleeMaxRange = 150;          // [base] WC3 melee range band

// --- items ---
constexpr int kBuffDurationTicks = 30 * kTickHz; // [map]

// --- match ---
constexpr int kDefaultWinsNeeded = 3;        // [map] qr=2 -> first to 3 round wins; menu lets the player pick 1..3
} // namespace cfg
