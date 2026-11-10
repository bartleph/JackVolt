// ============================================================================
//  draw.cpp - drawing the game
//
//  The picture is a block of memory, GAME_W x GAME_H pixels, that we write
//  colours into directly (a "DIB section"): quick, and simple to clip. Text
//  goes on top with ordinary GDI calls. main.cpp then copies the picture to
//  the window, scaled to fit.
// ============================================================================

#include "volt.h"
#include <stdio.h>

HDC            backDC;
static HBITMAP backBmp, oldBmp;
static DWORD  *pix;                      // the pixels: 0x00RRGGBB, top row first
DWORD          pal32[16];

HFONT          fontTiny, fontSmall, fontHud, fontBig, fontTitle;


// Backdrops: one picture per theme, which repeats as the camera moves
#define BACK_W 640
#define BACK_H 448
static DWORD backdrop[3][BACK_W * BACK_H];

int LevelTime(void);

// ============================================================================
//  Setting up
// ============================================================================

static DWORD Rgb32(COLORREF c)
{
    return (GetRValue(c) << 16) | (GetGValue(c) << 8) | GetBValue(c);
}

static DWORD Mix32(DWORD a, DWORD b, int t)     // t from 0 (all a) to 256 (all b)
{
    int r = (((a >> 16) & 255) * (256 - t) + ((b >> 16) & 255) * t) >> 8;
    int g = (((a >> 8) & 255) * (256 - t) + ((b >> 8) & 255) * t) >> 8;
    int bl = ((a & 255) * (256 - t) + (b & 255) * t) >> 8;
    return (r << 16) | (g << 8) | bl;
}

static unsigned Hash(unsigned x)
{
    x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16;
    return x;
}

static void MakeBackdrops(void)
{
    for (int t = 0; t < 3; t++)
    {
        DWORD *b = backdrop[t];
        for (int y = 0; y < BACK_H; y++)
            for (int x = 0; x < BACK_W; x++)
            {
                DWORD c;
                if (t == 0)
                {
                    // Night over Steel City: stars, and tower blocks with lit windows
                    c = Mix32(0x0A0C24, 0x2A1C48, y * 256 / BACK_H);
                    if (Hash(x * 7919 + y * 104729) % 700 == 0)
                        c = 0xC8C8E0;
                    int col = x / 40;
                    int top = 140 + (int)(Hash(col * 31) % 170);
                    int inset = (int)(Hash(col * 17) % 8);
                    if (y > top && x % 40 >= inset && x % 40 < 36)
                    {
                        c = 0x14142C;
                        if ((x % 40) % 8 >= 3 && (x % 40) % 8 < 6 && y % 12 >= 4 && y % 12 < 8 &&
                            Hash(x / 8 * 13 + y / 12 * 7 + col) % 4 == 0)
                            c = 0x8C7A30;
                    }
                }
                else if (t == 1)
                {
                    // The Gearworks: dark brickwork, big pipes and girders
                    c = Mix32(0x1E1612, 0x2E2018, y * 256 / BACK_H);
                    if (y % 24 == 0 || (x + (y / 24 % 2) * 24) % 48 == 0)
                        c = 0x161010;
                    int px = x % 160;
                    if (px >= 60 && px < 84)
                        c = Mix32(0x3A3A44, 0x5A5A66, (px - 60) < 12 ? (px - 60) * 20 : (84 - px) * 20);
                    int gy = y % 224;
                    if (gy >= 100 && gy < 112)
                        c = ((x + gy) % 16 < 8) ? 0x4A2A1A : 0x3A2014;
                }
                else
                {
                    // Gearhart's fortress: panels with glowing lines
                    c = Mix32(0x120A1E, 0x1E1030, y * 256 / BACK_H);
                    if (x % 64 == 0 || y % 64 == 0)
                        c = 0x2A1A44;
                    if ((x % 64 == 32 && (y / 64) % 2 == 0) || (y % 64 == 32 && (x / 64) % 3 == 1))
                        c = 0x1C3A50;
                    if (x % 64 == 32 && y % 64 == 32)
                        c = 0x40C8F0;
                }
                b[y * BACK_W + x] = c;
            }
    }
}

void SetupBackBuffer(HDC screenDC)
{
    BITMAPINFO bi;
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize        = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth       = GAME_W;
    bi.bmiHeader.biHeight      = -GAME_H;            // negative: top row first
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    backDC  = CreateCompatibleDC(screenDC);
    backBmp = CreateDIBSection(screenDC, &bi, DIB_RGB_COLORS, (void **)&pix, NULL, 0);
    oldBmp  = (HBITMAP)SelectObject(backDC, backBmp);
    SetBkMode(backDC, TRANSPARENT);

    for (int i = 0; i < 16; i++)
        pal32[i] = Rgb32(palette[i]);

    MakeBackdrops();
}

void FreeBackBuffer(void)
{
    SelectObject(backDC, oldBmp);
    DeleteObject(backBmp);
    DeleteDC(backDC);
}

static HFONT MakeFont(int height)
{
    return CreateFont(height, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
                      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
                      FIXED_PITCH | FF_MODERN, "Consolas");
}

void MakeFonts(void)
{
    fontTiny  = MakeFont(12);
    fontSmall = MakeFont(14);
    fontHud   = MakeFont(18);
    fontBig   = MakeFont(40);
    fontTitle = MakeFont(76);
}

void FreeFonts(void)
{
    DeleteObject(fontTiny);
    DeleteObject(fontSmall);
    DeleteObject(fontHud);
    DeleteObject(fontBig);
    DeleteObject(fontTitle);
}

// ============================================================================
//  Pixels
// ============================================================================

// Drawing is clipped to this rectangle (the play area, or the whole picture)
static int clipX0, clipY0, clipX1 = GAME_W, clipY1 = GAME_H;

void GfxClip(int x0, int y0, int x1, int y1)
{
    clipX0 = x0; clipY0 = y0; clipX1 = x1; clipY1 = y1;
}

void GfxRect(int x, int y, int w, int h, DWORD c)
{
    int x0 = x < clipX0 ? clipX0 : x, x1 = x + w > clipX1 ? clipX1 : x + w;
    int y0 = y < clipY0 ? clipY0 : y, y1 = y + h > clipY1 ? clipY1 : y + h;
    for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++)
            pix[yy * GAME_W + xx] = c;
}

// Draw a sprite with its top left at (x, y), each art pixel 'scale' pixels
// across. flip mirrors it; colour 'from' is drawn as 'to' (for keys and
// doors); white draws it all in white (a robot that's just been hit).
void GfxSprite(int s, int x, int y, int scale, BOOL flip, int from, int to, BOOL white)
{
    const SpriteArt *a = &spriteArt[s];
    if (x >= clipX1 || y >= clipY1 || x + a->w * scale <= clipX0 || y + a->h * scale <= clipY0)
        return;
    for (int row = 0; row < a->h; row++)
    {
        int sy = y + row * scale;
        if (sy + scale <= clipY0 || sy >= clipY1)
            continue;
        for (int col = 0; col < a->w; col++)
        {
            int c = sprPix[s][row * a->w + (flip ? a->w - 1 - col : col)];
            if (c < 0)
                continue;
            if (c == from) c = to;
            DWORD v = white ? 0xFFFFFF : pal32[c];
            int sx = x + col * scale;
            for (int dy = 0; dy < scale; dy++)
            {
                int yy = sy + dy;
                if (yy < clipY0 || yy >= clipY1) continue;
                for (int dx = 0; dx < scale; dx++)
                {
                    int xx = sx + dx;
                    if (xx >= clipX0 && xx < clipX1)
                        pix[yy * GAME_W + xx] = v;
                }
            }
        }
    }
}

// The backdrop, moving at a third of the camera's speed so it looks far away
void GfxBackdrop(int theme, int ox, int oy)
{
    const DWORD *b = backdrop[(theme - 1) % 3];
    ox = ((ox % BACK_W) + BACK_W) % BACK_W;
    oy = ((oy % (BACK_H - VIEW_H)) + (BACK_H - VIEW_H)) % (BACK_H - VIEW_H);
    for (int y = clipY0; y < clipY1; y++)
    {
        const DWORD *src = b + (y + oy) * BACK_W;
        DWORD *dst = pix + y * GAME_W;
        for (int x = clipX0; x < clipX1; x++)
            dst[x] = src[(x + ox) % BACK_W];
    }
}

// ============================================================================
//  Text (drawn with GDI, on top of the pixels)
// ============================================================================

void GfxText(HFONT f, int x, int y, const char *s, COLORREF c)
{
    HFONT old = (HFONT)SelectObject(backDC, f);
    SetTextColor(backDC, RGB(0, 0, 0));
    TextOut(backDC, x + 2, y + 2, s, lstrlen(s));
    SetTextColor(backDC, c);
    TextOut(backDC, x, y, s, lstrlen(s));
    SelectObject(backDC, old);
}

int GfxTextWidth(HFONT f, const char *s)
{
    HFONT old = (HFONT)SelectObject(backDC, f);
    SIZE sz;
    GetTextExtentPoint32(backDC, s, lstrlen(s), &sz);
    SelectObject(backDC, old);
    return sz.cx;
}

void GfxCentre(HFONT f, int y, const char *s, COLORREF c)
{
    GfxText(f, (GAME_W - GfxTextWidth(f, s)) / 2, y, s, c);
}

// ============================================================================
//  The level
// ============================================================================

int KeyColour(int k)                     // red, green, blue, yellow
{
    static const int c[4] = { 8, 11, 13, 10 };
    return c[k];
}

static void DrawTiles(void)
{
    int ts = (lv->tileset < 1 || lv->tileset > 3) ? 0 : lv->tileset - 1;
    int base = SPR_WALL1 + ts * 4;
    for (int ty = camY / TILE; ty <= (camY + VIEW_H) / TILE; ty++)
        for (int tx = camX / TILE; tx <= (camX + GAME_W) / TILE; tx++)
        {
            char c = TileAt(tx, ty);
            int  x = tx * TILE - camX, y = ty * TILE - camY;
            int  s = -1, from = -1, to = -1;
            switch (c)
            {
            case '#': s = base;     break;
            case '=': s = base + 1; break;
            case '-': s = base + 2; break;
            case '%': s = base + 3; break;
            case '^': s = SPR_SPIKES; break;
            case 'p': s = SPR_PIPE;   break;
            case 'q': s = SPR_PIPEH;  break;
            case 'l': s = SPR_LAMP;   break;
            case 'R': s = SPR_DOOR; from = 10; to = KeyColour(0); break;
            case 'G': s = SPR_DOOR; from = 10; to = KeyColour(1); break;
            case 'B': s = SPR_DOOR; from = 10; to = KeyColour(2); break;
            case 'Y': s = SPR_DOOR; from = 10; to = KeyColour(3); break;
            }
            if (s >= 0)
                GfxSprite(s, x, y, 2, FALSE, from, to, FALSE);
        }
}

static void DrawActors(void)
{
    int anim = (frameCount / 12) % 2;
    for (int i = 0; i < numActors; i++)
    {
        const Actor *a = &actors[i];
        if (a->kind == A_NONE)
            continue;
        int s = -1, from = -1, to = -1, scale = 2;
        BOOL flip = FALSE;
        float bob = 0;
        switch (a->kind)
        {
        case A_WALKER: s = anim ? SPR_WALKER2 : SPR_WALKER1; flip = a->dir < 0; break;
        case A_HOPPER: s = a->onGround ? SPR_HOPPER1 : SPR_HOPPER2; flip = a->dir < 0; break;
        case A_FLYER:  s = anim ? SPR_FLYER2 : SPR_FLYER1;   flip = a->dir < 0; break;
        case A_TURRET: s = SPR_TURRET;  flip = a->dir < 0; break;
        case A_BOSS:   s = anim ? SPR_BOSS2 : SPR_BOSS1;     flip = a->dir < 0; break;
        case A_GEM:     s = SPR_GEM;     break;
        case A_COG:     s = SPR_COG;     break;
        case A_BATTERY: s = SPR_BATTERY; break;
        case A_MEDKIT:  s = SPR_MEDKIT;  break;
        case A_POWER:   s = SPR_POWER;   break;
        case A_BOOTS:   s = SPR_BOOTS;   break;
        case A_KEYR: case A_KEYG: case A_KEYB: case A_KEYY:
            s = SPR_KEY; from = 10; to = KeyColour(a->kind - A_KEYR); break;
        case A_CRATE:   s = SPR_CRATE;   break;
        case A_EXIT:    s = SPR_EXIT;    break;
        }
        if (s < 0)
            continue;
        if (a->kind >= A_GEM && a->kind <= A_KEYY)
            bob = sinf((frameCount + i * 11) * 0.1f) * 3;       // things to pick up bob about

        // Centred on the actor, standing on its bottom edge
        int w = spriteArt[s].w * scale, h = spriteArt[s].h * scale;
        int x = (int)(a->x + a->w / 2) - w / 2 - camX;
        int y = (int)(a->y + a->h) - h - camY + (int)bob;
        if (a->kind >= A_GEM && a->kind <= A_KEYY)
            y += 4;
        GfxSprite(s, x, y, scale, flip, from, to, a->flash > 0 && (a->flash & 2));
    }
}

static void DrawHero(void)
{
    if (state == ST_DYING)
        return;
    if (hurtTimer > 0 && (frameCount / 3) % 2)
        return;                                  // flashing after a hit

    // The walk has six frames. Which one shows depends on how far he's
    // walked, so his feet keep pace with the floor whichever way he goes.
    static float lastX, walked;
    float moved = fabsf(heroX - lastX);
    lastX = heroX;
    if (moved > 0.5f && moved < 8)
        walked += moved;

    static const int bob[6] = { 0, 2, 2, 0, 2, 2 };     // up a little as the legs pass
    int s = SPR_STAND, lift = 0;
    if (!onGround)
        s = SPR_JUMP;
    else if (moved > 0.5f)
    {
        int f = (int)(walked / 8) % 6;
        s = SPR_WALK1 + f;
        lift = bob[f];
    }

    int x = (int)heroX + HERO_W / 2 - 16 - camX;
    int y = (int)heroY + HERO_H - 64 - camY - lift;
    if (heroDir < 0)
        x -= 2;                                  // the gun side swaps over
    GfxSprite(s, x, y, 2, heroDir < 0, -1, -1, FALSE);
}

static void DrawShots(void)
{
    for (int i = 0; i < MAX_SHOTS; i++)
    {
        const Shot *s = &shots[i];
        if (!s->used)
            continue;
        int x = (int)s->x - camX, y = (int)s->y - camY;
        if (s->enemy)
        {
            GfxRect(x + 2, y, 6, 10, pal32[8]);
            GfxRect(x, y + 2, 10, 6, pal32[8]);
            GfxRect(x + 3, y + 3, 4, 4, pal32[10]);
        }
        else
        {
            GfxRect(x, y, s->w, s->h, pal32[10]);
            GfxRect(x + 2, y + 2, s->w - 4, s->h - 4, 0xFFFFFF);
            int tail = s->vx > 0 ? x - 8 : x + s->w;
            GfxRect(tail, y + 2, 8, 2, pal32[9]);
        }
    }
}

static void DrawSparks(void)
{
    for (int i = 0; i < MAX_BITS; i++)
        if (bits[i].used && !bits[i].text[0])
        {
            int sz = bits[i].life > 20 ? 4 : 2;
            GfxRect((int)bits[i].x - camX, (int)bits[i].y - camY, sz, sz, Rgb32(bits[i].col));
        }
}

static void DrawPopups(void)
{
    for (int i = 0; i < MAX_BITS; i++)
        if (bits[i].used && bits[i].text[0])
        {
            int w = GfxTextWidth(fontSmall, bits[i].text);
            GfxText(fontSmall, (int)bits[i].x - camX - w / 2, (int)bits[i].y - camY, bits[i].text,
                 (bits[i].life / 4) % 2 ? RGB(255, 255, 255) : RGB(255, 220, 80));
        }
}

// ---- The status bar along the bottom
static void DrawHudPixels(void)
{
    GfxClip(0, 0, GAME_W, GAME_H);
    GfxRect(0, VIEW_H, GAME_W, GAME_H - VIEW_H, 0x101018);
    GfxRect(0, VIEW_H, GAME_W, 2, 0x5A5A70);

    // Health: eight cells
    for (int i = 0; i < MAX_HEALTH; i++)
    {
        DWORD c = (i < health) ? (health <= 2 ? pal32[8] : pal32[11]) : 0x303038;
        GfxRect(150 + i * 14, VIEW_H + 26, 11, 14, c);
    }
    // Fire power: a bolt for each shot
    for (int i = 0; i < MAX_FIRE; i++)
    {
        DWORD c = (i < firePower) ? pal32[10] : 0x303038;
        GfxRect(290 + i * 14, VIEW_H + 30, 10, 6, c);
        GfxRect(292 + i * 14, VIEW_H + 32, 6, 2, i < firePower ? 0xFFFFFF : 0x404048);
    }
    // Keys, and the boots
    for (int k = 0; k < 4; k++)
        if (keysHeld[k])
            GfxSprite(SPR_KEY, 368 + k * 22, VIEW_H + 22, 1, FALSE, 10, KeyColour(k), FALSE);
        else
            GfxRect(372 + k * 22, VIEW_H + 29, 10, 3, 0x303038);
    if (hasBoots)
        GfxSprite(SPR_BOOTS, 458, VIEW_H + 24, 1, FALSE, -1, -1, FALSE);
}

static void DrawHudText(void)
{
    char s[64];
    wsprintf(s, "%07d", score);
    GfxText(fontHud, 12, VIEW_H + 22, s, RGB(255, 255, 255));
    GfxText(fontSmall, 12, VIEW_H + 5, "SCORE", RGB(150, 150, 180));
    GfxText(fontSmall, 150, VIEW_H + 5, "HEALTH", RGB(150, 150, 180));
    GfxText(fontSmall, 290, VIEW_H + 5, "FIRE", RGB(150, 150, 180));
    GfxText(fontSmall, 368, VIEW_H + 5, "KEYS", RGB(150, 150, 180));
    wsprintf(s, "%d. %s", levelNum + 1, lv->name);
    GfxText(fontSmall, GAME_W - 12 - GfxTextWidth(fontSmall, s), VIEW_H + 5, s, RGB(150, 150, 180));
    wsprintf(s, "HI %07d", HiScore() > score ? HiScore() : score);
    GfxText(fontHud, GAME_W - 12 - GfxTextWidth(fontHud, s), VIEW_H + 22, s, RGB(255, 220, 80));
}

static void Panel(int y, int h)
{
    GfxClip(0, 0, GAME_W, GAME_H);
    GfxRect(110, y, GAME_W - 220, h, 0x7878A0);
    GfxRect(114, y + 4, GAME_W - 228, h - 8, 0x080818);
}

static void DrawWorld(void)
{
    GfxClip(0, 0, GAME_W, VIEW_H);
    GfxBackdrop(lv->backdrop, camX / 3, camY / 4);
    DrawTiles();
    DrawActors();
    DrawHero();
    DrawShots();
    DrawSparks();
    DrawHudPixels();

    if (state == ST_DONE)       Panel(110, 120);
    if (state == ST_VICTORY)    Panel(80, 180);
    if (paused)                 Panel(140, 64);

    GdiFlush();                                 // pixels done: now the text
    DrawPopups();
    DrawHudText();

    char s[80];
    if (state == ST_PLAYING && LevelTime() < 150 && !paused)
    {
        wsprintf(s, "LEVEL %d", levelNum + 1);
        GfxCentre(fontBig, 110, s, RGB(255, 220, 80));
        GfxCentre(fontHud, 160, lv->name, RGB(255, 255, 255));
    }
    if (paused)
        GfxCentre(fontBig, 150, "PAUSED", RGB(255, 255, 255));
    if (state == ST_DONE)
    {
        GfxCentre(fontBig, 124, "LEVEL COMPLETE", RGB(120, 255, 120));
        wsprintf(s, "Health bonus  %d", health * 100);
        GfxCentre(fontHud, 180, s, RGB(255, 255, 255));
    }
    if (state == ST_VICTORY)
    {
        GfxCentre(fontBig, 96, "YOU WIN!", RGB(255, 220, 80));
        GfxCentre(fontHud, 150, "Professor Gearhart's robots are scrap,", RGB(255, 255, 255));
        GfxCentre(fontHud, 174, "and Steel City is safe again.", RGB(255, 255, 255));
        wsprintf(s, "Final score  %d", score);
        GfxCentre(fontHud, 206, s, RGB(120, 220, 255));
        GfxCentre(fontSmall, 236, "Press Enter", RGB(160, 160, 190));
    }
}

// ============================================================================
//  The title and the high scores
// ============================================================================

static void DrawTitle(void)
{
    GfxClip(0, 0, GAME_W, GAME_H);
    GfxBackdrop(1, frameCount / 2, 0);
    GfxRect(0, 330, GAME_W, 70, 0x0A0A16);

    // Jack, big, and some of the robots he's up against
    GfxSprite(SPR_STAND, 60, 172, 5, FALSE, -1, -1, FALSE);
    int anim = (frameCount / 12) % 2;
    GfxSprite(anim ? SPR_WALKER2 : SPR_WALKER1, 480, 270, 4, TRUE, -1, -1, FALSE);
    GfxSprite(anim ? SPR_FLYER2 : SPR_FLYER1, 520, 150, 3, TRUE, -1, -1, FALSE);
    GfxSprite(SPR_HOPPER1, 404, 286, 3, TRUE, -1, -1, FALSE);

    GdiFlush();
    GfxCentre(fontTitle, 26, "JACK VOLT", RGB(255, 220, 60));
    GfxCentre(fontHud, 108, "versus the robots of Professor Gearhart", RGB(120, 220, 255));

    GfxCentre(fontHud, 160, "Press ENTER to play", ((frameCount / 30) % 2) ? RGB(255, 255, 255) : RGB(255, 220, 80));
    GfxCentre(fontSmall, 200, "Arrows: move     Space / Alt / Z: jump", RGB(200, 200, 220));
    GfxCentre(fontSmall, 220, "Ctrl / X: fire     Up: go through the exit", RGB(200, 200, 220));
    GfxCentre(fontSmall, 240, "P: pause   S: sound   M: music   F11: full screen", RGB(200, 200, 220));
    GfxCentre(fontSmall, 260, "H: high scores  E: level editor  C: character designer", RGB(200, 200, 220));
#ifndef __wasm__
    GfxCentre(fontSmall, 280, "Esc: quit", RGB(200, 200, 220));
#endif
    GfxCentre(fontSmall, 345, "Shoot crates open. Find the coloured keys. Reach the exit.", RGB(150, 150, 180));
    GfxCentre(fontSmall, 368, "Gamepad: A jump, X or B fire, Y pause, Start play", RGB(110, 110, 140));
}

static void DrawScores(void)
{
    GfxClip(0, 0, GAME_W, GAME_H);
    GfxBackdrop(3, frameCount / 2, 0);
    GdiFlush();
    GfxCentre(fontBig, 30, "HIGH SCORES", RGB(255, 220, 60));
    char s[64];
    for (int i = 0; i < NUM_SCORES; i++)
    {
        if (scores[i].score > 0)
            wsprintf(s, "%2d.  %07d  %-16s", i + 1, scores[i].score, scores[i].name);
        else
            wsprintf(s, "%2d.  -------  %-16s", i + 1, "");
        GfxCentre(fontHud, 96 + i * 24, s, i == 0 ? RGB(255, 255, 255) : RGB(180, 200, 230));
    }
    GfxCentre(fontSmall, 360, "Press ENTER to play, H for the title", RGB(150, 150, 180));
}

void Render(void)
{
    if (state == ST_EDIT)
        DrawEditor();
    else if (state == ST_DESIGN)
        DrawDesigner();
    else if (state == ST_TITLE)
        DrawTitle();
    else if (state == ST_SCORES)
        DrawScores();
    else
        DrawWorld();
    GdiFlush();
}
