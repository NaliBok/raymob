#include "bot.h"
#include <algorithm>
#include <cstdlib>

namespace ai {
int g_dbg = 0;

Bot::Bot(int player, Level level, uint32_t seed) : me_(player), lvl_(level), rng_(seed * 2654435761u + player + 1) {
  //                       react noise counters smartItems maxTB skip%   (artificial mistakes shrink with level)
  switch (level) {
    case Level::Easy:   P_ = {90, 60, false, false, 1, 35, 0};   break;
    case Level::Normal: P_ = {50, 30, false, false, 2, 15, 0};   break;
    case Level::Hard:   P_ = {30, 10, true,  false, 3, 3,  0}; break;   // saves up for up to 15 s
    case Level::Genius: P_ = {10, 0,  true,  true,  4, 0,  0}; break;   // plans up to 45 s ahead
  }
}

int Bot::Rand(int n) {
  rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
  return n > 0 ? (int)(rng_ % (uint32_t)n) : 0;
}

// value per gold of a building: how much useful force it produces against what the enemy currently fields
int Bot::ScoreDef(const sim::Sim& s, int def, const int* enemyArm, const int* enemyAtk) {
  const auto& D = gd::Data(); const auto& bd = D.buildings[def];
  int64_t hp, dps, range = 100; int armType, atkType, armor, trainDeci = 100;
  if (bd.isTower) { hp = bd.hp; dps = bd.towDps; armType = gd::ARM_FORT; atkType = bd.towAtk; armor = 5; range = bd.towRange; }
  else {
    const auto& u = D.units[bd.unit];
    hp = u.hp; dps = u.dps; armType = u.armType; atkType = u.atkType; armor = u.armor; range = u.rangeWc3; trainDeci = u.trainDeci;
  }
  int64_t offW = 0, offN = 0, defW = 0, defN = 0;
  for (int a = 0; a < gd::ARM_COUNT; a++) { int n = P_.counters ? enemyArm[a] + 1 : 1; offW += (int64_t)sim::AtkMult(atkType, a) * n; offN += n; }
  for (int t = 0; t < gd::ATK_COUNT; t++) { int n = P_.counters ? enemyAtk[t] + 1 : 1; defW += (int64_t)sim::AtkMult(t, armType) * n; defN += n; }
  int64_t off = offW / offN;                                   // percent
  int64_t inc = std::max<int64_t>(5, defW / defN);             // percent damage taken
  int64_t power = hp * 100 / inc * 100 / (100 + 6 * armor);
  power = power * dps * off / 100;
  if (range > 300) power = power * 115 / 100;                  // ranged units fight longer
  int64_t cost = bd.gold + bd.lumber / 2;
  int64_t score = power * 100 / ((int64_t)trainDeci * std::max<int64_t>(cost, 1));
  int own = 0; for (auto& b : s.buildings) if (b.alive && b.player == me_ && b.def == def) own++;
  score = score * 100 / (100 + (lvl_ == Level::Genius && !(g_dbg & 32) ? 35 : 15) * own);   // diversify, harder to counter
  if (P_.noise) score += score * (Rand(2 * P_.noise + 1) - P_.noise) / 100;
  return (int)std::min<int64_t>(score, 2000000000);
}

bool Bot::PlaceBest(sim::Sim& s, int def) {
  int sz = s.BuildSize(def), team = s.players[me_].team, cx2 = s.w;
  bool tower = def != sim::kTreasureDef && gd::Data().buildings[def].isTower;
  bool back = def == sim::kTreasureDef;
  int frontY = team == 0 ? 14 : 9, backY = team == 0 ? 20 : 3;
  int bestK = 1 << 30, bx = -1, by = -1;
  for (int y = 0; y < s.h; y++) for (int x = 0; x < s.w; x++) {
    if (!s.CanBuild(me_, def, x, y)) continue;
    int centerDist = std::abs(2 * x + sz - cx2);
    int k = back ? std::abs(y - backY) * 4 + centerDist / 4 + (team == 0 ? y : -y) * 0
                 : (tower ? centerDist * 3 + std::abs(y - frontY) * 2 : centerDist * 2 + std::abs(y - frontY));
    if (lvl_ == Level::Easy || lvl_ == Level::Normal) k += Rand(P_.noise / 4 + 1);
    if (k < bestK) { bestK = k; bx = x; by = y; }
  }
  if (bx < 0) return false;
  sim::Cmd c; c.type = sim::CMD_BUILD; c.player = (uint8_t)me_; c.a = (int16_t)def; c.b = (int16_t)bx; c.c = (int16_t)by;
  s.Push(c);
  return true;
}

void Bot::Think(sim::Sim& s) {
  if (s.winner >= 0 || --wait_ > 0) return;
  wait_ = (g_dbg & 64) ? 30 : P_.react;
  if (Rand(100) < P_.skipPct) return;                           // artificial lapse (low levels only)
  const auto& D = gd::Data();
  sim::Player& p = s.players[me_];
  int team = p.team;

  // ---- read the battlefield ----
  int enemyArm[gd::ARM_COUNT] = {0}, enemyAtk[gd::ATK_COUNT] = {0};
  int myUnits = 0, myHp = 0, myMax = 0, nearEnemyCastle = 0;
  const sim::Building& ec = s.buildings[1 - team];
  for (auto& u : s.units) {
    if (!u.alive) continue;
    if (u.team != team) { enemyArm[D.units[u.def].armType]++; enemyAtk[D.units[u.def].atkType]++; }
    else {
      myUnits++; myHp += u.hp; myMax += u.maxHp;
      int d = std::max(std::abs(u.x - ec.x), std::abs(u.y - ec.y));
      if (d <= 8) nearEnemyCastle++;
    }
  }
  int nb = 0, ntw = 0, ntb = 0;
  for (auto& b : s.buildings) if (b.alive && b.player == me_) { nb++; if (b.treasure) ntb++; else if (b.def >= 0 && D.buildings[b.def].isTower) ntw++; }

  // ---- items ----
  if (P_.smartItems && !(g_dbg & 2)) {
    bool decisive = nearEnemyCastle >= 6 || (nearEnemyCastle >= 4 && ec.hp * 100 < ec.maxHp * 60);
    if (decisive && s.CanBuy(me_, sim::ITEM_QUAD) && p.gold >= 1400 + 150) { sim::Cmd c; c.type = sim::CMD_BUY; c.player = me_; c.a = sim::ITEM_QUAD; s.Push(c); return; }
    if (decisive && !(s.buff[team].quadLeft > 0) && s.CanBuy(me_, sim::ITEM_DOUBLE) && p.gold >= 750) { sim::Cmd c; c.type = sim::CMD_BUY; c.player = me_; c.a = sim::ITEM_DOUBLE; s.Push(c); return; }
    if (myUnits >= 8 && myHp * 100 < myMax * 55 && s.CanBuy(me_, sim::ITEM_SCROLL) && p.gold >= 300) { sim::Cmd c; c.type = sim::CMD_BUY; c.player = me_; c.a = sim::ITEM_SCROLL; s.Push(c); return; }
    if (nb >= 6 && s.CanBuy(me_, sim::ITEM_DNB) && p.gold >= 900) { sim::Cmd c; c.type = sim::CMD_BUY; c.player = me_; c.a = sim::ITEM_DNB; s.Push(c); return; }
    if (nb >= 7 && s.CanBuy(me_, sim::ITEM_BASS) && p.gold >= 950) { sim::Cmd c; c.type = sim::CMD_BUY; c.player = me_; c.a = sim::ITEM_BASS; s.Push(c); return; }
  } else if (lvl_ == Level::Hard && nb >= 8 && s.CanBuy(me_, sim::ITEM_BASS) && p.gold >= 1000) {
    sim::Cmd c; c.type = sim::CMD_BUY; c.player = me_; c.a = sim::ITEM_BASS; s.Push(c); return;
  }

  // ---- tech ----
  bool wantTech = p.tech < 3 && nb >= 2 + p.tech * 3;
  if (wantTech && s.CanTech(me_)) { sim::Cmd c; c.type = sim::CMD_TECH; c.player = me_; s.Push(c); return; }

  // ---- economy: treasure boxes early ----
  if (ntb < ((g_dbg & 4) ? 0 : (g_dbg & 8) ? 1 : P_.maxTreasure) && nb >= 2 + ntb * 2 && p.gold >= D.c.treasureGold + 60) {
    if (PlaceBest(s, sim::kTreasureDef)) return;
  }

  // ---- upgrades ----
  if (P_.counters) {
    int bestGain = 0, bid = -1, bto = -1;
    for (auto& b : s.buildings) {
      if (!b.alive || b.def < 0 || b.player != me_) continue;
      for (int to = 0; to < D.buildingCount; to++) {
        if (!s.CanUpgrade(me_, b.id, to)) continue;
        int gain = ScoreDef(s, to, enemyArm, enemyAtk) - ScoreDef(s, b.def, enemyArm, enemyAtk) * 11 / 10;
        if (gain > bestGain) { bestGain = gain; bid = b.id; bto = to; }
      }
    }
    if (bid >= 0 && p.gold >= 100) { sim::Cmd c; c.type = sim::CMD_UPGRADE; c.player = me_; c.a = (int16_t)bid; c.b = (int16_t)bto; s.Push(c); return; }
  }

  // genius saves up for a tech level instead of spending lumber-starved gold elsewhere
  if (lvl_ == Level::Genius && !(g_dbg & 1) && wantTech && p.lumber >= D.c.techLumber[p.tech] && p.gold < D.c.techGold[p.tech] + 40) return;

  // ---- build: pick the best target, save up for it if the wait is short, otherwise buy the best affordable ----
  int best = -1, bestScore = -1, bestAny = -1, bestAnyScore = -1;
  for (int d = 0; d < D.buildingCount; d++) {
    const auto& bd = D.buildings[d];
    if (bd.race != p.race || bd.upgradeFrom >= 0 || bd.tech > p.tech) continue;
    if (bd.isTower && ntw >= 1 + nb / 6) continue;
    int sc = ScoreDef(s, d, enemyArm, enemyAtk);
    if (bd.isTower) sc = sc * (lvl_ == Level::Genius && !(g_dbg & 16) ? 80 : 40) / 100;
    if (sc > bestAnyScore) { bestAnyScore = sc; bestAny = d; }
    if (p.gold >= bd.gold && p.lumber >= bd.lumber && sc > bestScore) { bestScore = sc; best = d; }
  }
  if (bestAny >= 0 && bestAny != best && P_.waitTicks > 0 && bestAnyScore > bestScore * 12 / 10) {
    const auto& bd = D.buildings[bestAny];
    int gain = (int)((int64_t)p.income1000 * s.IncomeMultPermille(me_) / 1000000);          // gold per cycle
    int missG = std::max(0, bd.gold - p.gold), missL = std::max(0, bd.lumber - p.lumber);
    int etaG = missG > 0 ? (int)((int64_t)missG * cfg::kIncomeTicks / std::max(1, gain)) : 0;
    int etaL = missL > 0 ? ((missL + cfg::kLumberPerCycle - 1) / cfg::kLumberPerCycle) * cfg::kIncomeTicks : 0;
    if (std::max(etaG, etaL) <= P_.waitTicks) return;                                        // worth waiting for
  }
  if (best >= 0) PlaceBest(s, best);
}

} // namespace ai
