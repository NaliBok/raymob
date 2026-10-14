// Точка входа. Android (Raymob): окно на весь экран, портретная ориентация задана в манифесте.
#include "raylib.h"
#include "game/app.h"

int main() {
    SetConfigFlags(FLAG_VSYNC_HINT);
#ifdef PLATFORM_ANDROID
    InitWindow(0, 0, "game");
#else
    InitWindow(540, 960, "game");
#endif
    SetExitKey(KEY_NULL);        // кнопка «Назад» не закрывает приложение сама
    SetTargetFPS(60);
    static App app;
    app.init();
    while (!WindowShouldClose() && !app.quit) app.frame();
    app.shutdown();
    CloseWindow();
    return 0;
}
