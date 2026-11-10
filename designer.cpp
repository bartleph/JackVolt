// ============================================================================
//  designer.cpp - the character designer: a pixel editor for every picture
//  in the game (press C on the title screen), and sprites.txt
//
//  The pictures the game draws are in sprPix[]. They start as the standard
//  ones from art.cpp; if there's a sprites.txt next to the program, the
//  pictures in it are used instead. The designer changes sprPix[] directly,
//  and writes sprites.txt when you save.
// ============================================================================

#include "volt.h"
#include <stdio.h>

signed char sprPix[NUM_SPRITES][32 * 32];

// ============================================================================
//  sprites.txt
//
//      SPRITE STAND
//      ...one line per row: '.' see-through, 0-9 and A-F a colour...
//      END
// ============================================================================

static const char spriteHeader[] =
    "; Jack Volt artwork, made with the designer (press C on the title screen).\r\n"
    "; Each picture is a grid: one character per pixel.\r\n"
    ";   .  see-through\r\n"
    ";   0 black   1 navy   2 purple   3 dark green   4 brown   5 dark grey   6 light grey   7 white\r\n"
    ";   8 red     9 orange A yellow   B green        C cyan    D blue        E pink         F skin\r\n"
    "; Pictures face right: the game flips them for going left.\r\n"
    "; Keys and doors are drawn in yellow (A); the game swaps it for their colour.\r\n"
    "; Leave out a picture, or delete this file, to get the standard one back.\r\n\r\n";

static int ColourOf(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static void StandardSprite(int s)
{
    const SpriteArt *a = &spriteArt[s];
    for (int i = 0; i < a->w * a->h; i++)
        sprPix[s][i] = (signed char)ColourOf(a->rows[i]);
}

void LoadSprites(void)
{
    for (int s = 0; s < NUM_SPRITES; s++)
        StandardSprite(s);

    char path[MAX_PATH], line[256];
    ExePath(SPRITE_FILE, path);
    char *text = ReadTextFile(path);
    if (text == NULL)
        return;                         // no file: the standard pictures it is

    const char *p = text;
    int cur = -1, row = 0;
    while (*p)
    {
        p = NextLine(p, line, sizeof line);
        if (cur >= 0)
        {
            if (lstrcmpi(line, "END") == 0 || row >= spriteArt[cur].h)
            {
                cur = -1;
                continue;
            }
            int w = spriteArt[cur].w, len = lstrlen(line);
            for (int x = 0; x < w; x++)
                sprPix[cur][row * w + x] = (signed char)(x < len ? ColourOf(line[x]) : -1);
            row++;
            continue;
        }
        if (strncmp(line, "SPRITE ", 7) == 0)
        {
            char *id = line + 7;
            while (*id == ' ') id++;
            for (char *e = id + lstrlen(id); e > id && (e[-1] == ' ' || e[-1] == '\t'); )
                *--e = 0;
            for (int s = 0; s < NUM_SPRITES; s++)
                if (lstrcmpi(id, spriteIds[s]) == 0)
                {
                    cur = s;
                    row = 0;
                }
        }
    }
    free(text);
}

BOOL SaveSprites(void)
{
    char *buf = (char *)malloc(sizeof spriteHeader + NUM_SPRITES * (32 * 34 + 40));
    if (buf == NULL)
        return FALSE;
    int n = wsprintf(buf, "%s", spriteHeader);
    for (int s = 0; s < NUM_SPRITES; s++)
    {
        n += wsprintf(buf + n, "SPRITE %s\r\n", spriteIds[s]);
        int w = spriteArt[s].w, h = spriteArt[s].h;
        for (int y = 0; y < h; y++)
        {
            for (int x = 0; x < w; x++)
                buf[n++] = ".0123456789ABCDEF"[sprPix[s][y * w + x] + 1];
            buf[n++] = '\r';
            buf[n++] = '\n';
        }
        n += wsprintf(buf + n, "END\r\n\r\n");
    }
    buf[n] = 0;

    char path[MAX_PATH];
    ExePath(SPRITE_FILE, path);
    BOOL ok = WriteTextFile(path, buf);
    free(buf);
    if (!ok)
        MessageBox(hwndMain, "Couldn't save " SPRITE_FILE ".", "Jack Volt", MB_ICONERROR);
    return ok;
}

// ============================================================================
//  What can be designed, in four groups
// ============================================================================

typedef struct { int sprite, group; const char *name; } DesignItem;

static const DesignItem items[] =
{
    { SPR_STAND, 0, "Jack standing" },
    { SPR_WALK1, 0, "Jack walking, 1 of 6" }, { SPR_WALK2, 0, "Jack walking, 2 of 6" },
    { SPR_WALK3, 0, "Jack walking, 3 of 6" }, { SPR_WALK4, 0, "Jack walking, 4 of 6" },
    { SPR_WALK5, 0, "Jack walking, 5 of 6" }, { SPR_WALK6, 0, "Jack walking, 6 of 6" },
    { SPR_JUMP,  0, "Jack jumping" },
    { SPR_WALKER1, 1, "Walker, 1 of 2" }, { SPR_WALKER2, 1, "Walker, 2 of 2" },
    { SPR_HOPPER1, 1, "Hopper, on the ground" }, { SPR_HOPPER2, 1, "Hopper, in the air" },
    { SPR_FLYER1, 1, "Flyer, 1 of 2" }, { SPR_FLYER2, 1, "Flyer, 2 of 2" },
    { SPR_TURRET, 1, "Turret" },
    { SPR_BOSS1, 1, "Professor Gearhart, 1 of 2" }, { SPR_BOSS2, 1, "Professor Gearhart, 2 of 2" },
    { SPR_GEM, 2, "Gem" }, { SPR_COG, 2, "Cog" }, { SPR_BATTERY, 2, "Battery" },
    { SPR_MEDKIT, 2, "Medkit" }, { SPR_POWER, 2, "Fire power" }, { SPR_BOOTS, 2, "Spring boots" },
    { SPR_KEY, 2, "Key" }, { SPR_CRATE, 2, "Crate" }, { SPR_EXIT, 2, "Exit" },
    { SPR_WALL1, 3, "City wall" }, { SPR_GIRDER1, 3, "City girder" },
    { SPR_LEDGE1, 3, "City ledge" }, { SPR_BRICK1, 3, "City cracked block" },
    { SPR_WALL2, 3, "Factory wall" }, { SPR_GIRDER2, 3, "Factory girder" },
    { SPR_LEDGE2, 3, "Factory ledge" }, { SPR_BRICK2, 3, "Factory cracked block" },
    { SPR_WALL3, 3, "Fortress wall" }, { SPR_GIRDER3, 3, "Fortress girder" },
    { SPR_LEDGE3, 3, "Fortress ledge" }, { SPR_BRICK3, 3, "Fortress cracked block" },
    { SPR_SPIKES, 3, "Spikes" }, { SPR_DOOR, 3, "Door" },
    { SPR_PIPE, 3, "Pipe" }, { SPR_PIPEH, 3, "Pipe across" }, { SPR_LAMP, 3, "Lamp" },
};
#define NUM_ITEMS  ((int)(sizeof items / sizeof items[0]))
#define NUM_GROUPS 4
static const char *groupNames[NUM_GROUPS] = { "Jack", "Robots", "Things", "Tiles" };
static const char *colourNames[16] =
{
    "black", "navy", "purple", "dark green", "brown", "dark grey", "light grey", "white",
    "red", "orange", "yellow", "green", "cyan", "blue", "pink", "skin"
};

// ---- Where things are on the screen
#define GRID_X      8                   // the big grid, up to 352 x 352
#define GRID_Y      28
#define GRID_PIX    352
#define SIDE_X      372                 // everything else is down the right
#define TAB_Y       26
#define TAB_W       62
#define THUMB_Y     52
#define THUMB_STEP  37
#define SWATCH_Y    176
#define SWATCH      22
#define SWATCH_STEP 26
#define PREVIEW_Y   232
#define PREVIEW_H   100

static int  curItem;                    // the entry in items[] being drawn
static int  curGroup;
static int  groupItem[NUM_GROUPS] = { 0, 8, 17, 26 };   // what each group last showed
static int  colour = 12;                // the paint; -1 = see-through
static int  mouseX = -1, mouseY = -1;
static BOOL painting;
static BOOL designModified;
static int  savedUntil;
static signed char clip[32 * 32];
static int  clipW, clipH;

// Undo: a copy of a picture is kept before every change
#define MAX_UNDO 64
typedef struct { int sprite; signed char before[32 * 32]; } UndoStep;
static UndoStep undoSteps[MAX_UNDO];
static int      undoCount;

static int Spr(void)      { return items[curItem].sprite; }
static int SprW(void)     { return spriteArt[Spr()].w; }
static int SprH(void)     { return spriteArt[Spr()].h; }
static int Cell(void)     { return GRID_PIX / (SprW() > SprH() ? SprW() : SprH()); }

static void Select(int i)
{
    curItem  = i;
    curGroup = items[i].group;
    groupItem[curGroup] = i;
}

static void PushUndo(void)
{
    if (undoCount == MAX_UNDO)          // full: forget the oldest
    {
        memmove(&undoSteps[0], &undoSteps[1], (MAX_UNDO - 1) * sizeof(UndoStep));
        undoCount--;
    }
    undoSteps[undoCount].sprite = Spr();
    memcpy(undoSteps[undoCount].before, sprPix[Spr()], sizeof undoSteps[0].before);
    undoCount++;
}

static void Undo(void)
{
    if (undoCount == 0)
        return;
    undoCount--;
    int s = undoSteps[undoCount].sprite;
    memcpy(sprPix[s], undoSteps[undoCount].before, sizeof sprPix[s]);
    for (int i = 0; i < NUM_ITEMS; i++)         // show the picture that changed
        if (items[i].sprite == s)
            Select(i);
    designModified = TRUE;
}

static signed char *Pixel(int x, int y) { return &sprPix[Spr()][y * SprW() + x]; }

// Which pixel of the grid is at this point on the screen?
static BOOL GridCell(int mx, int my, int *cx, int *cy)
{
    if (mx < GRID_X || my < GRID_Y)
        return FALSE;
    *cx = (mx - GRID_X) / Cell();
    *cy = (my - GRID_Y) / Cell();
    return *cx < SprW() && *cy < SprH();
}

static void SetPixel(int x, int y, int c)
{
    if (*Pixel(x, y) != c)
    {
        *Pixel(x, y) = (signed char)c;
        designModified = TRUE;
    }
}

// Fill the patch of colour 'from' around (x, y) with colour 'to'
static void Fill(int x, int y, int from, int to)
{
    if (x < 0 || x >= SprW() || y < 0 || y >= SprH() || from == to || *Pixel(x, y) != from)
        return;
    *Pixel(x, y) = (signed char)to;
    Fill(x + 1, y, from, to);
    Fill(x - 1, y, from, to);
    Fill(x, y + 1, from, to);
    Fill(x, y - 1, from, to);
}

// Flip with H or V, or slide the picture a pixel with the arrow keys
static void Transform(WPARAM op)
{
    int w = SprW(), h = SprH();
    signed char old[32 * 32];
    memcpy(old, sprPix[Spr()], sizeof old);
    PushUndo();
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            int sx = x, sy = y;         // the pixel that moves into (x, y)
            switch (op)
            {
            case 'H':      sx = w - 1 - x;      break;
            case 'V':      sy = h - 1 - y;      break;
            case VK_LEFT:  sx = (x + 1) % w;     break;
            case VK_RIGHT: sx = (x + w - 1) % w; break;
            case VK_UP:    sy = (y + 1) % h;     break;
            case VK_DOWN:  sy = (y + h - 1) % h; break;
            }
            sprPix[Spr()][y * w + x] = old[sy * w + sx];
        }
    designModified = TRUE;
}

static void ThumbPos(int n, int *x, int *y)
{
    *x = SIDE_X + (n % 7) * THUMB_STEP;
    *y = THUMB_Y + (n / 7) * 38;
}

static void SwatchPos(int i, int *x, int *y)        // swatch 0 is see-through
{
    *x = SIDE_X + (i % 9) * SWATCH_STEP;
    *y = SWATCH_Y + (i / 9) * SWATCH_STEP;
}

// ============================================================================
//  Keys and the mouse
// ============================================================================

void EnterDesigner(void)
{
    undoCount = 0;
    designModified = FALSE;
    Select(curItem);
    StopSounds();
    state = ST_DESIGN;
}

static BOOL AskToSave(void)
{
    if (!designModified)
        return TRUE;
    int r = MessageBox(hwndMain, "Save your artwork to " SPRITE_FILE "?", "Character designer",
                       MB_YESNOCANCEL | MB_ICONQUESTION);
    if (r == IDCANCEL)
        return FALSE;
    if (r == IDYES && !SaveSprites())
        return FALSE;
    if (r == IDNO)
        LoadSprites();                  // throw the changes away
    designModified = FALSE;
    return TRUE;
}

BOOL DesignCanClose(void)
{
    return state != ST_DESIGN || AskToSave();
}

void DesignMouseDown(int mx, int my, BOOL right)
{
    // A group tab
    for (int g = 0; g < NUM_GROUPS; g++)
        if (mx >= SIDE_X + g * (TAB_W + 3) && mx < SIDE_X + g * (TAB_W + 3) + TAB_W &&
            my >= TAB_Y - 2 && my < TAB_Y + 20)
        {
            Select(groupItem[g]);
            return;
        }

    // A thumbnail picks what to draw
    for (int i = 0, n = 0; i < NUM_ITEMS; i++)
    {
        if (items[i].group != curGroup)
            continue;
        int tx, ty;
        ThumbPos(n++, &tx, &ty);
        if (mx >= tx && mx < tx + 34 && my >= ty && my < ty + 36)
        {
            Select(i);
            return;
        }
    }

    // A swatch picks the paint
    for (int i = 0; i <= 16; i++)
    {
        int sx, sy;
        SwatchPos(i, &sx, &sy);
        if (mx >= sx && mx < sx + SWATCH && my >= sy && my < sy + SWATCH)
        {
            colour = i - 1;
            return;
        }
    }

    int cx, cy;
    if (!GridCell(mx, my, &cx, &cy))
        return;
    if (GetKeyState(VK_CONTROL) < 0)            // Ctrl+click: pick up a colour
    {
        colour = *Pixel(cx, cy);
        return;
    }
    int c = right ? -1 : colour;
    PushUndo();
    if (GetKeyState(VK_SHIFT) < 0)              // Shift+click: fill a patch
    {
        Fill(cx, cy, *Pixel(cx, cy), c);
        designModified = TRUE;
        return;
    }
    painting = TRUE;
    SetCapture(hwndMain);                       // keep painting while dragging
    SetPixel(cx, cy, c);
}

void DesignMouseMove(int mx, int my, WPARAM buttons)
{
    mouseX = mx;
    mouseY = my;
    int cx, cy;
    if (!(buttons & (MK_LBUTTON | MK_RBUTTON)))
        painting = FALSE;
    if (!painting || !GridCell(mx, my, &cx, &cy))
        return;
    SetPixel(cx, cy, (buttons & MK_LBUTTON) ? colour : -1);
}

void DesignKey(WPARAM key)
{
    if (GetKeyState(VK_CONTROL) < 0)
    {
        switch (key)
        {
        case 'Z':
            Undo();
            break;
        case 'C':
            memcpy(clip, sprPix[Spr()], sizeof clip);
            clipW = SprW();
            clipH = SprH();
            break;
        case 'V':                       // into a picture of another size: the top left is kept
            if (clipW)
            {
                PushUndo();
                for (int y = 0; y < SprH(); y++)
                    for (int x = 0; x < SprW(); x++)
                        *Pixel(x, y) = (x < clipW && y < clipH) ? clip[y * clipW + x] : -1;
                designModified = TRUE;
            }
            break;
        case 'S':
            if (SaveSprites())
            {
                designModified = FALSE;
                savedUntil = frameCount + 120;
            }
            break;
        }
        return;
    }

    switch (key)
    {
    case VK_ESCAPE:
        if (AskToSave())
            ShowTitle();
        break;
    case VK_TAB:
        Select(groupItem[(curGroup + (GetKeyState(VK_SHIFT) < 0 ? NUM_GROUPS - 1 : 1)) % NUM_GROUPS]);
        break;
    case VK_PRIOR:
        Select((curItem + NUM_ITEMS - 1) % NUM_ITEMS);
        break;
    case VK_NEXT:
        Select((curItem + 1) % NUM_ITEMS);
        break;
    case 'H': case 'V':
    case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN:
        Transform(key);
        break;
    case VK_DELETE:
        PushUndo();
        memset(sprPix[Spr()], -1, sizeof sprPix[0]);
        designModified = TRUE;
        break;
    case 'R':                           // back to the standard picture
        PushUndo();
        StandardSprite(Spr());
        designModified = TRUE;
        break;
    }
}

// ============================================================================
//  Drawing
// ============================================================================

static void Outline(int x, int y, int w, int h, int t, DWORD c)
{
    GfxRect(x, y, w, t, c);
    GfxRect(x, y + h - t, w, t, c);
    GfxRect(x, y, t, h, c);
    GfxRect(x + w - t, y, t, h, c);
}

// The part of a robot or of Jack that touches things, in art pixels
static BOOL HitBox(int s, int *x0, int *y0, int *w, int *h)
{
    int pw = 0, ph = 0;
    switch (s)
    {
    case SPR_STAND: case SPR_WALK1: case SPR_WALK2: case SPR_WALK3:
    case SPR_WALK4: case SPR_WALK5: case SPR_WALK6: case SPR_JUMP:
        *w = HERO_W / 2; *h = HERO_H / 2;
        *x0 = 3; *y0 = spriteArt[s].h - *h;
        return TRUE;
    case SPR_WALKER1: case SPR_WALKER2: pw = 28; ph = 28; break;
    case SPR_HOPPER1: case SPR_HOPPER2: pw = 26; ph = 30; break;
    case SPR_FLYER1:  case SPR_FLYER2:  pw = 28; ph = 26; break;
    case SPR_TURRET:                    pw = 28; ph = 28; break;
    case SPR_BOSS1:   case SPR_BOSS2:   pw = 56; ph = 60; break;
    default: return FALSE;
    }
    *w = pw / 2; *h = ph / 2;
    *x0 = (spriteArt[s].w - *w) / 2;
    *y0 = spriteArt[s].h - *h;
    return TRUE;
}

// The picture in use: it walks, flaps, bobs or tiles a floor
static void DrawPreview(void)
{
    int x0 = SIDE_X, y0 = PREVIEW_Y, w = 260, h = PREVIEW_H;
    int set = 1, s = Spr();
    if (s >= SPR_WALL1 && s <= SPR_BRICK3)
        set = (s - SPR_WALL1) / 4 + 1;
    GfxClip(x0, y0, x0 + w, y0 + h);
    GfxBackdrop(set, frameCount / 2, 0);

    int floorY = y0 + h - TILE;
    int girder = SPR_WALL1 + (set - 1) * 4 + 1;
    if (curGroup != 3)
        for (int x = x0; x < x0 + w; x += TILE)
            GfxSprite(girder, x, floorY, 2, FALSE, -1, -1, FALSE);

    int range = w - 70, t = (frameCount * 2) % (2 * range);
    int walkX = x0 + 10 + (t < range ? t : 2 * range - t);
    BOOL back = t >= range;

    if (curGroup == 0)
    {
        if (s == SPR_STAND)
            GfxSprite(s, x0 + w / 2 - 16, floorY - 64, 2, FALSE, -1, -1, FALSE);
        else if (s == SPR_JUMP)
        {
            int hop = (int)(fabsf(sinf(frameCount * 0.05f)) * 40);
            GfxSprite(s, x0 + w / 2 - 16, floorY - 64 - hop, 2, FALSE, -1, -1, FALSE);
        }
        else                            // the whole walk, so you see the frames together
        {
            static const int bob[6] = { 0, 2, 2, 0, 2, 2 };
            int f = (frameCount * 2 / 8) % 6;
            GfxSprite(SPR_WALK1 + f, walkX, floorY - 64 - bob[f], 2, back, -1, -1, FALSE);
        }
    }
    else if (curGroup == 1)
    {
        // Robots have two frames each (the turret has one): show them in turn
        int first = s;
        if (s == SPR_WALKER2 || s == SPR_HOPPER2 || s == SPR_FLYER2 || s == SPR_BOSS2)
            first = s - 1;
        int frame = (s == SPR_TURRET) ? s : first + (frameCount / 12) % 2;
        int sw = spriteArt[s].w * 2, sh = spriteArt[s].h * 2;
        int y = floorY - sh;
        if (s == SPR_FLYER1 || s == SPR_FLYER2)
            y = y0 + 20 + (int)(sinf(frameCount * 0.08f) * 8);
        if (s == SPR_TURRET)
            GfxSprite(s, x0 + w / 2 - sw / 2, y, 2, (frameCount / 90) % 2, -1, -1, FALSE);
        else
            GfxSprite(frame, walkX, y, 2, back, -1, -1, FALSE);
    }
    else if (curGroup == 2)
    {
        int bob = (int)(sinf(frameCount * 0.1f) * 3);
        if (s == SPR_KEY)               // in all four colours
            for (int k = 0; k < 4; k++)
                GfxSprite(s, x0 + 20 + k * 60, floorY - 36 + bob, 2, FALSE, 10, KeyColour(k), FALSE);
        else if (s == SPR_EXIT || s == SPR_CRATE)
            GfxSprite(s, x0 + w / 2 - 16, floorY - spriteArt[s].h * 2, 2, FALSE, -1, -1, FALSE);
        else
            for (int k = 0; k < 3; k++)
                GfxSprite(s, x0 + 50 + k * 64, floorY - 36 + bob, 2, FALSE, -1, -1, FALSE);
    }
    else                                // tiles: a patch of floor, or a row of doors
    {
        for (int ty = 0; ty < 3; ty++)
            for (int tx = 0; tx < 9; tx++)
            {
                int from = -1, to = -1;
                if (s == SPR_DOOR) { from = 10; to = KeyColour(tx % 4); }
                if ((s == SPR_LEDGE1 || s == SPR_LEDGE2 || s == SPR_LEDGE3 || s == SPR_SPIKES) && ty < 2)
                    continue;
                if ((s == SPR_PIPE || s == SPR_LAMP) && tx % 3)
                    continue;
                if (s == SPR_PIPEH && ty != 1)
                    continue;
                GfxSprite(s, x0 + tx * TILE - 8, y0 + 4 + ty * TILE, 2, FALSE, from, to, FALSE);
            }
    }
    GfxClip(0, 0, GAME_W, GAME_H);
}

void DrawDesigner(void)
{
    GfxClip(0, 0, GAME_W, GAME_H);
    GfxRect(0, 0, GAME_W, GAME_H, 0x10101C);

    // The big grid: see-through pixels show as a chequerboard
    int cell = Cell(), w = SprW(), h = SprH(), s = Spr();
    GfxRect(GRID_X - 1, GRID_Y - 1, w * cell + 1, h * cell + 1, 0x000000);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            int c = sprPix[s][y * w + x];
            DWORD col = (c >= 0) ? pal32[c] : ((x + y) & 1) ? 0x3E3E4A : 0x2E2E38;
            GfxRect(GRID_X + x * cell, GRID_Y + y * cell, cell - 1, cell - 1, col);
        }
    int bx, by, bw, bh;
    if (HitBox(s, &bx, &by, &bw, &bh))
        Outline(GRID_X + bx * cell - 2, GRID_Y + by * cell - 2, bw * cell + 3, bh * cell + 3, 2, 0xFFD23C);
    int cx, cy;
    if (GridCell(mouseX, mouseY, &cx, &cy))
        Outline(GRID_X + cx * cell - 2, GRID_Y + cy * cell - 2, cell + 3, cell + 3, 2, 0xFFFFFF);

    // The tabs and the pictures in this group, at their own size
    for (int g = 0; g < NUM_GROUPS; g++)
        GfxRect(SIDE_X + g * (TAB_W + 3), TAB_Y - 2, TAB_W, 20, g == curGroup ? 0xFFE650 : 0x3C3C50);
    for (int i = 0, n = 0; i < NUM_ITEMS; i++)
    {
        if (items[i].group != curGroup)
            continue;
        int tx, ty;
        ThumbPos(n++, &tx, &ty);
        int t = items[i].sprite, sw = spriteArt[t].w, sh = spriteArt[t].h;
        GfxRect(tx, ty, 34, 36, 0x20203A);
        GfxSprite(t, tx + (34 - sw) / 2, ty + (36 - sh) / 2, 1, FALSE, -1, -1, FALSE);
        if (i == curItem)
            Outline(tx - 2, ty - 2, 38, 40, 2, 0xFFE650);
    }

    // The colours, with see-through first
    for (int i = 0; i <= 16; i++)
    {
        int sx, sy;
        SwatchPos(i, &sx, &sy);
        if (i == 0)
        {
            GfxRect(sx, sy, SWATCH, SWATCH, 0x3E3E4A);
            GfxRect(sx, sy, SWATCH / 2, SWATCH / 2, 0x2E2E38);
            GfxRect(sx + SWATCH / 2, sy + SWATCH / 2, SWATCH / 2, SWATCH / 2, 0x2E2E38);
        }
        else
            GfxRect(sx, sy, SWATCH, SWATCH, pal32[i - 1]);
        if (i - 1 == colour)
            Outline(sx - 3, sy - 3, SWATCH + 6, SWATCH + 6, 2, 0xFFFFFF);
    }

    DrawPreview();

    // Now the words
    GdiFlush();
    char line[120];
    wsprintf(line, "DESIGNER   %s%s", items[curItem].name, designModified ? "  *" : "");
    GfxText(fontSmall, GRID_X, 5, line, RGB(255, 220, 80));
    for (int g = 0; g < NUM_GROUPS; g++)
        GfxText(fontTiny, SIDE_X + g * (TAB_W + 3) + (TAB_W - GfxTextWidth(fontTiny, groupNames[g])) / 2,
                TAB_Y + 1, groupNames[g], g == curGroup ? RGB(0, 0, 0) : RGB(255, 255, 255));
    wsprintf(line, "Paint: %s", colour < 0 ? "see-through" : colourNames[colour]);
    GfxText(fontSmall, GAME_W - 8 - GfxTextWidth(fontSmall, line), 5, line, RGB(255, 255, 255));

    const char *note;
    if (frameCount < savedUntil)
        note = "Saved to " SPRITE_FILE;
    else if (s == SPR_KEY || s == SPR_DOOR)
        note = "Yellow parts are drawn in the key's or door's own colour.";
    else if (curGroup <= 1)
        note = "Draw it facing right. Yellow box: the part that touches things.";
    else if (curGroup == 3)
        note = "Each pixel is 2 x 2 in the game: a square is 32 x 32.";
    else
        note = "Things you pick up bob gently up and down in the game.";
    GfxText(fontTiny, GRID_X, GRID_Y + GRID_PIX + 4, note,
            frameCount < savedUntil ? RGB(120, 255, 120) : RGB(170, 200, 230));

    static const char *help[] =
    {
        "Click: paint   Right: rub out   Shift: fill",
        "Ctrl+click: pick up colour   H/V: flip",
        "Arrows: slide   Ctrl+Z: undo   Del: clear",
        "Ctrl+C/V: copy/paste   R: standard one",
        "Tab: group  PgUp/Dn: next  Ctrl+S: save",
    };
    for (int i = 0; i < 5; i++)
        GfxText(fontTiny, SIDE_X, PREVIEW_Y + PREVIEW_H + 3 + i * 12, help[i], RGB(170, 200, 230));
}
