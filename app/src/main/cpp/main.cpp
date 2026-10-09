#include "raylib.h"

int main() {
    // Разрешаем изменение размера окна
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);

    // Инициализируем окно. Сразу откроем его на весь доступный экран X11
    // (или зададим комфортное пропорциональное разрешение, например, 1080x1920 под вертикалки)
    const int screenWidth = 1080;
    const int screenHeight = 1920;
    
    InitWindow(screenWidth, screenHeight, "Castle Fight Mobile");

    // Виртуальное разрешение для логики игры (вертикальный формат мобилки)
    const int nativeWidth = 1080;
    const int nativeHeight = 1920;

    RenderTexture2D target = LoadRenderTexture(nativeWidth, nativeHeight);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);

    int playerGold = 50;
    int baseHp = 100;
    int enemyHp = 100;
    int unitsSpawned = 0;
    
    Vector2 lastTouch = {-1, -1};

    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        // --- 1. ВВОД И ЛОГИКА ---
        Vector2 touchWorldPos = { -1, -1 };
        bool isPressed = false;

        // Обработка клика мыши / тапа
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 mousePos = GetMousePosition();
            // Пересчет координат с учетом растягивания экрана
            float scaleX = (float)nativeWidth / (float)GetScreenWidth();
            float scaleY = (float)nativeHeight / (float)GetScreenHeight();
            touchWorldPos = { mousePos.x * scaleX, mousePos.y * scaleY };
            lastTouch = touchWorldPos;
            isPressed = true;
        }

        if (isPressed) {
            // Кнопка спавна юнита (По центру внизу: X: 140..940, Y: 1500..1650)
            if (touchWorldPos.x >= 140 && touchWorldPos.x <= 940 && touchWorldPos.y >= 1500 && touchWorldPos.y <= 1650) {
                if (playerGold >= 15) {
                    playerGold -= 15;
                    unitsSpawned++;
                }
            }
            // Кнопка атаки (Ниже спавна: X: 140..940, Y: 1700..1850)
            if (touchWorldPos.x >= 140 && touchWorldPos.x <= 940 && touchWorldPos.y >= 1700 && touchWorldPos.y <= 1850) {
                enemyHp -= 10;
                if (enemyHp < 0) enemyHp = 0;
            }
        }

        // --- 2. ОТРИСОВКА В БУФЕР (ВЕРТИКАЛЬНЫЙ ФОРМАТ) ---
        BeginTextureMode(target);
            ClearBackground(RAYWHITE);

            // Верхняя панель статуса
            DrawRectangle(0, 0, nativeWidth, 120, DARKGRAY);
            DrawText(TextFormat("BASE: %d", baseHp), 50, 40, 40, RED);
            DrawText(TextFormat("GOLD: %d", playerGold), 420, 40, 40, GOLD);
            DrawText(TextFormat("ENEMY: %d", enemyHp), 780, 40, 40, MAROON);

            // Игровое поле (Вертикальный Castle Fight: замок игрока внизу, враг вверху)
            DrawRectangle(200, 250, 680, 100, LIGHTGRAY); // Дорога
            
            // Башня врага (вверху)
            DrawRectangle(390, 200, 300, 150, RED);
            DrawText("ENEMY CASTLE", 430, 260, 30, WHITE);

            // Башня игрока (внизу)
            DrawRectangle(390, 1250, 300, 150, BLUE);
            DrawText("YOUR CASTLE", 440, 1310, 30, WHITE);

            // Юниты на поле
            for (int i = 0; i < unitsSpawned; i++) {
                int posY = 1150 - (i * 80) % 800;
                DrawCircle(540, posY, 25, DARKBLUE);
            }

            // --- НИЖНЯЯ ПАНЕЛЬ УПРАВЛЕНИЯ ---
            DrawRectangle(0, 1450, nativeWidth, 470, BEIGE);

            // Кнопка 1: Спавн
            DrawRectangle(140, 1500, 800, 150, DARKBLUE);
            DrawText("SPAWN UNIT (Cost: 15)", 250, 1555, 36, WHITE);

            // Кнопка 2: Атака
            DrawRectangle(140, 1700, 800, 150, MAROON);
            DrawText("ATTACK ENEMY", 370, 1755, 36, WHITE);

            // Метка последнего касания (чтобы вы видели, куда пришелся тап)
            if (lastTouch.x != -1) {
                DrawCircleV(lastTouch, 20, GREEN);
            }

        EndTextureMode();

        // --- 3. ВЫВОД НА ЭКРАН ТЕЛЕФОНА ---
        BeginDrawing();
            ClearBackground(BLACK);

            // Растягиваем вертикальный буфер на весь экран смартфона
            DrawTexturePro(
                target.texture, 
                (Rectangle){ 0, 0, (float)nativeWidth, (float)-nativeHeight }, 
                (Rectangle){ 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() }, 
                (Vector2){ 0, 0 }, 0.0f, WHITE
            );

        EndDrawing();
    }

    UnloadRenderTexture(target);
    CloseWindow();
    return 0;
}
