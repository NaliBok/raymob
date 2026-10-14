#include "bot.h"
#include <algorithm>
#include <cmath>

using namespace sim;

namespace ai {

const char* LEVEL_NAMES[5] = {"Новичок", "Ученик", "Воин", "Мастер", "Гений"};
static const int INTERVAL[5] = {110, 75, 45, 25, 12};
static const int ERR_PCT[5]  = {38, 20, 9, 2, 0};
static const double COUNTER_POW[5] = {0.0, 0.5, 0.8, 1.0, 1.25};

Bot::Bot(int player, int level, unsigned seed) : me_(player), lvl_(std::max(0, std::min(4, level))), rng_(seed * 2654435761u + 12345u) {
    if (!rng_) rng_ = 1;
    nextThink_ = 20 + (int)(rnd() % 30);
}

double Bot::unitScore(const World& w, int def) {
    const RaceDef& R = w.raceOf(me_);
    const UnitDef& u = R.u[def]; const BuildDef& b = R.b[def];
    int foe = 1 - me_;
    const RaceDef& FR = w.raceOf(foe);

    double dps = u.dmg * 20.0 / u.cd, power;
    if (u.flags & UF_HEAL) power = std::sqrt(u.hp * (u.dmg * 2.0 * 20 / u.cd) * 6);
    else power = std::sqrt(u.hp * dps);
    if (u.splash) power *= 1.15;
    if (u.flags & UF_SLOW) power *= 1.1;
    if (u.flags & UF_FLY) power *= 1.1;
    if (u.range >= 30) power *= 1.08;
    double rate = 20.0 / (b.spawn * (100 - 10 * w.pl[me_].up[UP_SPAWN]) / 100.0);
    double eff = power * rate / b.cost * 1000.0;

    // состав противника: живые юниты + здания (половинный вес)
    double A[ARM_COUNT] = {0}, B[ATK_COUNT] = {0}, total = 0, fliers = 0;
    for (auto& e : w.units) if (!e.dead && e.owner == foe) {
        const UnitDef& ed = FR.u[e.def]; A[ed.arm] += 1; B[ed.atk] += 1; total += 1; if (ed.flags & UF_FLY) fliers += 1;
    }
    for (auto& e : w.blds) if (!e.dead && e.owner == foe) {
        const UnitDef& ed = FR.u[e.def]; A[ed.arm] += 0.5; B[ed.atk] += 0.5; total += 0.5; if (ed.flags & UF_FLY) fliers += 0.5;
    }
    double counter = 1.0;
    if (total > 0.5 && COUNTER_POW[lvl_] > 0) {
        double off = 0, def_ = 0, sa = 0, sb = 0;
        for (int i = 0; i < ARM_COUNT; i++) { off += A[i] * DMG_MULT[u.atk][i] / 100.0; sa += A[i]; }
        for (int i = 0; i < ATK_COUNT; i++) { def_ += B[i] * DMG_MULT[i][u.arm] / 100.0; sb += B[i]; }
        off = sa > 0 ? off / sa : 1; def_ = sb > 0 ? def_ / sb : 1;
        if (u.flags & UF_HEAL) off = 1;
        counter = std::pow(off / def_, COUNTER_POW[lvl_]);
        double share = fliers / total;
        bool antiAir = u.atk != ATK_SIEGE && (u.range >= 30 || (u.flags & UF_FLY));
        if (share > 0.12) counter *= antiAir ? (1 + share) : std::max(0.35, 1 - 1.4 * share);
    }
    if (u.atk == ATK_SIEGE) counter *= 1.2;
    double cnt = w.countBuildings(me_, def);
    return eff * counter / (1.0 + 0.10 * cnt);
}

bool Bot::pickSpot(const World& w, int def, int& cx, int& cy, bool noisy) {
    const RaceDef& R = w.raceOf(me_);
    const BuildDef& b = R.b[def]; const UnitDef& u = R.u[def];
    bool tank = u.hp >= 250 && u.range < 30 && !(u.flags & UF_HEAL);
    bool found = false; double bestS = -1e9;
    int zTop = me_ == 1 ? 3 : MAP_H - 12, zBot = me_ == 1 ? 11 : MAP_H - 4;
    for (int y = zTop; y + b.h - 1 <= zBot; y++) for (int x = 0; x + b.w <= MAP_W; x++) {
        if (!w.canBuild(me_, def, x, y)) continue;
        double adj = 0;
        for (int yy = y - 1; yy <= y + b.h; yy++) for (int xx = x - 1; xx <= x + b.w; xx++) {
            bool inside = xx >= x && xx < x + b.w && yy >= y && yy < y + b.h;
            if (inside) continue;
            if (xx < 0 || xx >= MAP_W) { adj += 0.6; continue; }
            if (yy < 0 || yy >= MAP_H) continue;
            if (w.occOwn[yy * MAP_W + xx] == me_ + 1) adj += 1.0;
            else if (w.map->tile[yy * MAP_W + xx] == T_TREE) adj += 0.3;
        }
        double front = me_ == 0 ? (double)(zBot - (y + b.h - 1)) / (zBot - zTop) : (double)(y - zTop) / (zBot - zTop);
        double s = adj * 0.9 + (tank ? front * 1.8 : -front * 0.6) - std::abs(x + b.w * 0.5 - MAP_W * 0.5) * 0.08;
        s += rnd01() * (noisy ? 4.0 : 0.2);
        if (s > bestS) { bestS = s; cx = x; cy = y; found = true; }
    }
    return found;
}

bool Bot::tryUpgrade(const World& w, Command& c) {
    int nb = 0; for (auto& b : w.blds) if (!b.dead && b.owner == me_) nb++;
    static const int order[6] = {UP_INCOME, UP_ATK, UP_ARM, UP_HP, UP_SPAWN, UP_CASTLE};
    for (int k = 0; k < 6; k++) {
        int idx = order[k]; int cost = w.upgradeCost(me_, idx);
        if (!cost || w.pl[me_].gold < cost) continue;
        int lv = w.pl[me_].up[idx]; bool ok = false;
        switch (idx) {
            case UP_INCOME: ok = nb >= 4 + lv * 4; break;
            case UP_ATK: case UP_ARM: case UP_HP: ok = nb >= 8 + lv * 5; break;
            case UP_SPAWN: ok = nb >= 12 + lv * 5; break;
            case UP_CASTLE: ok = nb >= 6 && w.pl[me_].castleHp * 100 < w.pl[me_].castleMax * 65; break;
        }
        if (!ok) continue;
        if (lvl_ == 1 && rnd() % 100 < 75) continue;
        if (lvl_ == 2 && idx == UP_SPAWN) continue;
        c = Command(); c.type = CMD_UPGRADE; c.player = (u8)me_; c.a = idx; return true;
    }
    return false;
}

double Bot::rollout(const World& w, const Command& c, int ticks) {
    World s = w; s.rec = false; s.events.clear(); s.pending.clear();
    if (c.type) s.submit(c);
    for (int t = 0; t < ticks && s.winner < 0; t++) s.step();
    int foe = 1 - me_;
    double v = s.pl[me_].castleHp - s.pl[foe].castleHp;
    for (auto& u : s.units) {
        const UnitDef& d = s.raceOf(u.owner).u[u.def];
        double val = u.hp * (d.bounty + 8) / 100.0;
        v += (u.owner == me_ ? 0.6 : -0.6) * val;
    }
    for (auto& b : s.blds) v += (b.owner == me_ ? 0.15 : -0.15) * b.hp;
    if (s.winner == me_) v += 5000; else if (s.winner == foe) v -= 5000;
    return v;
}

void Bot::think(const World& w, std::vector<Command>& out) {
    if (w.winner >= 0 || w.tick < nextThink_) return;
    nextThink_ = w.tick + INTERVAL[lvl_] + (int)(rnd() % 8);
    if ((int)(rnd() % 100) < ERR_PCT[lvl_] / 2) return;     // зевнул ход

    Command c;
    if (lvl_ >= 1 && tryUpgrade(w, c)) { out.push_back(c); return; }

    const RaceDef& R = w.raceOf(me_);
    int gold = w.pl[me_].gold, nb = R.nb;
    double sc[MAX_BUILDS]; bool avail[MAX_BUILDS];
    for (int d = 0; d < nb; d++) { sc[d] = unitScore(w, d); avail[d] = R.b[d].req < 0 || w.hasBuilding(me_, R.b[d].req); }
    // ценность открытия технологий
    for (int d = 0; d < nb; d++) if (!avail[d]) {
        int r = R.b[d].req;
        if (r >= 0 && lvl_ >= 2) sc[r] += 0.55 * sc[d];
    }
    if (lvl_ >= 3 && w.tick < 20 * 90) for (int d = 0; d < nb; d++) if (R.b[d].cost > 140) sc[d] *= 0.8;   // сначала экономика

    int best = -1; double bestS = -1;
    for (int d = 0; d < nb; d++) if (avail[d] && gold >= R.b[d].cost && sc[d] > bestS) { bestS = sc[d]; best = d; }
    // копить на лучшее
    if (lvl_ >= 3) {
        int inc = w.income(me_);
        for (int d = 0; d < nb; d++) if (avail[d] && gold < R.b[d].cost && R.b[d].cost - gold <= inc * 8 / 10 && sc[d] > bestS * 1.5 && bestS >= 0) return;
    }
    if (best < 0) return;
    if ((int)(rnd() % 100) < ERR_PCT[lvl_]) {              // ошибка: случайная постройка
        int tries = 8; while (tries--) { int d = (int)(rnd() % nb); if (avail[d] && gold >= R.b[d].cost) { best = d; break; } }
    }

    int cx = 0, cy = 0;
    if (lvl_ >= 4) {
        // прогон будущего: топ-3 кандидата
        std::vector<Cand> cs;
        for (int d = 0; d < nb; d++) if (avail[d] && gold >= R.b[d].cost) {
            int x, y; if (pickSpot(w, d, x, y, false)) cs.push_back(Cand{d, x, y, sc[d]});
        }
        std::sort(cs.begin(), cs.end(), [](const Cand& a, const Cand& b) { return a.score > b.score; });
        if (cs.size() > 3) cs.resize(3);
        if (cs.empty()) return;
        double top = cs[0].score, ev[3] = {0, 0, 0}, mean = 0;
        for (size_t i = 0; i < cs.size(); i++) {
            Command t; t.type = CMD_BUILD; t.player = (u8)me_; t.a = cs[i].def; t.b = cs[i].cx; t.c = cs[i].cy;
            ev[i] = rollout(w, t, 240); mean += ev[i];
        }
        mean /= cs.size();
        double bs = -1e9; int bi = 0;
        for (size_t i = 0; i < cs.size(); i++) {
            double s = cs[i].score / top + std::max(-0.3, std::min(0.3, (ev[i] - mean) / 300.0));
            if (s > bs) { bs = s; bi = (int)i; }
        }
        best = cs[bi].def; cx = cs[bi].cx; cy = cs[bi].cy;
    } else {
        if (!pickSpot(w, best, cx, cy, lvl_ <= 1)) return;
    }
    c = Command(); c.type = CMD_BUILD; c.player = (u8)me_; c.a = best; c.b = cx; c.c = cy;
    out.push_back(c);
}
}
