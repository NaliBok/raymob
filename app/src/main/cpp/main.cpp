#if defined(_WIN32)
    #define _WINSOCK_DEPRECATED_NO_WARNINGS
    #define WIN32_LEAN_AND_MEAN
    #define NOGDI             // Отключает Rectangle и графику Windows
    #define NOUSER            // Отключает CloseWindow и интерфейс Windows
#endif

#include "raylib.h"

// Исправляем конфликт DrawText на Windows
#if defined(_WIN32)
    #undef DrawText
#endif

#include <iostream>
#include <vector>
#include <cstring>

// Настройка кроссплатформенных сокетов
#if defined(_WIN32)
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    typedef int SOCKET;
    #define INVALID_SOCKET -1
    #define closesocket close
#endif

// Константы игрового мира
const int CELL_SIZE = 30;
const int COLS = 26;
const int ROWS = 16;
const int SCREEN_WIDTH = COLS * CELL_SIZE;
const int SCREEN_HEIGHT = ROWS * CELL_SIZE;

enum GameState { STATE_MENU, STATE_WAITING, STATE_GAMEPING, STATE_GAMEOVER };
enum GameStyle { STYLE_ASCII, STYLE_PIXEL };

struct Vector2i { int x; int y; };

// Сетевые пакеты
struct GamePacket {
    int posX, posY;
    int isDead;
};

// Сетевой менеджер с поддержкой Broadcast поиска
class NetworkManager {
public:
    SOCKET sock;
    SOCKET bcastSock; 
    sockaddr_in peerAddr;
    bool isServer = false;
    bool connected = false;

    void Init() {
#if defined(_WIN32)
        WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    }

    void StartHost(int port) {
        isServer = true;
        connected = false;
        
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        SetNonBlocking(sock);
        sockaddr_in local;
        memset(&local, 0, sizeof(local));
        local.sin_family = AF_INET;
        local.sin_port = htons(port);
        local.sin_addr.s_addr = INADDR_ANY;
        bind(sock, (sockaddr*)&local, sizeof(local));

        bcastSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        SetNonBlocking(bcastSock);
        sockaddr_in bcastLocal;
        memset(&bcastLocal, 0, sizeof(bcastLocal));
        bcastLocal.sin_family = AF_INET;
        bcastLocal.sin_port = htons(port + 1); 
        bcastLocal.sin_addr.s_addr = INADDR_ANY;
        bind(bcastSock, (sockaddr*)&bcastLocal, sizeof(bcastLocal));
    }

    void StartClient(const char* ip, int port) {
        isServer = false;
        connected = false;
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        SetNonBlocking(sock);

        memset(&peerAddr, 0, sizeof(peerAddr));
        peerAddr.sin_family = AF_INET;
        peerAddr.sin_port = htons(port);
        peerAddr.sin_addr.s_addr = inet_addr(ip);
    }

    void SendDiscoveryBroadcast(int port) {
        SOCKET scanSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        int broadcastEnable = 1;
        setsockopt(scanSock, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcastEnable, sizeof(broadcastEnable));
        
        sockaddr_in target;
        memset(&target, 0, sizeof(target));
        target.sin_family = AF_INET;
        target.sin_port = htons(port + 1);
        target.sin_addr.s_addr = INADDR_BROADCAST; 

        const char* msg = "DISCOVER_SNAKE_HOST";
        sendto(scanSock, msg, strlen(msg), 0, (sockaddr*)&target, sizeof(target));
        closesocket(scanSock);
    }

    void HandleBroadcastRequests(int gamePort) {
        if (!isServer) return;
        char buf;
        sockaddr_in from;
        socklen_t fromLen = sizeof(from);
        int bytes = recvfrom(bcastSock, &buf, sizeof(buf) - 1, 0, (sockaddr*)&from, &fromLen);
        if (bytes > 0) {
            if (buf == 'D') { // Упрощенная проверка заголовка бродкаста
                const char* reply = "SNAKE_HOST_HERE";
                from.sin_port = htons(gamePort); 
                sendto(sock, reply, strlen(reply), 0, (sockaddr*)&from, sizeof(from));
            }
        }
    }

    void SendGameData(GamePacket p) {
        if (isServer && !connected) return;
        sendto(sock, (const char*)&p, sizeof(p), 0, (sockaddr*)&peerAddr, sizeof(peerAddr));
    }

    bool ReceiveGameData(GamePacket& p, char* outDiscoveredIP = nullptr) {
        sockaddr_in from;
        socklen_t fromLen = sizeof(from);
        char buf[sizeof(GamePacket) + 32];
        int bytes = recvfrom(sock, buf, sizeof(buf) - 1, 0, (sockaddr*)&from, &fromLen);
        
        if (bytes > 0) {
            buf[bytes] = '\0';
            if (!isServer && strcmp(buf, "SNAKE_HOST_HERE") == 0) {
                if (outDiscoveredIP) {
                    strcpy(outDiscoveredIP, inet_ntoa(from.sin_addr));
                }
                return false;
            }
            
            if (bytes == sizeof(GamePacket)) {
                memcpy(&p, buf, sizeof(GamePacket));
                if (isServer && !connected) {
                    peerAddr = from;
                    connected = true;
                }
                return true;
            }
        }
        return false;
    }

    void Clean() {
        closesocket(sock);
        if (isServer) closesocket(bcastSock);
#if defined(_WIN32)
        WSACleanup();
#endif
    }

private:
    void SetNonBlocking(SOCKET s) {
#if defined(_WIN32)
        unsigned long mode = 1; ioctlsocket(s, FIONBIO, &mode);
#else
        fcntl(s, F_SETFL, O_NONBLOCK);
#endif
    }
};

bool DrawButton(Rectangle rect, const char* text, Color baseColor, Color textColor) {
    Vector2 mousePos = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mousePos, rect);
    Color drawColor = hovered ? ColorAlpha(baseColor, 0.8f) : baseColor;
    
    DrawRectangleRec(rect, drawColor);
    DrawRectangleLinesEx(rect, 2, textColor);
    
    int fontSize = 20;
    int textWidth = MeasureText(text, fontSize);
    DrawText(text, rect.x + (rect.width/2) - (textWidth/2), rect.y + (rect.height/2) - (fontSize/2), fontSize, textColor);
    
    return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Flexible P2P Network Snake");
    SetTargetFPS(60);

    NetworkManager net;
    net.Init();

    GameState state = STATE_MENU;
    GameStyle currentStyle = STYLE_ASCII;

    char ipBuffer[16] = "192.168.1.100";
    int ipLetterCount = strlen(ipBuffer);
    bool ipInputActive = false;

    std::vector<Vector2i> mySnake = {{5, 5}, {4, 5}, {3, 5}};
    Vector2i myDir = {1, 0};
    bool myDead = false;
    Vector2i peerPos = {-1, -1};
    bool peerDead = false;

    float moveTimer = 0.0f;
    float moveSpeed = 0.15f;

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_SPACE)) currentStyle = (currentStyle == STYLE_ASCII) ? STYLE_PIXEL : STYLE_ASCII;

        switch (state) {
            case STATE_MENU: {
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    Vector2 m = GetMousePosition();
                    if (CheckCollisionPointRec(m, { (float)SCREEN_WIDTH/2 - 120, 210, 240, 40 })) ipInputActive = true;
                    else ipInputActive = false;
                }

                if (ipInputActive) {
                    int key = GetCharPressed();
                    while (key > 0) {
                        if ((key >= '0' && key <= '9') || key == '.') {
                            if (ipLetterCount < 15) {
                                ipBuffer[ipLetterCount] = (char)key;
                                ipBuffer[ipLetterCount+1] = '\0';
                                ipLetterCount++;
                            }
                        }
                        key = GetCharPressed();
                    }
                    if (IsKeyPressed(KEY_BACKSPACE)) {
                        ipLetterCount--;
                        if (ipLetterCount < 0) ipLetterCount = 0;
                        ipBuffer[ipLetterCount] = '\0';
                    }
                }
                break;
            }
            case STATE_WAITING: {
                if (net.isServer) {
                    net.HandleBroadcastRequests(8888);
                    GamePacket dummy;
                    if (net.ReceiveGameData(dummy)) state = STATE_GAMEPING;
                } else {
                    GamePacket dummy;
                    char discoveredIP[16] = {0};
                    net.ReceiveGameData(dummy, discoveredIP);
                    if (strlen(discoveredIP) > 0) {
                        net.StartClient(discoveredIP, 8888);
                        state = STATE_GAMEPING;
                    }
                }
                break;
            }
            case STATE_GAMEPING: {
                if (IsKeyPressed(KEY_W) && myDir.y != 1)  myDir = {0, -1};
                if (IsKeyPressed(KEY_S) && myDir.y != -1) myDir = {0, 1};
                if (IsKeyPressed(KEY_A) && myDir.x != 1)  myDir = {-1, 0};
                if (IsKeyPressed(KEY_D) && myDir.x != -1) myDir = {1, 0};

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    Vector2 touch = GetMousePosition();
                    float normX = touch.x / SCREEN_WIDTH;
                    float normY = touch.y / SCREEN_HEIGHT;
                    if (normY < normX && normY < (1.0f - normX) && myDir.y != 1)  myDir = {0, -1};
                    if (normY > normX && normY > (1.0f - normX) && myDir.y != -1) myDir = {0, 1};
                    if (normX < normY && normX < (1.0f - normY) && myDir.x != 1)  myDir = {-1, 0};
                    if (normX > normY && normX > (1.0f - normY) && myDir.x != -1) myDir = {1, 0};
                }

                moveTimer += GetFrameTime();
                if (moveTimer >= moveSpeed && !myDead) {
                    moveTimer = 0.0f;
                    for (size_t i = mySnake.size() - 1; i > 0; i--) mySnake[i] = mySnake[i - 1];
                    mySnake[0].x += myDir.x; mySnake[0].y += myDir.y;
if (mySnake[0].x < 0 || mySnake[0].x >= COLS || mySnake[0].y < 0 || mySnake[0].y >= ROWS) {
myDead = true;
}
net.SendGameData({ mySnake[0].x, mySnake[0].y, (int)myDead });
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
