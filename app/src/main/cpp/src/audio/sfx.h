// Звук: эффекты и музыка синтезируются кодом (8-битный стиль), внешних файлов нет.
#pragma once
namespace sfx {
enum Id { S_TAP, S_BUILD, S_SELL, S_COIN, S_HIT, S_DEATH, S_CASTLE, S_WIN, S_LOSE, S_ERROR, S_SHOT, S_UPGRADE, S_COUNT };
void init();
void shutdown();
void play(Id id);
void setEnabled(bool sound, bool music);
void update();          // вызывать каждый кадр (зацикливает музыку)
}
