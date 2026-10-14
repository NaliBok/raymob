#include "synth.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>
#include "raylib.h"

namespace synth {
namespace {
constexpr int kRate = 22050;
Sound sfx[SFX_COUNT]; Sound music[2]; bool ready = false;

struct Buf { std::vector<short> s; };
void Tone(Buf& b, float f0, float f1, float ms, float vol, float duty = 0.5f, bool noise = false) {
  int n = (int)(kRate * ms / 1000.f); float ph = 0;
  for (int i = 0; i < n; i++) {
    float t = (float)i / n, f = f0 + (f1 - f0) * t;
    ph += f / kRate; ph -= floorf(ph);
    float v = noise ? ((rand() & 1) ? 1.f : -1.f) : (ph < duty ? 1.f : -1.f);
    float env = (t < 0.05f ? t / 0.05f : 1.f) * (1.f - t) ;
    b.s.push_back((short)(v * env * vol * 32000));
  }
}
void Rest(Buf& b, float ms) { b.s.insert(b.s.end(), (size_t)(kRate * ms / 1000.f), 0); }
Sound Make(const Buf& b) {
  Wave w{}; w.frameCount = (unsigned)b.s.size(); w.sampleRate = kRate; w.sampleSize = 16; w.channels = 1;
  w.data = (void*)b.s.data();
  Sound s = LoadSoundFromWave(w);       // copies the samples
  return s;
}
float N(int semis) { return 220.f * powf(2.f, semis / 12.f); }   // A3 = 0

Sound Track(bool battle) {
  Buf b;
  // minor-key arpeggio + bass, 16 steps; battle is faster and harder
  static const int mel[16]  = {0, 3, 7, 12, 10, 7, 3, 7, 0, 3, 8, 12, 11, 8, 3, 7};
  static const int bass[16] = {-12, -12, -5, -5, -12, -12, -7, -7, -12, -12, -5, -5, -8, -8, -7, -7};
  float step = battle ? 120.f : 190.f; int bars = 2;
  for (int bar = 0; bar < bars; bar++) for (int i = 0; i < 16; i++) {
    Buf m, bs; Tone(m, N(mel[i] + (bar ? 2 : 0)), N(mel[i] + (bar ? 2 : 0)), step * 0.9f, battle ? 0.16f : 0.12f, 0.25f);
    Tone(bs, N(bass[i]), N(bass[i]), step * 0.9f, 0.13f, 0.5f);
    size_t n = std::max(m.s.size(), bs.s.size());
    for (size_t k = 0; k < n; k++) { int v = (k < m.s.size() ? m.s[k] : 0) + (k < bs.s.size() ? bs.s[k] : 0); b.s.push_back((short)v); }
    Rest(b, step * 0.1f);
  }
  return Make(b);
}
}

void Init() {
  InitAudioDevice();
  Buf b;
  auto mk = [&](Sfx id) { sfx[id] = Make(b); b.s.clear(); };
  Tone(b, 900, 900, 40, 0.3f, 0.25f); mk(CLICK);
  Tone(b, 200, 400, 70, 0.35f); Tone(b, 400, 700, 90, 0.35f); mk(BUILD);
  Tone(b, 600, 900, 50, 0.2f, 0.25f); mk(SPAWN);
  Tone(b, 0, 0, 40, 0.25f, 0.5f, true); mk(HIT);
  Tone(b, 120, 60, 220, 0.5f, 0.5f, true); mk(CASTLE);
  for (int i : {0, 4, 7, 12}) { Tone(b, N(i + 12), N(i + 12), 110, 0.3f, 0.25f); }
  Tone(b, N(24), N(24), 320, 0.3f, 0.25f); mk(WIN);
  for (int i : {7, 3, 0, -5}) { Tone(b, N(i), N(i), 160, 0.3f, 0.5f); }
  mk(LOSE);
  Tone(b, 140, 100, 120, 0.3f, 0.5f); mk(DENY);
  Tone(b, 1200, 1200, 40, 0.25f, 0.25f); Tone(b, 1700, 1700, 80, 0.25f, 0.25f); mk(COIN);
  music[0] = Track(false); music[1] = Track(true);
  ready = true;
}
void Shutdown() { if (ready) CloseAudioDevice(); ready = false; }
void Play(Sfx s) { if (ready && IsAudioDeviceReady() && sfx[s].frameCount) PlaySound(sfx[s]); }

void Update(bool sfxOn, bool musicOn, bool inGame) {
  if (!ready) return;
  static int cur = -1;
  for (int i = 0; i < 2; i++) SetSoundVolume(music[i], musicOn ? 0.5f : 0.f);
  for (int i = 0; i < SFX_COUNT; i++) SetSoundVolume(sfx[i], sfxOn ? 1.f : 0.f);
  int want = inGame ? 1 : 0;
  if (cur != want) { if (cur >= 0) StopSound(music[cur]); cur = want; }
  if (musicOn && !IsSoundPlaying(music[want])) PlaySound(music[want]);
  if (!musicOn && IsSoundPlaying(music[want])) StopSound(music[want]);
}
}
