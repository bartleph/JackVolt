// ============================================================================
//  JACK VOLT - a run-and-gun scrolling platform game for Windows
//  volt.h - what all the parts of the program share
// ============================================================================

#ifndef VOLT_H
#define VOLT_H

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ---- The picture: drawn at this size, then scaled to fit the window
#define GAME_W      640
#define GAME_H      400
#define VIEW_H      352                 // the play area; the status bar is below it
#define TILE        32                  // a map square on screen (16 x 16 art, doubled)

// ---- Levels
#define MAX_W       128                 // the biggest map, in squares
#define MAX_H       64
#define MAX_LEVELS  16
#define LEVEL_FILE  "levels.txt"
#define SCORE_FILE  "scores.txt"

// ---- The hero
#define HERO_W      20                  // the part of him that touches things
#define HERO_H      56
#define MAX_HEALTH  8
#define MAX_FIRE    4                   // shots on screen at once, at full power

// ---- Things in the level
#define MAX_ACTORS  200
#define MAX_SHOTS   40
#define MAX_BITS    300                 // sparks and floating scores

// ---- Game states
enum { ST_TITLE, ST_PLAYING, ST_DYING, ST_DONE, ST_VICTORY, ST_SCORES, ST_EDIT };

// ---- Kinds of actor (anything in a level that isn't a map square)
enum
{
    A_NONE,
    A_WALKER, A_HOPPER, A_FLYER, A_TURRET, A_BOSS,              // robots
    A_GEM, A_COG, A_BATTERY, A_MEDKIT, A_POWER, A_BOOTS,       // things to pick up
    A_KEYR, A_KEYG, A_KEYB, A_KEYY,
    A_CRATE, A_EXIT
};

typedef struct
{
    int   kind;
    float x, y;                         // top left, in pixels
    int   w, h;
    float vx, vy;
    int   hp;
    int   dir;                          // -1 left, 1 right
    int   timer;
    int   flash;                        // frames left of the "hit" flash
    int   inside;                       // a crate: what it holds
    BOOL  onGround;
} Actor;

typedef struct
{
    BOOL  used;
    BOOL  enemy;                        // fired by a robot (hurts Jack)
    float x, y, vx, vy;
    int   w, h;
} Shot;

typedef struct
{
    BOOL     used;
    float    x, y, vx, vy;
    int      life;
    COLORREF col;
    char     text[8];                   // a floating score, or "" for a spark
} Bit;

typedef struct
{
    char name[40];
    int  tileset;                       // 1 to 3
    int  backdrop;                      // 1 to 3
    int  music;                         // 1 to 3
    int  w, h;
    char rows[MAX_H][MAX_W + 1];
} Level;

typedef struct
{
    int  score;
    char name[24];
} ScoreEntry;
#define NUM_SCORES 10

// ---- Shared variables
extern HWND   hwndMain;
extern BOOL   active;                   // is the window in front?
extern int    state, stateTimer, frameCount;
extern BOOL   paused;
extern BOOL   soundOn, musicOn;

extern Level  levels[MAX_LEVELS];
extern int    numLevels, levelNum;
extern Level *lv;                       // the level being played
extern char   map[MAX_H][MAX_W + 1];    // its squares, as they are now

extern float  heroX, heroY, heroVY;
extern int    heroDir, health, firePower, hurtTimer;
extern BOOL   onGround, hasBoots, keysHeld[4];
extern int    score, levelStartScore;
extern int    camX, camY;

extern Actor  actors[MAX_ACTORS];
extern int    numActors;
extern Shot   shots[MAX_SHOTS];
extern Bit    bits[MAX_BITS];
extern ScoreEntry scores[NUM_SCORES];

extern const unsigned char *simKeys;    // pretend keys, for the level checker
extern BOOL   simulating;               // no sounds or sparks while checking

// ---- game.cpp
void  NewGame(void);
void  StartLevel(int n);
void  Update(void);
void  ShowTitle(void);
void  EndGame(void);
BOOL  KeyDown(int vk);
BOOL  Solid(int tx, int ty);
char  TileAt(int tx, int ty);
void  MoveHero(void);
void  OpenDoors(char door);

// ---- draw.cpp
void  MakeFonts(void);
void  FreeFonts(void);
void  Render(void);
void  SetupBackBuffer(HDC screenDC);
void  FreeBackBuffer(void);
extern HDC   backDC;
extern HFONT fontTiny, fontSmall, fontHud, fontBig, fontTitle;
extern DWORD pal32[16];                 // the palette as 0x00RRGGBB
// Drawing pieces, also used by the editor
void  GfxClip(int x0, int y0, int x1, int y1);
void  GfxRect(int x, int y, int w, int h, DWORD c);
void  GfxSprite(int s, int x, int y, int scale, BOOL flip, int from, int to, BOOL white);
void  GfxBackdrop(int theme, int ox, int oy);
void  GfxText(HFONT f, int x, int y, const char *s, COLORREF c);
int   GfxTextWidth(HFONT f, const char *s);
void  GfxCentre(HFONT f, int y, const char *s, COLORREF c);
int   KeyColour(int k);

// ---- art.cpp (made by gen/make_art.py)
enum
{
    SPR_STAND, SPR_WALK1, SPR_WALK2, SPR_WALK3,                 // 16 x 32
    SPR_WALK4, SPR_WALK5, SPR_WALK6, SPR_JUMP,
    SPR_WALKER1, SPR_WALKER2, SPR_HOPPER1, SPR_HOPPER2,
    SPR_FLYER1, SPR_FLYER2, SPR_TURRET,
    SPR_GEM, SPR_COG, SPR_BATTERY, SPR_MEDKIT, SPR_POWER, SPR_BOOTS,
    SPR_KEY, SPR_CRATE,
    SPR_SPIKES, SPR_PIPE, SPR_PIPEH, SPR_LAMP, SPR_DOOR,
    SPR_WALL1, SPR_GIRDER1, SPR_LEDGE1, SPR_BRICK1,
    SPR_WALL2, SPR_GIRDER2, SPR_LEDGE2, SPR_BRICK2,
    SPR_WALL3, SPR_GIRDER3, SPR_LEDGE3, SPR_BRICK3,
    SPR_EXIT,                                                   // 16 x 32
    SPR_BOSS1, SPR_BOSS2,                                       // 32 x 32
    NUM_SPRITES
};
typedef struct
{
    int         w, h;
    const char *rows;                   // w * h characters: '.' or a colour 0-F
} SpriteArt;
extern const SpriteArt spriteArt[NUM_SPRITES];
extern const COLORREF  palette[16];

// ---- levels.cpp
extern const char defaultLevels[];
void  LoadLevels(void);
int   ParseLevels(const char *text);
int   ParseLevelsInto(const char *text, Level *into, int maxLevels);
char *ReadTextFile(const char *path);
BOOL  WriteTextFile(const char *path, const char *text);
void  ExePath(const char *fileName, char *path);
const char *NextLine(const char *p, char *line, int size);

// ---- editor.cpp
extern BOOL testMode;                   // playing a level from the editor
void  EnterEditor(int n);
void  EditKey(WPARAM key);
void  EditMouseDown(int x, int y, BOOL right);
void  EditMouseMove(int x, int y, WPARAM buttons);
void  EditMouseUp(void);
void  EditWheel(int delta, BOOL sideways);
BOOL  EditCanClose(void);
void  DrawEditor(void);
const char *LevelProblem(const Level *l);
BOOL  SaveLevels(void);
int   ImportStandardLevels(void);

// ---- checker.cpp
extern BOOL checkDone, checkGood;
extern char checkSummary[120];
extern unsigned char checkMark[MAX_H][MAX_W];   // 1 = Jack can stand here, 2 = missed
void  CheckLevel(int n);
void  HeroSetState(float x, float y, float vy, BOOL ground);
BOOL  HeroOnSpikes(void);

// ---- sound.cpp
enum
{
    SND_SHOOT, SND_HIT, SND_BOOM, SND_PICKUP, SND_KEY, SND_DOOR, SND_HURT,
    SND_DIE, SND_JUMP, SND_EXIT, SND_BRICK, SND_POWER, SND_ZAP, SND_START,
    SND_BIGBOOM, SND_CRATE,
    NUM_SOUNDS
};
void  SoundInit(void);
void  SoundShutdown(void);
void  SoundPump(void);
void  Play(int effect);
void  StopSounds(void);

// ---- pad.cpp
void  PadPoll(void);
BOOL  PadHeld(int vk);
int   PadPressed(void);

// ---- scores.cpp
void  LoadScores(void);
BOOL  ScoreQualifies(int s);
void  AddScore(int s, const char *name);
int   HiScore(void);

// ---- inputbox.cpp
BOOL  InputBox(HWND owner, const char *title, const char *prompt, char *buf, int size);

#endif
