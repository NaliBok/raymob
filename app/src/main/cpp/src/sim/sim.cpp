#include "sim.h"
#include <algorithm>
#include <cstdlib>

namespace sim {

static inline i32 absi(i32 v) { return v < 0 ? -v : v; }
static const int DX8[8] = {0, 0, -1, 1, -1, 1, -1, 1};
static const int DY8[8] = {-1, 1, 0, 0, -1, -1, 1, 1};

// ---------------------------------------------------------------- инициализация
static std::shared_ptr<const Flow> buildFlow(const Map& m) {
    auto f = std::make_shared<Flow>();
    for (int team = 0; team < 2; team++) {
        std::vector<u16>& d = f->d[team];
        d.assign(MAP_W * MAP_H, 65535);
        std::vector<int> q; q.reserve(MAP_W * MAP_H);
        int enemy = 1 - team;
        int r0 = enemy == 1 ? 0 : MAP_H - CASTLE_H;
        for (int r = r0; r < r0 + CASTLE_H; r++) for (int c = CASTLE_CX; c < CASTLE_CX + CASTLE_W; c++) {
            d[r * MAP_W + c] = 0; q.push_back(r * MAP_W + c);
        }
        for (size_t h = 0; h < q.size(); h++) {
            int cur = q[h], cx = cur % MAP_W, cy = cur / MAP_W;
            for (int k = 0; k < 4; k++) {
                int nx = cx + DX8[k], ny = cy + DY8[k];
                if (nx < 0 || ny < 0 || nx >= MAP_W || ny >= MAP_H) continue;
                if (tileBlocked(m.tile[ny * MAP_W + nx])) continue;
                if (d[ny * MAP_W + nx] != 65535) continue;
                d[ny * MAP_W + nx] = d[cur] + 1; q.push_back(ny * MAP_W + nx);
            }
        }
    }
    return f;
}

void World::init(int mapId, int race0, int race1, u32 seed) {
    map = std::make_shared<Map>(makeMap(mapId));
    flow = buildFlow(*map);
    tick = 0; rng = seed ? seed : 1; nextId = 1; winner = -1;
    units.clear(); blds.clear(); pending.clear(); events.clear();
    occ.assign(MAP_W * MAP_H, 0); occOwn.assign(MAP_W * MAP_H, 0);
    int races[2] = {race0, race1};
    for (int p = 0; p < 2; p++) {
        pl[p] = Player();
        pl[p].race = (u8)races[p]; pl[p].gold = 300;   // guess
        pl[p].castleHp = pl[p].castleMax = CASTLE_HP;
        int r0 = p == 1 ? 0 : MAP_H - CASTLE_H;
        for (int r = r0; r < r0 + CASTLE_H; r++) for (int c = CASTLE_CX; c < CASTLE_CX + CASTLE_W; c++) {
            occ[r * MAP_W + c] = -(p + 1); occOwn[r * MAP_W + c] = (u8)(p + 1);
        }
    }
}

int World::castleCenterX(int) const { return (CASTLE_CX * CELLU) + CASTLE_W * CELLU / 2; }
int World::castleCenterY(int owner) const {
    return owner == 1 ? CASTLE_H * CELLU / 2 : (MAP_H * CELLU - CASTLE_H * CELLU / 2);
}

void World::emit(u8 t, u8 owner, u8 def, i32 x, i32 y, i32 x2, i32 y2, i32 val, u8 aux) {
    if (!rec) return;
    events.push_back(Event{t, owner, def, aux, x, y, x2, y2, val});
}

// ---------------------------------------------------------------- запросы
const Building* World::findBld(i32 id) const {
    size_t lo = 0, hi = blds.size();
    while (lo < hi) { size_t m = (lo + hi) / 2; if (blds[m].id < id) lo = m + 1; else hi = m; }
    return (lo < blds.size() && blds[lo].id == id) ? &blds[lo] : nullptr;
}
const Building* World::bldAt(int cx, int cy) const {
    if (cx < 0 || cy < 0 || cx >= MAP_W || cy >= MAP_H) return nullptr;
    i32 o = occ[cy * MAP_W + cx];
    return o > 0 ? findBld(o) : nullptr;
}
bool World::hasBuilding(int p, int def) const { return countBuildings(p, def) > 0; }
int World::countBuildings(int p, int def) const {
    int n = 0; for (auto& b : blds) if (!b.dead && b.owner == p && b.def == def) n++; return n;
}
int World::countUnits(int p) const {
    int n = 0; for (auto& u : units) if (!u.dead && u.owner == p) n++; return n;
}
int World::income(int p) const {
    int v = 5;                                  // map: базовые 5 золота за цикл
    const RaceDef& r = raceOf(p);
    for (auto& b : blds) if (!b.dead && b.owner == p) v += r.b[b.def].cost / 12;     // guess: вклад зданий
    return v * (100 + 25 * pl[p].up[UP_INCOME]) / 100;                              // map: Treasure Box +25%/ур.
}
int World::upgradeCost(int p, int idx) const {
    if (idx < 0 || idx >= NUM_UPGRADES) return 0;
    int lv = pl[p].up[idx];
    if (lv >= UPGRADES[idx].levels) return 0;
    return UPGRADES[idx].cost[lv];
}

bool World::canBuild(int p, int def, int cx, int cy, const char** why) const {
    const char* dummy; if (!why) why = &dummy;
    const RaceDef& r = raceOf(p);
    if (winner >= 0) { *why = "Матч окончен"; return false; }
    if (def < 0 || def >= r.nb) { *why = "?"; return false; }
    const BuildDef& b = r.b[def];
    if (b.req >= 0 && !hasBuilding(p, b.req)) { *why = "Нужно здание"; return false; }
    if (pl[p].gold < b.cost) { *why = "Мало золота"; return false; }
    for (int y = cy; y < cy + b.h; y++) for (int x = cx; x < cx + b.w; x++) {
        if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) { *why = "За картой"; return false; }
        if (map->tile[y * MAP_W + x] != T_ZONE || zoneOwner(y) != p) { *why = "Только своя зона"; return false; }
        if (occ[y * MAP_W + x] != 0) { *why = "Место занято"; return false; }
    }
    return true;
}

// ---------------------------------------------------------------- команды
void World::apply(const Command& c) {
    if (c.player >= NUM_PLAYERS || winner >= 0) return;
    Player& P = pl[c.player];
    if (c.type == CMD_BUILD) {
        if (!canBuild(c.player, c.a, c.b, c.c)) return;
        const BuildDef& bd = raceOf(c.player).b[c.a];
        P.gold -= bd.cost;
        { static const int EARLY[4] = {20, 15, 10, 5}; if (P.built < 4) P.gold += EARLY[P.built]; P.built++; }   // map: бонус раннего строительства
        Building b{}; b.id = nextId++; b.owner = c.player; b.def = (u8)c.a; b.cx = c.b; b.cy = c.c;
        b.hp = b.maxhp = bd.hp; b.spawnT = bd.spawn / 2 + 20;
        blds.push_back(b);
        for (int y = b.cy; y < b.cy + bd.h; y++) for (int x = b.cx; x < b.cx + bd.w; x++) {
            occ[y * MAP_W + x] = b.id; occOwn[y * MAP_W + x] = (u8)(c.player + 1);
        }
        emit(EV_BUILD, c.player, b.def, (b.cx + bd.w / 2) * CELLU + (bd.w & 1 ? CELLU / 2 : 0), (b.cy + bd.h / 2) * CELLU + (bd.h & 1 ? CELLU / 2 : 0), 0, 0, bd.w);
    } else if (c.type == CMD_SELL) {
        for (auto& b : blds) if (b.id == c.a && !b.dead && b.owner == c.player) {
            const BuildDef& bd = raceOf(c.player).b[b.def];
            P.gold += bd.cost * 6 / 10; b.dead = true;
            for (int y = b.cy; y < b.cy + bd.h; y++) for (int x = b.cx; x < b.cx + bd.w; x++) { occ[y * MAP_W + x] = 0; occOwn[y * MAP_W + x] = 0; }
            emit(EV_SELL, c.player, b.def, (b.cx * CELLU) + bd.w * CELLU / 2, (b.cy * CELLU) + bd.h * CELLU / 2);
            break;
        }
    } else if (c.type == CMD_UPGRADE) {
        int cost = upgradeCost(c.player, c.a);
        if (cost <= 0 || P.gold < cost) return;
        P.gold -= cost; P.up[c.a]++;
        if (c.a == UP_CASTLE) { P.castleMax += 500; P.castleHp = std::min(P.castleMax, P.castleHp + 500); }
        emit(EV_UPGRADE, c.player, (u8)c.a, 0, 0);
    } else if (c.type == CMD_SURRENDER) {
        P.castleHp = 0;
    }
}

// ---------------------------------------------------------------- движение
bool World::blockedPt(i32 x, i32 y, int owner, bool fly) const {
    if (x < 0 || y < 0) return true;
    int cx = x / CELLU, cy = y / CELLU;
    if (cx >= MAP_W || cy >= MAP_H) return true;
    if (fly) return false;
    int i = cy * MAP_W + cx;
    if (tileBlocked(map->tile[i])) return true;
    u8 o = occOwn[i];
    return o != 0 && o != owner + 1;
}
bool World::blockedBox(i32 x, i32 y, int owner, bool fly) const {
    const i32 h = 40;
    return blockedPt(x - h, y - h, owner, fly) || blockedPt(x + h, y - h, owner, fly) ||
           blockedPt(x - h, y + h, owner, fly) || blockedPt(x + h, y + h, owner, fly);
}
bool World::tryMove(Unit& u, i32 dx, i32 dy, bool fly) {
    if (!blockedBox(u.x + dx, u.y + dy, u.owner, fly)) { u.x += dx; u.y += dy; return true; }
    if (dx && !blockedBox(u.x + dx, u.y, u.owner, fly)) { u.x += dx; return true; }
    if (dy && !blockedBox(u.x, u.y + dy, u.owner, fly)) { u.y += dy; return true; }
    return false;
}
void World::stepToward(Unit& u, i32 tx, i32 ty, i32 speed, bool fly) {
    i64 dx = tx - u.x, dy = ty - u.y;
    i64 d = isqrt64(dx * dx + dy * dy);
    if (d == 0) return;
    i64 st = std::min<i64>(speed, d);
    if (u.slow > 0) st = std::max<i64>(1, st * 55 / 100);
    if (tryMove(u, (i32)(dx * st / d), (i32)(dy * st / d), fly)) u.anim++;
}
void World::followFlow(Unit& u, const UnitDef& d, i32 speed) {
    bool fly = (d.flags & UF_FLY) != 0;
    int enemy = 1 - u.owner;
    if (fly) { stepToward(u, castleCenterX(enemy), castleCenterY(enemy), speed, true); return; }
    int cx = std::min(MAP_W - 1, std::max(0, u.x / CELLU)), cy = std::min(MAP_H - 1, std::max(0, u.y / CELLU));
    const std::vector<u16>& fd = flow->d[u.owner];
    int best = fd[cy * MAP_W + cx], bx = -1, by = -1;
    for (int k = 0; k < 8; k++) {
        int nx = cx + DX8[k], ny = cy + DY8[k];
        if (nx < 0 || ny < 0 || nx >= MAP_W || ny >= MAP_H) continue;
        if (tileBlocked(map->tile[ny * MAP_W + nx])) continue;
        if (k >= 4 && (tileBlocked(map->tile[cy * MAP_W + nx]) || tileBlocked(map->tile[ny * MAP_W + cx]))) continue;
        int dv = fd[ny * MAP_W + nx];
        if (dv < best) { best = dv; bx = nx; by = ny; }
    }
    i32 tx, ty;
    if (bx < 0) { tx = castleCenterX(enemy); ty = castleCenterY(enemy); }
    else { tx = bx * CELLU + CELLU / 2; ty = by * CELLU + CELLU / 2; }
    stepToward(u, tx, ty, speed, false);
}

// ---------------------------------------------------------------- бой
i32 World::calcDamage(int attOwner, int atk, i32 base, int arm, int armorVal, int defOwner) const {
    i64 d = (i64)base * DMG_MULT[atk][arm] * DAMAGE_SCALE / 10000;
    d = d * (100 + 12 * pl[attOwner].up[UP_ATK]) / 100;
    int a = armorVal + 2 * pl[defOwner].up[UP_ARM];          // формула брони Warcraft III: 1/(1+0.06*a)
    d = d * 100 / (100 + 6 * std::max(0, a));
    return (i32)std::max<i64>(1, d);
}

void World::hurtUnit(Unit& v, i32 dmg, int killerOwner, int, int fromX, int fromY) {
    if (v.dead) return;
    v.hp -= dmg;
    emit(EV_HIT, v.owner, v.def, v.x, v.y, fromX, fromY, dmg);
    if (v.hp <= 0) {
        v.dead = true;
        const UnitDef& d = raceOf(v.owner).u[v.def];
        emit(EV_DEATH, v.owner, v.def, v.x, v.y);
        if (killerOwner >= 0 && killerOwner != v.owner) {
            pl[killerOwner].gold += d.bounty; pl[killerOwner].earned += d.bounty; pl[killerOwner].kills++;
            emit(EV_BOUNTY, (u8)killerOwner, 0, v.x, v.y, 0, 0, d.bounty);
        }
    }
}
void World::hurtCastle(int owner, i32 dmg, int) {
    pl[owner].castleHp -= dmg;
    emit(EV_CASTLE_HIT, owner, 0, castleCenterX(owner), castleCenterY(owner), 0, 0, dmg);
}

namespace {
struct Tgt { int kind = 0; int idx = -1; i64 d2 = 0; };
}

static inline i64 rectD2(i32 x, i32 y, i32 x0, i32 y0, i32 x1, i32 y1, i32& px, i32& py) {
    px = x < x0 ? x0 : (x > x1 ? x1 : x);
    py = y < y0 ? y0 : (y > y1 ? y1 : y);
    i64 dx = x - px, dy = y - py; return dx * dx + dy * dy;
}

void World::spawnUnit(const Building& b) {
    const RaceDef& r = raceOf(b.owner);
    const UnitDef& d = r.u[b.def];
    const BuildDef& bd = r.b[b.def];
    Unit u{}; u.id = nextId++; u.owner = b.owner; u.def = b.def;
    u.maxhp = u.hp = (i32)((i64)d.hp * (100 + 12 * pl[b.owner].up[UP_HP]) / 100);
    i32 span = bd.w * CELLU - 6 * SUB;
    u.x = b.cx * CELLU + 3 * SUB + (i32)(rnd() % (u32)span);
    u.y = b.owner == 0 ? b.cy * CELLU - CELLU / 2 : (b.cy + bd.h) * CELLU + CELLU / 2;
    if (u.y < CELLU / 2) u.y = CELLU / 2;
    if (u.y > MAP_H * CELLU - CELLU / 2) u.y = MAP_H * CELLU - CELLU / 2;
    u.cd = (i32)(rnd() % 8);
    units.push_back(u);
    emit(EV_SPAWN, u.owner, u.def, u.x, u.y);
}

void World::doAttack(Unit&, const UnitDef&) {}

void World::updateUnit(Unit& u) {
    const RaceDef& R = raceOf(u.owner);
    const UnitDef& d = R.u[u.def];
    if (u.cd > 0) u.cd--;
    if (u.slow > 0) u.slow--;
    if (u.atkAnim > 0) u.atkAnim--;
    if ((d.flags & UF_REGEN) && (tick % 10) == 0 && u.hp < u.maxhp) u.hp++;
    bool fly = (d.flags & UF_FLY) != 0;
    i32 speed = d.speed;

    // --- целители не атакуют, а лечат
    if (d.flags & UF_HEAL) {
        if (u.cd == 0) {
            int bi = -1; i64 bestRatio = 1000000;
            for (size_t j = 0; j < units.size(); j++) {
                const Unit& a = units[j];
                if (a.dead || a.owner != u.owner || a.hp >= a.maxhp || a.id == u.id) continue;
                i64 dx = a.x - u.x, dy = a.y - u.y;
                if (dx * dx + dy * dy > (i64)d.range * SUB * d.range * SUB) continue;
                i64 ratio = (i64)a.hp * 1000 / a.maxhp;
                if (ratio < bestRatio) { bestRatio = ratio; bi = (int)j; }
            }
            if (bi >= 0) {
                Unit& a = units[bi]; a.hp = std::min(a.maxhp, a.hp + d.dmg * 2);
                u.cd = d.cd; u.atkAnim = 6;
                emit(EV_HEAL, u.owner, u.def, u.x, u.y, a.x, a.y, d.dmg * 2);
            }
        }
        followFlow(u, d, speed);
        return;
    }

    // --- поиск цели
    int enemy = 1 - u.owner;
    bool airOk = d.atk != ATK_SIEGE && (d.range >= 30 || fly);
    i64 aggro = (i64)(d.range + 40) * SUB;
    Tgt best; best.d2 = aggro * aggro + 1;
    i32 tpx = 0, tpy = 0;
    for (size_t j = 0; j < units.size(); j++) {
        const Unit& e = units[j];
        if (e.dead || e.owner != enemy) continue;
        const UnitDef& ed = raceOf(enemy).u[e.def];
        if ((ed.flags & UF_FLY) && !airOk) continue;
        i64 dx = e.x - u.x, dy = e.y - u.y, d2 = dx * dx + dy * dy;
        if (d2 < best.d2) { best.d2 = d2; best.kind = 1; best.idx = (int)j; }
    }
    for (size_t j = 0; j < blds.size(); j++) {
        const Building& b = blds[j];
        if (b.dead || b.owner != enemy) continue;
        const BuildDef& bd = raceOf(enemy).b[b.def];
        i32 px, py;
        i64 d2 = rectD2(u.x, u.y, b.cx * CELLU, b.cy * CELLU, (b.cx + bd.w) * CELLU, (b.cy + bd.h) * CELLU, px, py);
        if (d2 < best.d2) { best.d2 = d2; best.kind = 2; best.idx = (int)j; tpx = px; tpy = py; }
    }
    {
        int r0 = enemy == 1 ? 0 : MAP_H - CASTLE_H; i32 px, py;
        i64 d2 = rectD2(u.x, u.y, CASTLE_CX * CELLU, r0 * CELLU, (CASTLE_CX + CASTLE_W) * CELLU, (r0 + CASTLE_H) * CELLU, px, py);
        if (d2 < best.d2) { best.d2 = d2; best.kind = 3; tpx = px; tpy = py; }
    }
    if (best.kind == 0) { followFlow(u, d, speed); return; }

    i64 reach = (i64)(d.range + d.radius) * SUB;
    i32 txp = tpx, typ = tpy; i64 edge2 = best.d2;
    if (best.kind == 1) {
        const Unit& e = units[best.idx];
        txp = e.x; typ = e.y;
        i64 er = (i64)raceOf(enemy).u[e.def].radius * SUB;
        i64 dc = isqrt64(best.d2); edge2 = std::max<i64>(0, dc - er); edge2 *= edge2;
    }
    if (edge2 > reach * reach) {      // идём к цели
        stepToward(u, txp, typ, speed, fly);
        return;
    }
    if (u.cd > 0) return;
    // --- удар
    u.cd = d.cd; u.atkAnim = 6;
    int dmgBase = d.dmg;
    if (best.kind == 1) {
        Unit& e = units[best.idx];
        const UnitDef& ed = raceOf(enemy).u[e.def];
        i32 dmg = calcDamage(u.owner, d.atk, dmgBase, ed.arm, ed.armor, enemy);
        if (d.range > 20) emit(EV_SHOT, u.owner, u.def, u.x, u.y, e.x, e.y, 0, d.atk);
        i32 ix = e.x, iy = e.y;
        hurtUnit(e, dmg, u.owner, d.atk, u.x, u.y);
        if (d.flags & UF_SLOW) e.slow = 50;
        if (d.splash > 0) {
            i64 sr = (i64)d.splash * SUB;
            for (size_t j = 0; j < units.size(); j++) {
                Unit& o = units[j];
                if (o.dead || o.owner != enemy || (int)j == best.idx) continue;
                const UnitDef& od = raceOf(enemy).u[o.def];
                if ((od.flags & UF_FLY) && !airOk) continue;
                i64 dx = o.x - ix, dy = o.y - iy;
                if (dx * dx + dy * dy <= sr * sr) hurtUnit(o, calcDamage(u.owner, d.atk, dmgBase, od.arm, od.armor, enemy) * 6 / 10, u.owner, d.atk, u.x, u.y);
            }
        }
    } else if (best.kind == 2) {
        Building& b = blds[best.idx];
        i32 dmg = calcDamage(u.owner, d.atk, dmgBase, ARM_FORT, 5, enemy);
        if (d.range > 20) emit(EV_SHOT, u.owner, u.def, u.x, u.y, tpx, tpy, 0, d.atk);
        b.hp -= dmg;
        emit(EV_HIT, (u8)enemy, 255, tpx, tpy, u.x, u.y, dmg);
    } else {
        i32 dmg = calcDamage(u.owner, d.atk, dmgBase, ARM_FORT, 8, enemy);
        if (d.range > 20) emit(EV_SHOT, u.owner, u.def, u.x, u.y, tpx, tpy, 0, d.atk);
        hurtCastle(enemy, dmg, d.atk);
    }
}

void World::separate() {
    size_t n = units.size();
    for (size_t i = 0; i < n; i++) {
        Unit& a = units[i]; if (a.dead) continue;
        const UnitDef& ad = raceOf(a.owner).u[a.def];
        bool af = (ad.flags & UF_FLY) != 0;
        for (size_t j = i + 1; j < n; j++) {
            Unit& b = units[j]; if (b.dead) continue;
            const UnitDef& bd = raceOf(b.owner).u[b.def];
            if (((bd.flags & UF_FLY) != 0) != af) continue;
            i64 rr = (i64)(ad.radius + bd.radius) * SUB * 7 / 10;
            i64 dx = b.x - a.x, dy = b.y - a.y;
            if (absi((i32)dx) >= rr || absi((i32)dy) >= rr) continue;
            i64 d2 = dx * dx + dy * dy;
            if (d2 >= rr * rr) continue;
            i64 d = isqrt64(d2);
            if (d == 0) { dx = ((a.id + b.id) & 1) ? 8 : -8; dy = ((a.id ^ b.id) & 1) ? 8 : -8; d = isqrt64(dx * dx + dy * dy); }
            i64 push = std::min<i64>((rr - d) / 2 + 1, 6);
            i32 px = (i32)(dx * push / d), py = (i32)(dy * push / d);
            tryMove(a, -px, -py, af);
            tryMove(b, px, py, af);
        }
    }
}

// ---------------------------------------------------------------- шаг
void World::step() {
    if (winner >= 0) return;
    for (auto& c : pending) apply(c);
    pending.clear();

    if (tick > 0 && tick % INCOME_PERIOD == 0) {
        for (int p = 0; p < 2; p++) { int v = income(p); pl[p].gold += v; pl[p].earned += v; emit(EV_INCOME, (u8)p, 0, 0, 0, 0, 0, v); }
    }
    // спавн
    int cnt[2] = {countUnits(0), countUnits(1)};
    for (size_t i = 0; i < blds.size(); i++) {
        Building& b = blds[i]; if (b.dead) continue;
        if (b.spawnT > 0) b.spawnT--;
        if (b.spawnT <= 0) {
            if (cnt[b.owner] >= UNIT_CAP) continue;
            spawnUnit(b); cnt[b.owner]++;
            int base = raceOf(b.owner).b[b.def].spawn;
            b.spawnT = base * (100 - 10 * pl[b.owner].up[UP_SPAWN]) / 100;
        }
    }
    size_t n0 = units.size();
    for (size_t i = 0; i < n0; i++) if (!units[i].dead) updateUnit(units[i]);
    separate();

    // замки стреляют
    for (int p = 0; p < 2; p++) {
        if (pl[p].castleCd > 0) { pl[p].castleCd--; continue; }
        i64 bestD = (i64)CASTLE_RANGE * SUB; bestD *= bestD; int bi = -1;
        for (size_t j = 0; j < units.size(); j++) {
            const Unit& e = units[j]; if (e.dead || e.owner == p) continue;
            i64 dx = e.x - castleCenterX(p), dy = e.y - castleCenterY(p), d2 = dx * dx + dy * dy;
            if (d2 < bestD) { bestD = d2; bi = (int)j; }
        }
        if (bi >= 0) {
            Unit& e = units[bi];
            i32 dmg = calcDamage(p, ATK_NORMAL, CASTLE_DMG, raceOf(1 - p).u[e.def].arm, raceOf(1 - p).u[e.def].armor, 1 - p);
            emit(EV_SHOT, (u8)p, 255, castleCenterX(p), castleCenterY(p), e.x, e.y, 0, ATK_NORMAL);
            hurtUnit(e, dmg, p, ATK_NORMAL, castleCenterX(p), castleCenterY(p));
            pl[p].castleCd = CASTLE_CD;
        }
    }
    // внезапная смерть: слабейший замок истекает всё быстрее
    if (tick >= SUDDEN_TICKS && tick % 20 == 0) {
        i32 d = 12 + (tick - SUDDEN_TICKS) / 120;
        int weak = pl[0].castleHp < pl[1].castleHp ? 0 : (pl[1].castleHp < pl[0].castleHp ? 1 : -1);
        if (weak < 0) {   // равенство: у кого меньше войск
            i64 v[2] = {0, 0};
            for (auto& u : units) if (!u.dead) v[u.owner] += u.hp;
            weak = v[0] <= v[1] ? 0 : 1;
        }
        pl[weak].castleHp -= d;
        emit(EV_CASTLE_HIT, (u8)weak, 1, castleCenterX(weak), castleCenterY(weak), 0, 0, d, 1);
    }
    // уборка
    for (auto& b : blds) if (!b.dead && b.hp <= 0) {
        b.dead = true; const BuildDef& bd = raceOf(b.owner).b[b.def];
        for (int y = b.cy; y < b.cy + bd.h; y++) for (int x = b.cx; x < b.cx + bd.w; x++) { occ[y * MAP_W + x] = 0; occOwn[y * MAP_W + x] = 0; }
        emit(EV_BDEATH, b.owner, b.def, b.cx * CELLU + bd.w * CELLU / 2, b.cy * CELLU + bd.h * CELLU / 2, 0, 0, bd.w);
        // награда разрушителю
        int killer = 1 - b.owner; int g = bd.cost / 4; pl[killer].gold += g; pl[killer].earned += g;
    }
    units.erase(std::remove_if(units.begin(), units.end(), [](const Unit& u) { return u.dead; }), units.end());
    blds.erase(std::remove_if(blds.begin(), blds.end(), [](const Building& b) { return b.dead; }), blds.end());
    tick++;
    checkEnd();
}

void World::checkEnd() {
    if (winner >= 0) return;
    bool a = pl[0].castleHp <= 0, b = pl[1].castleHp <= 0;
    if (a && b) winner = 2; else if (a) winner = 1; else if (b) winner = 0;
    else if (tick >= OVERTIME_TICKS) winner = pl[0].castleHp > pl[1].castleHp ? 0 : (pl[1].castleHp > pl[0].castleHp ? 1 : 2);
    if (winner >= 0) emit(EV_END, 0, 0, 0, 0, 0, 0, winner);
}

u32 World::hash() const {
    u32 h = 2166136261u;
    auto mix = [&](u32 v) { h ^= v; h *= 16777619u; };
    mix((u32)tick); mix(rng);
    for (int p = 0; p < 2; p++) { mix((u32)pl[p].gold); mix((u32)pl[p].castleHp); for (int k = 0; k < NUM_UPGRADES; k++) mix(pl[p].up[k]); }
    for (auto& u : units) { mix((u32)u.id); mix((u32)u.x); mix((u32)u.y); mix((u32)u.hp); }
    for (auto& b : blds) { mix((u32)b.id); mix((u32)b.hp); mix((u32)b.spawnT); }
    return h;
}
}
