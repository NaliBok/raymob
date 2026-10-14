#pragma once
#include "../sim/sim.h"

namespace ai {

enum class Level { Easy, Normal, Hard, Genius };
extern int g_dbg;   // experiment switches used by tests/levels.cpp (0 in the game)

// A bot only speaks the same Cmd protocol a human does, so it can run on the client,
// on a server, or stand in for a disconnected player.
class Bot {
public:
  Bot(int player, Level level, uint32_t seed);
  void Think(sim::Sim& s);                 // call every tick

private:
  struct Params { int react; int noise; bool counters; bool smartItems; int maxTreasure; int skipPct; int waitTicks; };
  int Rand(int n);
  bool PlaceBest(sim::Sim& s, int def);
  int  ScoreDef(const sim::Sim& s, int def, const int* enemyArm, const int* enemyAtk);

  int me_; Level lvl_; Params P_; uint32_t rng_; int wait_ = 0;
};

} // namespace ai
