// Бот. Уровни 0..4: Новичок, Ученик, Воин, Мастер, Гений.
// Ошибки на низких уровнях искусственные (задержка, случайные решения, слепота к составу врага).
// Гений использует контрпики и прогон будущего боя на копии симуляции. Читов с ресурсами нет.
#pragma once
#include "sim/sim.h"
#include <vector>

namespace ai {

extern const char* LEVEL_NAMES[5];

class Bot {
public:
    Bot(int player = 1, int level = 2, unsigned seed = 7);
    void think(const sim::World& w, std::vector<sim::Command>& out);
    int level() const { return lvl_; }
private:
    struct Cand { int def, cx, cy; double score; };
    int me_, lvl_; unsigned rng_; int nextThink_ = 40;
    unsigned rnd() { rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5; return rng_; }
    double rnd01() { return (rnd() & 0xFFFF) / 65536.0; }
    bool pickSpot(const sim::World& w, int def, int& cx, int& cy, bool noisy);
    double unitScore(const sim::World& w, int def);
    bool tryUpgrade(const sim::World& w, sim::Command& c);
    double rollout(const sim::World& w, const sim::Command& c, int ticks);
};
}
