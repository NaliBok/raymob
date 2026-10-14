// Прогресс игрока. Файл подписан контрольной суммой с солью (защита от случайной порчи и простой правки).
#pragma once
#include <cstdint>
struct SaveData {
    uint32_t magic = 0x43463031; uint16_t ver = 1;
    uint8_t vibration = 1, sound = 1, music = 1, pad = 0;
    uint32_t unlocked = 0x5;       // битовая маска рас (Люди и Орки открыты сразу)
    int32_t campaign = 0;          // сколько миссий пройдено
    int32_t wins = 0, losses = 0, lastRace = 0;
    uint32_t check = 0;
};
bool loadSave(SaveData& s);
void writeSave(SaveData& s);
