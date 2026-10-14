// Пиксельный шрифт 5x7 (кириллица + латиница + цифры), заглавные буквы. Рисуется из текстуры-атласа.
#pragma once
#include "raylib.h"
namespace font {
void init();
void shutdown();
int width(const char* utf8);                       // ширина строки в пикселях
void draw(const char* utf8, int x, int y, Color c);
void drawShadow(const char* utf8, int x, int y, Color c, Color shadow);
void drawCenter(const char* utf8, int cx, int y, Color c, bool shadow = true);
void drawScaled(const char* utf8, int x, int y, Color c, int scale);
int wrap(const char* utf8, int maxW, int x, int y, Color c, int lineH = 9);   // возвращает высоту
}
