#include "data.h"
#include <cstring>

namespace sim {

const char* ATK_NAMES[ATK_COUNT] = {"Обычная", "Пронзающая", "Осадная", "Магия", "Хаос", "Герой"};
const char* ARM_NAMES[ARM_COUNT] = {"Лёгкая", "Средняя", "Тяжёлая", "Крепость", "Герой", "Без брони", "Божеств."};
// base: стандартная таблица Warcraft III. Строки - тип атаки, столбцы - тип брони (проценты)
//                             Лёгк Средн Тяж Креп Герой БезБр Божест
const int DMG_MULT[ATK_COUNT][ARM_COUNT] = {
    /* Normal */ {100, 150, 100,  70, 100, 100,   5},
    /* Pierce */ {200,  75, 100,  35,  50, 150,   5},
    /* Siege  */ {100,  50, 100, 150,  50, 150,   5},
    /* Magic  */ {125,  75, 200,  35,  50, 100,   5},
    /* Chaos  */ {100, 100, 100, 100, 100, 100, 100},
    /* Hero   */ {100, 100, 100,  50, 100, 100,   5},
};

// Казна (Treasure Box) из карты: множитель дохода +25% за уровень. Остальное - guess.
const UpgradeDef UPGRADES[NUM_UPGRADES] = {
    {"Оружие",    "+12% к урону",          3, {120, 220, 380, 0, 0}},
    {"Доспехи",   "+2 к броне юнитов",     3, {120, 220, 380, 0, 0}},
    {"Живучесть", "+12% здоровья новых",   3, {110, 200, 340, 0, 0}},
    {"Казна",     "+25% к доходу",         5, {150, 250, 400, 600, 900}},
    {"Кузнецы",   "-10% время создания",   2, {150, 260, 0, 0, 0}},
    {"Стены",     "+500 к замку и ремонт", 3, {140, 240, 380, 0, 0}},
};

extern const RaceDef RACES_GEN[RACE_COUNT];
const RaceDef& race(int id) { return RACES_GEN[id < 0 || id >= RACE_COUNT ? 0 : id]; }

i64 isqrt64(i64 v) {
    if (v <= 0) return 0;
    int bits = 0; i64 t = v; while (t) { bits++; t >>= 1; }
    i64 x = (i64)1 << ((bits + 1) / 2), y;
    for (;;) { y = (x + v / x) / 2; if (y >= x) break; x = y; }
    return x;
}

// ---------- карты ----------
// Задаётся верхняя половина (22 строки), нижняя - зеркало. 'z' зона застройки, '.' земля,
// '#' скалы, '~' вода, '^' дерево. Замки ставятся автоматически.
static const char* HALF_A[22] = {
 "^..^.........^.", "...............", ".^............^",
 "zzzzzzzzzzzzzzz", "zzzzzzzzzzzzzzz", "zzzzzzzzzzzzzzz", "zzzz^zzzzzz^zzz", "zzzzzzzzzzzzzzz",
 "zzzzzzzzzzzzzzz", "zzzzzzzzzzzzzzz", "zzzzzzzzzzzzzzz", "zzzzzzzzzzzzzzz",
 "...............", "...............", "...............", "...............", "...............",
 "...............", "....^.....^....", "...............", "...............", ".....#...#.....",
};
static const char* HALF_B[22] = {
 "^..^.........^.", "...............", ".^............^",
 "zzzzzzzzzzzzzzz", "zzzzzzzzzzzzzzz", "zzzzzzzzzzzzzzz", "zzz^zzzzzzzz^zz", "zzzzzzzzzzzzzzz",
 "zzzzzzzzzzzzzzz", "zzzzzzzzzzzzzzz", "zzzzzzzzzzzzzzz", "zzzzzzzzzzzzzzz",
 "...............", "...............", "###.........###", "####.......####", "#####.....#####",
 "####.......####", "###....#....###", "##.............", "...............", "........#......",
};
static const char* HALF_C[22] = {
 "^..^.........^.", "...............", ".^............^",
 "zzzzzzzzzzzzzzz", "zzzzzzzzzzzzzzz", "zzzzzzzzzzzzzzz", "zzzzzzzzzzzzzzz", "zzzzz^zzz^zzzzz",
 "zzzzzzzzzzzzzzz", "zzzzzzzzzzzzzzz", "zzzzzzzzzzzzzzz", "zzzzzzzzzzzzzzz",
 "...............", "..^............", "...~~~.....~~~.", "..~~~~~...~~~~.", "..~~~~~~..~~~~.",
 "...~~~~~..~~~..", "....~~~~...~...", "..............^", "....^..........", "...............",
};
static const char* MAP_NAMES[MAP_COUNT] = {"Равнина", "Теснина", "Озёра"};
const char* mapName(int id) { return MAP_NAMES[(id % MAP_COUNT + MAP_COUNT) % MAP_COUNT]; }

Map makeMap(int id) {
    Map m; m.id = (id % MAP_COUNT + MAP_COUNT) % MAP_COUNT; strncpy(m.name, mapName(id), 23);
    const char** half = m.id == 0 ? HALF_A : (m.id == 1 ? HALF_B : HALF_C);
    m.tile.assign(MAP_W * MAP_H, T_GROUND);
    for (int r = 0; r < 22; r++) {
        for (int c = 0; c < MAP_W; c++) {
            char ch = half[r][c];
            u8 t = T_GROUND;
            if (ch == '#') t = T_ROCK; else if (ch == '~') t = T_WATER; else if (ch == '^') t = T_TREE; else if (ch == 'z') t = T_ZONE;
            m.tile[r * MAP_W + c] = t;
            m.tile[(MAP_H - 1 - r) * MAP_W + c] = t;
        }
    }
    for (int c = CASTLE_CX; c < CASTLE_CX + CASTLE_W; c++) for (int r = 0; r < CASTLE_H; r++) {
        m.tile[r * MAP_W + c] = T_GROUND; m.tile[(MAP_H - 1 - r) * MAP_W + c] = T_GROUND;
    }
    return m;
}
}
