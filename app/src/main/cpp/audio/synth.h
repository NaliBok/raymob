#pragma once
// 8-bit style sound generated in code: no audio files.
namespace synth {
enum Sfx { CLICK, BUILD, SPAWN, HIT, CASTLE, WIN, LOSE, DENY, COIN, SFX_COUNT };
void Init();
void Shutdown();
void Play(Sfx s);
void Update(bool sfxOn, bool musicOn, bool inGame);   // call once per frame; loops the music tracks
}
