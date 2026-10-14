#pragma once
#include <cstdint>
// Settings + progress, stored in the app-private directory with a keyed checksum so casual edits are rejected.
struct Settings {
  bool sound = true, music = true;
  bool vib = true, vibCastle = true, vibEnd = true, vibUi = false;   // master switch + per-event switches
  int lang = 0;                                                       // 0 = English, 1 = Russian
  int wins = 1;                                                       // rounds needed to win a match (1..3)
};
struct Progress {
  uint32_t unlockedRaces = 1;       // bit per race, human is always unlocked
  uint32_t campaignDone = 0;        // bit per campaign stage
  int matchesWon = 0, matchesLost = 0;
};
extern Settings gSettings;
extern Progress gProgress;
void LoadSave();
void WriteSave();
