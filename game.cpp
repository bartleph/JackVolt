// ============================================================================
//  game.cpp - the rules: Jack, his blaster, the robots, the things to pick up
//
//  Everything moves in pixels on a 32-pixel grid of map squares. Each 1/60th
//  of a second Update() moves Jack, his shots, the robots and the sparks, and
//  keeps the camera on him.
// ============================================================================

#include "volt.h"
#include <stdio.h>

// ---- How Jack moves (pixels per frame, and per frame per frame)
#define RUN_SPEED     3.0f
#define JUMP_SPEED    9.5f
#define BOOT_SPEED    11.0f              // with the spring boots
#define GRAVITY_UP    0.4f               // while rising with jump held: a higher jump
#define GRAVITY       0.8f
#define MAX_FALL      10.0f
#define SHOT_SPEED    10.0f
#define FIRE_DELAY    10                 // frames between shots
#define HURT_TIME     90                 // frames Jack flashes after a hit

int    state, stateTimer, frameCount;
BOOL   paused, active = TRUE;

Level *lv;
int    levelNum;
char   map[MAX_H][MAX_W + 1];

float  heroX, heroY, heroVY;
int    heroDir = 1, health, firePower, hurtTimer;
BOOL   onGround, hasBoots, keysHeld[4];
int    score, levelStartScore;
int    camX, camY;
int    levelTimer;                       // frames since the level started

Actor  actors[MAX_ACTORS];
int    numActors;
Shot   shots[MAX_SHOTS];
Bit    bits[MAX_BITS];

const unsigned char *simKeys;
BOOL   simulating;

static BOOL jumpWasHeld, upWasHeld;
static int  fireTimer;
static int  startFire;                   // what Jack had when the level began,
static BOOL startBoots;                  // so dying puts it back

// ============================================================================
//  Keys
// ============================================================================

BOOL KeyDown(int vk)
{
    if (simKeys)
        return simKeys[vk & 255] != 0;
    return active && ((GetAsyncKeyState(vk) & 0x8000) || PadHeld(vk));
}

static BOOL JumpHeld(void)  { return KeyDown(VK_SPACE) || KeyDown(VK_MENU) || KeyDown('Z'); }
static BOOL FireHeld(void)  { return KeyDown(VK_CONTROL) || KeyDown('X'); }

// ============================================================================
//  The map
// ============================================================================

char TileAt(int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx >= lv->w || ty >= lv->h)
        return '#';                      // outside the map counts as wall
    return map[ty][tx];
}

// Crates are solid too: Jack can stand on them, and robots and their shots
// can't get through. This grid says which squares have a crate in them
// (the number of the crate in actors[], plus 1, or 0 for none).
static short crateAt[MAX_H][MAX_W];

int CrateAt(int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx >= MAX_W || ty >= MAX_H)
        return 0;
    return crateAt[ty][tx];
}

BOOL Solid(int tx, int ty)
{
    if (CrateAt(tx, ty))
        return TRUE;
    char c = TileAt(tx, ty);
    return c == '#' || c == '=' || c == '%' || c == 'R' || c == 'G' || c == 'B' || c == 'Y';
}

// Does a box overlap any solid square?
static BOOL BoxSolid(float x, float y, int w, int h)
{
    int x0 = (int)floorf(x / TILE), x1 = (int)floorf((x + w - 0.01f) / TILE);
    int y0 = (int)floorf(y / TILE), y1 = (int)floorf((y + h - 0.01f) / TILE);
    for (int ty = y0; ty <= y1; ty++)
        for (int tx = x0; tx <= x1; tx++)
            if (Solid(tx, ty))
                return TRUE;
    return FALSE;
}

// Would a box falling to 'newY' land on a ledge (-) it was above?
static BOOL LandsOnLedge(float x, float oldY, float newY, int w, int h)
{
    float oldBottom = oldY + h, newBottom = newY + h;
    int   ty = (int)floorf((newBottom - 0.01f) / TILE);
    if (oldBottom > ty * TILE + 0.01f)   // was already below its top: fall through
        return FALSE;
    int x0 = (int)floorf(x / TILE), x1 = (int)floorf((x + w - 0.01f) / TILE);
    for (int tx = x0; tx <= x1; tx++)
        if (TileAt(tx, ty) == '-')
            return TRUE;
    return FALSE;
}

// Move a box, stopping at walls. Says what it bumped into.
typedef struct { BOOL hitX, landed, bonked; } Bumps;

static Bumps MoveBox(float *x, float *y, int w, int h, float dx, float dy, BOOL ledges)
{
    Bumps b = { FALSE, FALSE, FALSE };

    if (dx != 0)
    {
        float nx = *x + dx;
        if (!BoxSolid(nx, *y, w, h))
            *x = nx;
        else
        {
            b.hitX = TRUE;
            if (dx > 0)                  // up against the square we ran into
                *x = floorf((nx + w - 0.01f) / TILE) * TILE - w;
            else
                *x = (floorf(nx / TILE) + 1) * TILE;
            if (BoxSolid(*x, *y, w, h))  // (shouldn't happen, but never get stuck)
                *x = nx - dx;
        }
    }

    if (dy != 0)
    {
        float ny = *y + dy;
        if (dy > 0 && (BoxSolid(*x, ny, w, h) || (ledges && LandsOnLedge(*x, *y, ny, w, h))))
        {
            b.landed = TRUE;
            *y = floorf((ny + h - 0.01f) / TILE) * TILE - h;
        }
        else if (dy < 0 && BoxSolid(*x, ny, w, h))
        {
            b.bonked = TRUE;
            *y = (floorf(ny / TILE) + 1) * TILE;
        }
        else
            *y = ny;
    }
    return b;
}

// A key opens every door of its colour
void OpenDoors(char door)
{
    for (int ty = 0; ty < lv->h; ty++)
        for (int tx = 0; tx < lv->w; tx++)
            if (map[ty][tx] == door)
                map[ty][tx] = '.';
    Play(SND_DOOR);
}

static int DoorColour(char c)
{
    switch (c)
    {
    case 'R': return 0;
    case 'G': return 1;
    case 'B': return 2;
    case 'Y': return 3;
    }
    return -1;
}

// ============================================================================
//  Sparks and floating scores
// ============================================================================

static void Spark(float x, float y, COLORREF col, int count, float speed)
{
    if (simulating)
        return;
    for (int n = 0; n < count; n++)
        for (int i = 0; i < MAX_BITS; i++)
            if (!bits[i].used)
            {
                Bit *b = &bits[i];
                float a = (float)(rand() % 628) / 100.0f;
                float s = speed * (0.3f + (float)(rand() % 100) / 100.0f);
                b->used = TRUE;
                b->x = x; b->y = y;
                b->vx = cosf(a) * s;
                b->vy = sinf(a) * s - 1.5f;
                b->life = 25 + rand() % 25;
                b->col = col;
                b->text[0] = 0;
                break;
            }
}

static void Popup(float x, float y, int points)
{
    if (simulating)
        return;
    for (int i = 0; i < MAX_BITS; i++)
        if (!bits[i].used)
        {
            Bit *b = &bits[i];
            b->used = TRUE;
            b->x = x; b->y = y;
            b->vx = 0; b->vy = -0.8f;
            b->life = 60;
            b->col = RGB(255, 255, 255);
            wsprintf(b->text, "%d", points);
            return;
        }
}

static void AddPoints(float x, float y, int points)
{
    score += points;
    Popup(x, y, points);
}

static void UpdateBits(void)
{
    for (int i = 0; i < MAX_BITS; i++)
    {
        Bit *b = &bits[i];
        if (!b->used)
            continue;
        b->x += b->vx;
        b->y += b->vy;
        if (!b->text[0])
            b->vy += 0.15f;
        if (--b->life <= 0)
            b->used = FALSE;
    }
}

// ============================================================================
//  Starting a level
// ============================================================================

static Actor *NewActor(int kind, int tx, int ty)
{
    if (numActors >= MAX_ACTORS)
        return NULL;
    Actor *a = &actors[numActors++];
    memset(a, 0, sizeof *a);
    a->kind = kind;
    a->dir  = -1;

    static const struct { int kind, w, h, hp; } sizes[] =
    {
        { A_WALKER, 28, 28, 2 }, { A_HOPPER, 26, 30, 2 }, { A_FLYER, 28, 26, 1 },
        { A_TURRET, 28, 28, 3 }, { A_BOSS, 56, 60, 30 },  { A_CRATE, 32, 32, 1 },
        { A_EXIT, 32, 64, 0 },
    };
    a->w = a->h = 24;                    // things to pick up
    for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; i++)
        if (sizes[i].kind == kind)
        {
            a->w  = sizes[i].w;
            a->h  = sizes[i].h;
            a->hp = sizes[i].hp;
        }
    // Standing on the bottom of its square, in the middle
    a->x = (float)(tx * TILE + (TILE - a->w) / 2);
    a->y = (float)(ty * TILE + TILE - a->h);
    if (kind == A_FLYER)
        a->y -= 4;
    a->timer = 30 + (tx * 7 + ty * 13) % 60;    // so robots don't all act at once
    return a;
}

static int KindFor(char c, int *inside)
{
    *inside = A_NONE;
    switch (c)
    {
    case 'E': return A_WALKER;
    case 'J': return A_HOPPER;
    case 'F': return A_FLYER;
    case 'T': return A_TURRET;
    case 'K': return A_BOSS;
    case '$': return A_GEM;
    case 'o': return A_COG;
    case 'h': return A_BATTERY;
    case '+': return A_MEDKIT;
    case 'f': return A_POWER;
    case 'j': return A_BOOTS;
    case 'r': return A_KEYR;
    case 'g': return A_KEYG;
    case 'b': return A_KEYB;
    case 'y': return A_KEYY;
    case 'X': return A_EXIT;
    case '0': return A_CRATE;                           // empty crate
    case '1': *inside = A_GEM;     return A_CRATE;      // crates with something inside
    case '2': *inside = A_BATTERY; return A_CRATE;
    case '3': *inside = A_POWER;   return A_CRATE;
    case '4': *inside = A_MEDKIT;  return A_CRATE;
    case '5': *inside = A_COG;     return A_CRATE;
    }
    return A_NONE;
}

void StartLevel(int n)
{
    levelNum = n;
    lv = &levels[n];
    numActors = 0;
    memset(shots, 0, sizeof shots);
    memset(bits, 0, sizeof bits);
    memset(keysHeld, 0, sizeof keysHeld);
    memset(crateAt, 0, sizeof crateAt);
    heroX = heroY = TILE;

    for (int ty = 0; ty < lv->h; ty++)
    {
        memcpy(map[ty], lv->rows[ty], MAX_W + 1);
        for (int tx = 0; tx < lv->w; tx++)
        {
            char c = map[ty][tx];
            int  inside;
            int  kind = KindFor(c, &inside);
            if (c == 'P')
            {
                heroX = (float)(tx * TILE + (TILE - HERO_W) / 2);
                heroY = (float)(ty * TILE + TILE - HERO_H);
                map[ty][tx] = '.';
            }
            else if (kind != A_NONE)
            {
                Actor *a = NewActor(kind, tx, ty);
                if (a)
                {
                    a->inside = inside;
                    if (kind == A_CRATE)
                        crateAt[ty][tx] = (short)(a - actors + 1);
                }
                map[ty][tx] = '.';
            }
        }
    }

    heroVY      = 0;
    heroDir     = 1;
    onGround    = FALSE;
    health      = MAX_HEALTH;
    hurtTimer   = 0;
    fireTimer   = 0;
    jumpWasHeld = TRUE;                  // don't jump just because Space started the game
    upWasHeld   = TRUE;
    levelTimer  = 0;
    startFire   = firePower;
    startBoots  = hasBoots;
    levelStartScore = score;
    state  = ST_PLAYING;
    paused = FALSE;
    stateTimer = 0;

    // Put the camera on Jack straight away
    camX = (int)heroX - GAME_W / 2;
    camY = (int)heroY - VIEW_H / 2;
}

void NewGame(void)
{
    score     = 0;
    firePower = 1;
    hasBoots  = FALSE;
    Play(SND_START);
    StartLevel(0);
}

void ShowTitle(void)
{
    state = ST_TITLE;
    stateTimer = 0;
    paused = FALSE;
}

void EndGame(void)
{
    StopSounds();
    if (ScoreQualifies(score))
    {
        static char name[24] = "";
        char msg[80];
        wsprintf(msg, "You scored %d - a new high score! Your name:", score);
        if (InputBox(hwndMain, "Jack Volt", msg, name, sizeof name))
            AddScore(score, name);
        else
            AddScore(score, "Anon");
        state = ST_SCORES;
        stateTimer = 0;
        return;
    }
    ShowTitle();
}

// ============================================================================
//  Jack
// ============================================================================

static void Hurt(void)
{
    if (hurtTimer > 0 || state != ST_PLAYING)
        return;
    health--;
    hurtTimer = HURT_TIME;
    if (health <= 0)
    {
        state = ST_DYING;
        stateTimer = 0;
        Play(SND_DIE);
        Spark(heroX + HERO_W / 2, heroY + HERO_H / 2, palette[12], 30, 4);
        Spark(heroX + HERO_W / 2, heroY + HERO_H / 2, palette[7], 20, 3);
    }
    else
        Play(SND_HURT);
}

void MoveHero(void)
{
    int dir = (KeyDown(VK_RIGHT) ? 1 : 0) - (KeyDown(VK_LEFT) ? 1 : 0);
    if (dir)
        heroDir = dir;

    // Jumping: a fresh press while standing on something
    BOOL jump = JumpHeld();
    if (jump && !jumpWasHeld && onGround)
    {
        heroVY   = hasBoots ? -BOOT_SPEED : -JUMP_SPEED;
        onGround = FALSE;
        Play(SND_JUMP);
    }
    jumpWasHeld = jump;

    // Left and right, opening any door he walks into with the right key
    float oldX = heroX;
    Bumps b = MoveBox(&heroX, &heroY, HERO_W, HERO_H, dir * RUN_SPEED, 0, TRUE);
    if (b.hitX && !simKeys)
    {
        int tx = (dir > 0) ? (int)floorf((oldX + HERO_W + RUN_SPEED) / TILE)
                           : (int)floorf((oldX - RUN_SPEED) / TILE);
        for (int ty = (int)(heroY / TILE); ty <= (int)((heroY + HERO_H - 1) / TILE); ty++)
        {
            int k = DoorColour(TileAt(tx, ty));
            if (k >= 0 && keysHeld[k])
                OpenDoors(TileAt(tx, ty));
        }
    }

    // Up and down: a higher jump while jump is held on the way up
    heroVY += (heroVY < 0 && jump) ? GRAVITY_UP : GRAVITY;
    if (heroVY > MAX_FALL)
        heroVY = MAX_FALL;
    b = MoveBox(&heroX, &heroY, HERO_W, HERO_H, 0, heroVY, TRUE);
    onGround = b.landed;
    if (b.landed || b.bonked)
        heroVY = 0;
}

// Is Jack touching spikes? (Only their pointed half counts.)
BOOL HeroOnSpikes(void)
{
    int x0 = (int)(heroX / TILE), x1 = (int)((heroX + HERO_W - 1) / TILE);
    int y0 = (int)((heroY) / TILE), y1 = (int)((heroY + HERO_H - 1) / TILE);
    for (int ty = y0; ty <= y1; ty++)
        for (int tx = x0; tx <= x1; tx++)
            if (TileAt(tx, ty) == '^' && heroY + HERO_H > ty * TILE + 12)
                return TRUE;
    return FALSE;
}

static BOOL Overlap(float ax, float ay, int aw, int ah, float bx, float by, int bw, int bh)
{
    return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

static void Fire(void)
{
    if (fireTimer > 0)
        fireTimer--;
    if (!FireHeld() || fireTimer > 0)
        return;

    int mine = 0;
    for (int i = 0; i < MAX_SHOTS; i++)
        if (shots[i].used && !shots[i].enemy)
            mine++;
    if (mine >= firePower)
        return;

    for (int i = 0; i < MAX_SHOTS; i++)
        if (!shots[i].used)
        {
            Shot *s = &shots[i];
            s->used  = TRUE;
            s->enemy = FALSE;
            s->w = 14; s->h = 6;
            s->x  = (heroDir > 0) ? heroX + HERO_W : heroX - s->w;
            s->y  = heroY + 24;
            s->vx = heroDir * SHOT_SPEED;
            s->vy = 0;
            fireTimer = FIRE_DELAY;
            Play(SND_SHOOT);
            return;
        }
}

static void EnemyShot(float x, float y, float vx, float vy)
{
    for (int i = 0; i < MAX_SHOTS; i++)
        if (!shots[i].used)
        {
            Shot *s = &shots[i];
            s->used = TRUE; s->enemy = TRUE;
            s->w = 10; s->h = 10;
            s->x = x - 5; s->y = y - 5;
            s->vx = vx; s->vy = vy;
            return;
        }
}

// ============================================================================
//  Robots, crates and things to pick up
// ============================================================================

static BOOL IsRobot(int kind) { return kind >= A_WALKER && kind <= A_BOSS; }

static int RobotPoints(int kind)
{
    switch (kind)
    {
    case A_WALKER: return 100;
    case A_HOPPER: return 200;
    case A_FLYER:  return 150;
    case A_TURRET: return 300;
    case A_BOSS:   return 5000;
    }
    return 0;
}

static COLORREF RobotColour(int kind)
{
    switch (kind)
    {
    case A_HOPPER: return palette[11];
    case A_FLYER:  return palette[2];
    case A_BOSS:   return palette[10];
    }
    return palette[6];
}

static void Destroy(Actor *a)
{
    float cx = a->x + a->w / 2, cy = a->y + a->h / 2;
    if (a->kind == A_BOSS)
    {
        Spark(cx, cy, palette[9], 60, 6);
        Spark(cx, cy, palette[10], 40, 5);
        Spark(cx, cy, palette[7], 30, 4);
        Play(SND_BIGBOOM);
        AddPoints(cx, a->y, RobotPoints(a->kind));
        // The boss drops the yellow key
        a->kind = A_KEYY;
        a->w = a->h = 24;
        a->x = cx - 12;
        a->y = a->y + 60 - 24;
        return;
    }
    Spark(cx, cy, RobotColour(a->kind), 16, 3);
    Spark(cx, cy, palette[9], 10, 2.5f);
    Play(SND_BOOM);
    AddPoints(cx, a->y, RobotPoints(a->kind));
    a->kind = A_NONE;
}

void OpenCrate(Actor *a)
{
    crateAt[(int)(a->y / TILE)][(int)(a->x / TILE)] = 0;    // no longer in the way
    float cx = a->x + a->w / 2, bottom = a->y + a->h;
    Spark(cx, a->y + 16, palette[4], 14, 2.5f);
    Spark(cx, a->y + 16, palette[9], 6, 2);
    Play(SND_CRATE);
    if (a->inside == A_NONE)
    {
        AddPoints(cx, a->y, 100);
        a->kind = A_NONE;
        return;
    }
    a->kind = a->inside;
    a->w = a->h = 24;
    a->x = cx - 12;
    a->y = bottom - 24;
}

static BOOL HeroNear(const Actor *a, float dx, float dy)
{
    float hx = heroX + HERO_W / 2, hy = heroY + HERO_H / 2;
    return fabsf(a->x + a->w / 2 - hx) < dx && fabsf(a->y + a->h / 2 - hy) < dy;
}

// Can a robot see along its row to Jack, with no wall between?
static BOOL ClearLine(const Actor *a)
{
    int ty = (int)((a->y + a->h / 2) / TILE);
    int t0 = (int)((a->x + a->w / 2) / TILE), t1 = (int)((heroX + HERO_W / 2) / TILE);
    if (t0 > t1) { int t = t0; t0 = t1; t1 = t; }
    for (int tx = t0; tx <= t1; tx++)
        if (Solid(tx, ty))
            return FALSE;
    return TRUE;
}

static void UpdateRobot(Actor *a)
{
    float hx = heroX + HERO_W / 2;
    float cx = a->x + a->w / 2;
    Bumps b;

    switch (a->kind)
    {
    case A_WALKER:
    {
        // Walk along, turning round at walls and at the edge of a drop
        b = MoveBox(&a->x, &a->y, a->w, a->h, a->dir * 1.0f, 0, TRUE);
        int front = (a->dir > 0) ? (int)((a->x + a->w) / TILE) : (int)((a->x - 1) / TILE);
        int below = (int)((a->y + a->h + 1) / TILE);
        char under = TileAt(front, below);
        if (b.hitX || !(Solid(front, below) || under == '-'))
            a->dir = -a->dir;
        a->vy += GRAVITY;
        if (a->vy > MAX_FALL) a->vy = MAX_FALL;
        b = MoveBox(&a->x, &a->y, a->w, a->h, 0, a->vy, TRUE);
        if (b.landed) a->vy = 0;
        break;
    }

    case A_HOPPER:
        a->vy += GRAVITY;
        if (a->vy > MAX_FALL) a->vy = MAX_FALL;
        b = MoveBox(&a->x, &a->y, a->w, a->h, a->vx, 0, TRUE);
        if (b.hitX) a->vx = -a->vx;
        b = MoveBox(&a->x, &a->y, a->w, a->h, 0, a->vy, TRUE);
        a->onGround = b.landed;
        if (b.landed || b.bonked) a->vy = 0;
        if (a->onGround)
        {
            a->vx = 0;
            if (--a->timer <= 0)
            {
                a->dir = (hx > cx) ? 1 : -1;
                a->vx = a->dir * 2.2f;
                a->vy = -8.0f;
                a->timer = 45 + rand() % 40;
            }
        }
        break;

    case A_FLYER:
    {
        // Drift towards Jack, slowly, and bob up and down
        float tx = hx - cx, ty = (heroY + 10) - (a->y + a->h / 2);
        float d = sqrtf(tx * tx + ty * ty);
        float bob = sinf((frameCount + a->timer * 7) * 0.08f) * 0.6f;
        if (d > 1)
        {
            MoveBox(&a->x, &a->y, a->w, a->h, tx / d * 1.1f, 0, FALSE);
            MoveBox(&a->x, &a->y, a->w, a->h, 0, ty / d * 0.8f + bob, FALSE);
        }
        a->dir = (tx > 0) ? 1 : -1;
        break;
    }

    case A_TURRET:
        a->dir = (hx > cx) ? 1 : -1;
        if (--a->timer <= 0)
        {
            a->timer = 20;
            if (HeroNear(a, 330, 40) && ClearLine(a))
            {
                EnemyShot(a->dir > 0 ? a->x + a->w + 4 : a->x - 4, a->y + 8, a->dir * 4.0f, 0);
                Play(SND_ZAP);
                a->timer = 100;
            }
        }
        break;

    case A_BOSS:
        a->dir = (hx > cx) ? 1 : -1;
        a->vy += GRAVITY;
        if (a->vy > MAX_FALL) a->vy = MAX_FALL;
        // Keep some distance from Jack: close in when far, back off when near
        a->vx = (fabsf(hx - cx) > 200) ? a->dir * 1.3f : (fabsf(hx - cx) < 120 ? -a->dir * 1.3f : 0);
        b = MoveBox(&a->x, &a->y, a->w, a->h, a->vx, 0, TRUE);
        b = MoveBox(&a->x, &a->y, a->w, a->h, 0, a->vy, TRUE);
        a->onGround = b.landed;
        if (b.landed || b.bonked) a->vy = 0;
        if (--a->timer <= 0)
        {
            int step = rand() % 3;
            if (step == 0 && a->onGround)
                a->vy = -9.0f;           // a jump that shakes the floor
            else
            {
                float gx = a->dir > 0 ? a->x + a->w + 6 : a->x - 6;
                EnemyShot(gx, a->y + 34, a->dir * 4.5f, -0.9f);
                EnemyShot(gx, a->y + 34, a->dir * 4.5f, 0);
                EnemyShot(gx, a->y + 34, a->dir * 4.5f, 0.9f);
                Play(SND_ZAP);
            }
            a->timer = (a->hp > 15) ? 90 : 55;  // angrier when hurt
        }
        break;
    }
}

static void UpdateActors(void)
{
    for (int i = 0; i < numActors; i++)
    {
        Actor *a = &actors[i];
        if (a->kind == A_NONE)
            continue;
        if (a->flash > 0)
            a->flash--;

        // Robots only wake up when they're near the screen
        if (IsRobot(a->kind) && HeroNear(a, GAME_W * 0.8f, VIEW_H * 0.9f))
            UpdateRobot(a);

        if (!Overlap(heroX, heroY, HERO_W, HERO_H, a->x, a->y, a->w, a->h))
            continue;

        float cx = a->x + a->w / 2;
        switch (a->kind)
        {
        case A_WALKER: case A_HOPPER: case A_FLYER: case A_TURRET: case A_BOSS:
            Hurt();
            break;
        case A_GEM:     AddPoints(cx, a->y, 200);  Play(SND_PICKUP); a->kind = A_NONE; break;
        case A_COG:     AddPoints(cx, a->y, 1000); Play(SND_PICKUP); a->kind = A_NONE; break;
        case A_BATTERY:
            if (health < MAX_HEALTH) health++; else AddPoints(cx, a->y, 100);
            Play(SND_POWER); a->kind = A_NONE;
            break;
        case A_MEDKIT:
            health = (health + 4 > MAX_HEALTH) ? MAX_HEALTH : health + 4;
            Play(SND_POWER); a->kind = A_NONE;
            break;
        case A_POWER:
            if (firePower < MAX_FIRE) firePower++; else AddPoints(cx, a->y, 500);
            Play(SND_POWER); a->kind = A_NONE;
            break;
        case A_BOOTS:
            hasBoots = TRUE;
            Play(SND_POWER); a->kind = A_NONE;
            break;
        case A_KEYR: case A_KEYG: case A_KEYB: case A_KEYY:
            keysHeld[a->kind - A_KEYR] = TRUE;
            AddPoints(cx, a->y, 100);
            Play(SND_KEY); a->kind = A_NONE;
            break;
        case A_EXIT:
            // Press Up in front of the exit to leave
            if (KeyDown(VK_UP) && !upWasHeld)
            {
                int bonus = health * 100;
                score += bonus;
                state = ST_DONE;
                stateTimer = 0;
                Play(SND_EXIT);
            }
            break;
        }
    }
    upWasHeld = KeyDown(VK_UP);
}

static void UpdateShots(void)
{
    for (int i = 0; i < MAX_SHOTS; i++)
    {
        Shot *s = &shots[i];
        if (!s->used)
            continue;
        s->x += s->vx;
        s->y += s->vy;

        // Into a wall? Cracked blocks (%) break.
        int tx = (int)floorf((s->x + s->w / 2) / TILE), ty = (int)floorf((s->y + s->h / 2) / TILE);
        if (Solid(tx, ty))
        {
            if (CrateAt(tx, ty))                 // a crate: Jack's shots open it
            {
                if (!s->enemy)
                    OpenCrate(&actors[CrateAt(tx, ty) - 1]);
                else
                    Spark(s->x + s->w / 2, s->y + s->h / 2, palette[8], 4, 1.5f);
            }
            else if (!s->enemy && TileAt(tx, ty) == '%')
            {
                // The whole stack of cracked blocks falls, so a wall two
                // blocks high can be shot through
                int top = ty, bottom = ty;
                while (top > ty - 3 && TileAt(tx, top - 1) == '%') top--;
                while (bottom < ty + 3 && TileAt(tx, bottom + 1) == '%') bottom++;
                for (int y = top; y <= bottom; y++)
                {
                    map[y][tx] = '.';
                    Spark(tx * TILE + 16.0f, y * TILE + 16.0f, palette[lv->tileset == 1 ? 9 : lv->tileset == 2 ? 4 : 14], 12, 2.5f);
                    score += 10;
                }
                Play(SND_BRICK);
            }
            else
                Spark(s->x + s->w / 2, s->y + s->h / 2, s->enemy ? palette[8] : palette[10], 4, 1.5f);
            s->used = FALSE;
            continue;
        }

        // Far off the screen
        if (s->x < camX - 200 || s->x > camX + GAME_W + 200 || s->y < camY - 200 || s->y > camY + VIEW_H + 200)
        {
            s->used = FALSE;
            continue;
        }

        if (s->enemy)
        {
            if (Overlap(s->x, s->y, s->w, s->h, heroX, heroY, HERO_W, HERO_H))
            {
                Hurt();
                s->used = FALSE;
            }
            continue;
        }

        // Jack's shots: robots and crates
        for (int k = 0; k < numActors; k++)
        {
            Actor *a = &actors[k];
            if (!(IsRobot(a->kind) || a->kind == A_CRATE))
                continue;
            if (!Overlap(s->x, s->y, s->w, s->h, a->x, a->y, a->w, a->h))
                continue;
            s->used = FALSE;
            if (a->kind == A_CRATE)
                OpenCrate(a);
            else
            {
                a->flash = 6;
                if (--a->hp <= 0)
                    Destroy(a);
                else
                    Play(SND_HIT);
            }
            break;
        }
    }
}

// ============================================================================
//  The camera: keep Jack near the middle, without showing outside the map
// ============================================================================

static void UpdateCamera(void)
{
    int hx = (int)heroX + HERO_W / 2, hy = (int)heroY + HERO_H / 2;
    if (hx - camX > GAME_W * 55 / 100) camX = hx - GAME_W * 55 / 100;
    if (hx - camX < GAME_W * 45 / 100) camX = hx - GAME_W * 45 / 100;
    if (hy - camY > VIEW_H * 60 / 100) camY = hy - VIEW_H * 60 / 100;
    if (hy - camY < VIEW_H * 35 / 100) camY = hy - VIEW_H * 35 / 100;

    int maxX = lv->w * TILE - GAME_W, maxY = lv->h * TILE - VIEW_H;
    if (camX > maxX) camX = maxX;
    if (camY > maxY) camY = maxY;
    if (camX < 0) camX = 0;
    if (camY < 0) camY = 0;
}

// ============================================================================
//  Each frame
// ============================================================================

void Update(void)
{
    frameCount++;
    stateTimer++;

    switch (state)
    {
    case ST_TITLE:
    case ST_SCORES:
        if (stateTimer > 60 * 10)        // swap between the title and the scores
        {
            state = (state == ST_TITLE) ? ST_SCORES : ST_TITLE;
            stateTimer = 0;
        }
        break;

    case ST_PLAYING:
        if (paused)
            break;
        levelTimer++;
        MoveHero();
        if (HeroOnSpikes())
            Hurt();
        Fire();
        UpdateActors();
        UpdateShots();
        UpdateBits();
        if (hurtTimer > 0)
            hurtTimer--;
        UpdateCamera();
        break;

    case ST_DYING:
        UpdateBits();
        if (stateTimer > 120)
        {
            score     = levelStartScore;  // try the level again as it was
            firePower = startFire;
            hasBoots  = startBoots;
            StartLevel(levelNum);
        }
        break;

    case ST_DONE:
        UpdateBits();
        if (stateTimer > 180)
        {
            if (testMode)
                EnterEditor(levelNum);   // a test from the editor: go back to it
            else if (levelNum + 1 < numLevels)
                StartLevel(levelNum + 1);
            else
            {
                state = ST_VICTORY;
                stateTimer = 0;
            }
        }
        break;

    case ST_VICTORY:
        // Fireworks
        if (stateTimer % 25 == 0)
        {
            static const int cols[] = { 8, 10, 11, 12, 14, 9 };
            Spark((float)(camX + 80 + rand() % (GAME_W - 160)), (float)(camY + 40 + rand() % 140),
                  palette[cols[rand() % 6]], 30, 4);
            Play(SND_BOOM);
        }
        UpdateBits();
        break;
    }
}

int LevelTime(void) { return levelTimer; }

// For the level checker: put Jack somewhere, ready to jump
void HeroSetState(float x, float y, float vy, BOOL ground)
{
    heroX = x; heroY = y; heroVY = vy; onGround = ground;
    jumpWasHeld = FALSE;
}
