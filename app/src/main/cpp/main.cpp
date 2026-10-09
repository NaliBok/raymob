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
}ameData({ mySnake[0].x, mySnake[0].y, (int)myDead });
}
GamePacket inPack;
while (net.ReceiveGameData(inPack)) {
peerPos.x = inPack.posX;
peerPos.y = inPack.posY;
peerDead = (bool)inPack.isDead;
if (!net.connected) net.connected = true;
}
if (myDead && peerDead) state = STATE_GAMEOVER;
break;
}
case STATE_GAMEOVER: {
if (IsKeyPressed(KEY_ENTER)) {
mySnake = {{5, 5}, {4, 5}, {3, 5}};
myDir = {1, 0}; myDead = false; peerDead = false;
state = STATE_MENU;
net.Clean();
}
break;
}
}
// --- ОТРИСОВКА ---
BeginDrawing();
ClearBackground(BLACK);
if (state == STATE_MENU) {
DrawText("MULTIPLAYER LAN SNAKE", SCREEN_WIDTH/2 - 160, 50, 26, GREEN);
if (DrawButton({ (float)SCREEN_WIDTH/2 - 120, 130, 240, 45 }, "HOST GAME", DARKGREEN, WHITE)) {
net.StartHost(8888);
state = STATE_WAITING;
}
DrawRectangleRec({ (float)SCREEN_WIDTH/2 - 120, 210, 240, 40 }, ipInputActive ? DARKGRAY : GRAY);
DrawRectangleLinesEx({ (float)SCREEN_WIDTH/2 - 120, 210, 240, 40 }, 2, ipInputActive ? GREEN : GRAY);
DrawText(ipBuffer, SCREEN_WIDTH/2 - 100, 220, 20, WHITE);
DrawText("Target IP (For Manual Join):", SCREEN_WIDTH/2 - 120, 190, 14, GRAY);
if (DrawButton({ (float)SCREEN_WIDTH/2 - 120, 270, 240, 45 }, "CONNECT MANUAL", BLUE, WHITE)) {
net.StartClient(ipBuffer, 8888);
net.SendGameData({mySnake[0].x, mySnake[0].y, 0});
state = STATE_GAMEPING;
}
if (DrawButton({ (float)SCREEN_WIDTH/2 - 120, 340, 240, 45 }, "SCAN LAN (AUTO JOIN)", ORANGE, WHITE)) {
net.StartClient("255.255.255.255", 8888);
net.SendDiscoveryBroadcast(8888);
state = STATE_WAITING;
}
}
else if (state == STATE_WAITING) {
if (net.isServer) {
DrawText("YOU ARE HOSTING!", SCREEN_WIDTH/2 - 110, SCREEN_HEIGHT/2 - 40, 22, GREEN);
DrawText("Waiting for mobile/PC client to join...", SCREEN_WIDTH/2 - 170, SCREEN_HEIGHT/2, 18, WHITE);
} else {
DrawText("SCANNING LOCAL NETWORK...", SCREEN_WIDTH/2 - 140, SCREEN_HEIGHT/2 - 20, 20, ORANGE);
DrawText("(Ensure Host has pressed 'Host Game')", SCREEN_WIDTH/2 - 150, SCREEN_HEIGHT/2 + 10, 14, GRAY);
}
}
else if (state == STATE_GAMEPING || state == STATE_GAMEOVER) {
for (int i = 0; i <= COLS; i++) DrawLine(i * CELL_SIZE, 0, i * CELL_SIZE, SCREEN_HEIGHT, ColorAlpha(GREEN, 0.2f));
for (int i = 0; i <= ROWS; i++) DrawLine(0, i * CELL_SIZE, SCREEN_WIDTH, i * CELL_SIZE, ColorAlpha(GREEN, 0.2f));
for (size_t i = 0; i < mySnake.size(); i++) {
int rx = mySnake[i].x * CELL_SIZE; int ry = mySnake[i].y * CELL_SIZE;
if (!myDead) {
if (currentStyle == STYLE_ASCII) DrawText(i == 0 ? "@" : "o", rx + 6, ry + 2, 26, GREEN);
else DrawRectangle(rx + 2, ry + 2, CELL_SIZE - 4, CELL_SIZE - 4, LIME);
}
}
int prx = peerPos.x * CELL_SIZE; int pry = peerPos.y * CELL_SIZE;
if (peerPos.x != -1 && !peerDead) {
if (currentStyle == STYLE_ASCII) DrawText("X", prx + 6, pry + 2, 26, BLUE);
else DrawRectangle(prx + 2, pry + 2, CELL_SIZE - 4, CELL_SIZE - 4, BLUE);
}
DrawText(TextFormat("STYLE: %s (SPACE) | TOUCH EDGES TO STEER", currentStyle == STYLE_ASCII ? "ASCII" : "PIXEL"), 10, 10, 14, GRAY);
if (state == STATE_GAMEOVER) {
DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.6f));
DrawText("GAME OVER", SCREEN_WIDTH/2 - 70, SCREEN_HEIGHT/2 - 30, 26, RED);
DrawText("Press ENTER / Tap to Reset", SCREEN_WIDTH/2 - 120, SCREEN_HEIGHT/2 + 10, 18, WHITE);
if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { state = STATE_MENU; net.Clean(); }
}
}
EndDrawing();
}
net.Clean();
CloseWindow();
return 0;
}
