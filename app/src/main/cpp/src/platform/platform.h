// Платформенный слой: вибрация и защищённое хранилище. На Android - через Raymob, на ПК - обычные файлы.
#pragma once
namespace plat {
void vibrate(int ms);
bool save(const char* name, const void* data, int size);
int load(const char* name, void* out, int maxSize);    // возвращает прочитанный размер или 0
}
