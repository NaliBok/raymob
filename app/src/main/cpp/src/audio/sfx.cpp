#include "sfx.h"
#include "raylib.h"
#include <vector>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdint>

namespace sfx {
struct Note { float f0, f1, sec, vol; int wave; };    // wave: 0 квадрат, 1 треугольник, 2 шум, 3 квадрат 25%
static const int SR = 22050;
static Sound snd[S_COUNT]; static Sound music; static bool ready = false, sndOn = true, musOn = true;
static double lastT[S_COUNT];

static float osc(int w, double ph, uint32_t& rng) {
    double f = ph - std::floor(ph);
    switch (w) {
        case 0: return f < 0.5 ? 1.f : -1.f;
        case 3: return f < 0.25 ? 1.f : -1.f;
        case 1: return (float)(f < 0.5 ? 4 * f - 1 : 3 - 4 * f);
        default: rng = rng * 1664525u + 1013904223u; return ((rng >> 16) & 0x7FFF) / 16384.f - 1.f;
    }
}
static Sound build(const std::vector<Note>& notes) {
    std::vector<int16_t> pcm; double ph = 0; uint32_t rng = 12345;
    for (auto& n : notes) {
        int len = (int)(n.sec * SR);
        for (int i = 0; i < len; i++) {
            double t = (double)i / len, f = n.f0 + (n.f1 - n.f0) * t;
            ph += f / SR;
            double env = (t < 0.05 ? t / 0.05 : 1.0) * (1.0 - t * 0.85);
            pcm.push_back((int16_t)(osc(n.wave, ph, rng) * n.vol * env * 9000));
        }
    }
    Wave w; w.frameCount = (unsigned)pcm.size(); w.sampleRate = SR; w.sampleSize = 16; w.channels = 1;
    w.data = malloc(pcm.size() * 2); memcpy(w.data, pcm.data(), pcm.size() * 2);
    Sound s = LoadSoundFromWave(w); UnloadWave(w); return s;
}
static Sound buildMusic() {
    // Am - F - C - G, два прохода по 8 секунд; бас (треугольник), арпеджио (квадрат 25%), хэт (шум)
    static const float root[4] = {110.f, 87.31f, 130.81f, 98.f};
    static const float tri[4][3] = {{1, 1.189f, 1.498f}, {1, 1.26f, 1.498f}, {1, 1.26f, 1.498f}, {1, 1.26f, 1.498f}};
    const double step = 0.125; int total = (int)(16.0 * SR);
    std::vector<int16_t> pcm(total, 0); uint32_t rng = 777;
    double pb = 0, pl = 0;
    for (int s = 0; s < 128; s++) {
        int chord = (s / 16) % 4, pass = s / 64, k = s % 16;
        float rt = root[chord];
        int a = (k % 4 == 0) ? 0 : (k % 4 == 2 ? 2 : 1);
        float lead = rt * 4 * tri[chord][(k * 3 + pass) % 3] * (pass ? 2.f : 1.f) * ((k % 8) == 7 ? 1.5f : 1.f);
        (void)a;
        int n0 = (int)(s * step * SR), n1 = (int)((s + 1) * step * SR);
        for (int i = n0; i < n1 && i < total; i++) {
            double t = (double)(i - n0) / (n1 - n0);
            pb += rt / SR; pl += lead / SR;
            float bass = (k % 2 == 0) ? osc(1, pb, rng) * 0.5f * (float)(1.0 - t * 0.5) : 0.f;
            float arp = osc(3, pl, rng) * 0.28f * (float)(1.0 - t);
            float hat = (k % 2 == 1) ? osc(2, 0, rng) * 0.10f * (float)(1.0 - t) : 0.f;
            pcm[i] = (int16_t)((bass + arp + hat) * 12000);
        }
    }
    Wave w; w.frameCount = total; w.sampleRate = SR; w.sampleSize = 16; w.channels = 1;
    w.data = malloc(total * 2); memcpy(w.data, pcm.data(), total * 2);
    Sound s = LoadSoundFromWave(w); UnloadWave(w); return s;
}
void init() {
    InitAudioDevice(); if (!IsAudioDeviceReady()) return;
    snd[S_TAP] = build({{880, 1200, 0.04f, 0.5f, 0}});
    snd[S_BUILD] = build({{220, 440, 0.07f, 0.7f, 1}, {440, 440, 0.06f, 0.5f, 0}, {660, 880, 0.10f, 0.5f, 3}});
    snd[S_SELL] = build({{660, 330, 0.14f, 0.5f, 0}});
    snd[S_COIN] = build({{1320, 1320, 0.05f, 0.4f, 0}, {1760, 1760, 0.10f, 0.4f, 0}});
    snd[S_HIT] = build({{0, 0, 0.05f, 0.4f, 2}});
    snd[S_DEATH] = build({{0, 0, 0.07f, 0.5f, 2}, {200, 70, 0.12f, 0.6f, 1}});
    snd[S_CASTLE] = build({{0, 0, 0.20f, 0.8f, 2}, {120, 45, 0.30f, 0.9f, 1}});
    snd[S_WIN] = build({{523, 523, 0.12f, 0.6f, 0}, {659, 659, 0.12f, 0.6f, 0}, {784, 784, 0.12f, 0.6f, 0}, {1047, 1047, 0.35f, 0.7f, 0}});
    snd[S_LOSE] = build({{440, 440, 0.2f, 0.6f, 1}, {349, 349, 0.2f, 0.6f, 1}, {294, 294, 0.2f, 0.6f, 1}, {220, 110, 0.5f, 0.7f, 1}});
    snd[S_ERROR] = build({{150, 150, 0.08f, 0.6f, 0}, {110, 110, 0.12f, 0.6f, 0}});
    snd[S_SHOT] = build({{1500, 600, 0.05f, 0.25f, 0}});
    snd[S_UPGRADE] = build({{523, 523, 0.07f, 0.5f, 3}, {659, 659, 0.07f, 0.5f, 3}, {784, 784, 0.07f, 0.5f, 3}, {1047, 1047, 0.18f, 0.5f, 3}});
    music = buildMusic(); SetSoundVolume(music, 0.35f);
    ready = true;
}
void shutdown() { if (!ready) return; for (int i = 0; i < S_COUNT; i++) UnloadSound(snd[i]); UnloadSound(music); CloseAudioDevice(); ready = false; }
void play(Id id) {
    if (!ready || !sndOn) return;
    double t = GetTime(); double gap = (id == S_HIT || id == S_SHOT) ? 0.07 : (id == S_DEATH ? 0.05 : 0.0);
    if (t - lastT[id] < gap) return;
    lastT[id] = t; PlaySound(snd[id]);
}
void setEnabled(bool s, bool m) { sndOn = s; musOn = m; if (ready && !m && IsSoundPlaying(music)) StopSound(music); }
void update() { if (ready && musOn && !IsSoundPlaying(music)) PlaySound(music); }
}
