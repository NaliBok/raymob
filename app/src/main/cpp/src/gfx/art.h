// Процедурная пиксель-графика. Спрайты юнитов 16x16 (большие 32x32), здания 2x2/3x3 клеток, замок 5x3.
// Все спрайты рисуются на лету в один атлас. Если рядом с игрой лежит atlas-замена, её можно подключить позже.
#pragma once
#include "raylib.h"
namespace art {
enum { IC_COIN, IC_HEART, IC_SWORD, IC_SHIELD, IC_CLOCK, IC_LOCK, IC_STAR, IC_UP, IC_DOWN, IC_COUNT };
enum { TL_GROUND, TL_ZONE0, TL_ZONE1, TL_ROCK, TL_WATER, TL_TREE };
void init();
void shutdown();
Texture2D tex();
int unitSize(int race, int def);                         // 16 или 32
Rectangle unit(int race, int def, int owner, int frame);   // frame 0,1 - шаг, 2 - удар
Rectangle building(int race, int def, int owner);
Rectangle castle(int owner, int damaged);
Rectangle tile(int kind, int variant);
Rectangle icon(int id);
Rectangle shadow();
Color raceColor(int race);
Color teamColor(int owner);
}
