// Приложение: экраны, ввод, отрисовка. Виртуальное разрешение с целочисленным масштабом (пиксель-арт без размытия).
#pragma once
#include "raylib.h"
#include "sim/sim.h"
#include "ai/bot.h"
#include "game/save.h"
#include <vector>
#include <cstdio>
#include <unordered_map>

enum Scr { SC_SPLASH, SC_MENU, SC_PRE, SC_CAMP, SC_RACES, SC_RACE, SC_SET, SC_HELP, SC_GAME, SC_RESULT };

struct Input { int x = 0, y = 0; bool down = false, pressed = false, released = false; int dx = 0, dy = 0; int dragTotal = 0; bool back = false; };
struct Particle { float x, y, vx, vy, life; Color c; };
struct Proj { float x0, y0, x1, y1, t; Color c; };
struct FTxt { float x, y, life; char s[16]; Color c; };

class App {
public:
    void init();
    void frame();
    void shutdown();
    bool quit = false;
    // тестовые хуки (для автоматических скриншотов)
    void testGoto(int scr) { go(scr); fade = 0; cur = nxt = scr; }
    void testStart(int myRace, int foeRace, int map, int level);
    void testTap(int x, int y) { fakeTap = true; fx = x; fy = y; }
    Image snapshot();
    void testAdvance(int ticks);
    void setFast(int n) { fastFrames = n; }
    Scr scr() const { return (Scr)cur; }

private:
    // ---- ядро
    RenderTexture2D rt{}; int scale = 1, VW = 240, VH = 480, TOPI = 12;
    Input in; bool fakeTap = false; int fx = 0, fy = 0; int pressId = 0;
    int cur = SC_SPLASH, nxt = SC_SPLASH; float fade = 1.f; float tm = 0; int fastFrames = 0;
    SaveData sd; float quitArm = 0;
    void layout(); void readInput(float dt); void go(int s);
    // ---- UI-хелперы
    void panel(int x, int y, int w, int h, Color fill, Color border);
    bool button(int x, int y, int w, int h, const char* t, int style = 0, bool enabled = true);
    bool hit(int x, int y, int w, int h) const { return in.x >= x && in.y >= y && in.x < x + w && in.y < y + h; }
    void icon(int id, int x, int y, Color tint = WHITE);
    void sprite(Rectangle r, int x, int y, Color tint = WHITE, int s = 1);
    void backdrop(float t, bool withCastle);
    void title(const char* t, int y);
    float scrollList(float s, int x, int y, int w, int h, float contentH);
    float dragS = 0; bool dragging = false;
    // ---- экраны меню
    void drawSplash(float dt); void drawMenu(); void drawPre(); void drawCamp(); void drawRaces(); void drawRace(); void drawSettings(); void drawHelp(); void drawResult();
    int menuT = 0; int preRace = 0, preFoe = 0, preMap = 0, preLevel = 2, preMission = -1; bool preRandFoe = false;
    int raceSel = 0; float scrollY = 0; int helpPage = 0; int resetArm = 0;
    // ---- игра (game.cpp)
    sim::World world; ai::Bot bot; double acc = 0; int speed = 1; bool paused = false, pauseMenu = false;
    int myRace = 0, foeRace = 1, mapId = 0, level = 2, mission = -1;
    float dtNow = 0; float camY = 0; bool mapDrag = false; int mapDragMoved = 0;
    int tab = 0, selBuild = -1, ghostX = -1, ghostY = -1, selPlaced = 0; int tickCount = 0;
    float shake = 0, flash = 0, endTimer = -1; bool won = false; float vibGap = 0; int lastMsgT = 0; char msg[48] = {}; float msgT = 0;
    std::vector<Particle> parts; std::vector<Proj> projs; std::vector<FTxt> ftx;
    std::unordered_map<int, std::pair<int, int>> prev;
    std::vector<sim::Command> cmds;
    int mapX0 = 0, mapTop = 0, mapBot = 0; int resultUnlock = -1;
    void startMatch(int mr, int fr, int map, int lvl, int mis);
    void stepSim(); void handleEvents(); void drawGame(float alpha); void gameUI(); void gameInput();
    void drawBars(); void drawBuildTab(int py); void drawUpgTab(int py); void drawInfoTab(int py); void drawPause();
    void showMsg(const char* s) { snprintf(msg, sizeof(msg), "%s", s); msgT = 1.6f; }
    void finishMatch();
    void spark(float x, float y, int n, Color c, float spd = 30);
    void worldToScr(int wx, int wy, int& sx, int& sy) const { sx = mapX0 + wx; sy = mapTop + wy - (int)camY; }
    void clampCam();
    bool placeable(int cx, int cy) const;
    void findFree(int def);
    void vib(int ms);
};

extern int g_sc;
inline void scis(int x, int y, int w, int h) { BeginScissorMode(x * g_sc, y * g_sc, w * g_sc, h * g_sc); }
extern const char* RACE_RU[12];
extern const char* RACE_DESC[12];
extern const int UNLOCK_ORDER[10];
