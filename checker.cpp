// ============================================================================
//  checker.cpp - "can this level be finished?" (press V in the editor)
//
//  It tries every move Jack can make - steps, and jumps of every length,
//  steered left or right at different moments - from every place he can
//  stand, using the game's own MoveHero(). Each new place found is tried in
//  turn, until there's nowhere new. Then:
//    - keys he can reach open their doors, and the spring boots let him jump
//      higher, so it looks again;
//    - cracked blocks and crates he can shoot are opened (crates are solid
//      until then, and what's inside must still be reached), so it looks again;
//    - a boss he can shoot drops the yellow key.
//  Robots are left out: Jack can always shoot them. Spikes aren't: a route
//  that touches them doesn't count.
// ============================================================================

#include "volt.h"

BOOL checkDone, checkGood;
char checkSummary[120];
unsigned char checkMark[MAX_H][MAX_W];

// Places Jack has stood, one bit for each pixel position in the level
#define SPAN_X (MAX_W * TILE)
static unsigned char seen[SPAN_X * MAX_H * TILE / 8];
static BOOL Seen(int x, int y)
{
    unsigned i = (unsigned)(y * SPAN_X + x);
    return (seen[i >> 3] >> (i & 7)) & 1;
}
static void SetSeen(int x, int y)
{
    unsigned i = (unsigned)(y * SPAN_X + x);
    seen[i >> 3] |= (unsigned char)(1 << (i & 7));
}

typedef struct { short x, y; } Place;
static Place *places;                   // every place found (and still to try)
static int    numPlaces, roomPlaces;

static unsigned char touched[MAX_ACTORS];
static unsigned char pretendKeys[256];

static BOOL Overlap(float ax, float ay, int aw, int ah, float bx, float by, int bw, int bh)
{
    return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

static void AddPlace(int x, int y)
{
    if (x < 0 || y < 0 || x >= SPAN_X || y >= MAX_H * TILE || Seen(x, y))
        return;
    SetSeen(x, y);
    if (numPlaces == roomPlaces)
    {
        int more = roomPlaces ? roomPlaces * 2 : 4096;
        Place *p = (Place *)realloc(places, more * sizeof(Place));
        if (p == NULL)
            return;
        places = p;
        roomPlaces = more;
    }
    places[numPlaces].x = (short)x;
    places[numPlaces].y = (short)y;
    numPlaces++;
}

// One move from a standing place: hold a direction from frame steerFrom to
// steerTo, and jump for the first jumpFrames frames. If he lands safely,
// the new place is added.
static void TryMove(Place from, int dir, int jumpFrames, int steerFrom, int steerTo)
{
    HeroSetState(from.x, from.y, 0, TRUE);
    simKeys = pretendKeys;
    BOOL airborne = FALSE;
    for (int f = 0; f < 240; f++)
    {
        memset(pretendKeys, 0, sizeof pretendKeys);
        if (f >= steerFrom && f < steerTo)
            pretendKeys[dir < 0 ? VK_LEFT : VK_RIGHT] = (dir != 0);
        if (f < jumpFrames)
            pretendKeys[VK_SPACE] = 1;
        MoveHero();
        if (HeroOnSpikes())
            break;
        for (int i = 0; i < numActors; i++)
            if (!touched[i] && Overlap(heroX, heroY, HERO_W, HERO_H, actors[i].x, actors[i].y, actors[i].w, actors[i].h))
                touched[i] = 1;
        if (!onGround)
            airborne = TRUE;
        if (onGround && (jumpFrames == 0 ? f >= steerTo - 1 : airborne))
        {
            AddPlace((int)(heroX + 0.5f), (int)(heroY + 0.5f));
            break;
        }
    }
    simKeys = NULL;
}

static void TryAll(Place p)
{
    static const int jumps[4] = { 1, 6, 12, 60 };
    TryMove(p, -1, 0, 0, 2);                            // a short step each way
    TryMove(p,  1, 0, 0, 2);
    for (int j = 0; j < 4; j++)
    {
        TryMove(p, 0, jumps[j], 0, 0);                  // straight up
        for (int d = -1; d <= 1; d += 2)
        {
            TryMove(p, d, jumps[j], 0, 999);            // steered all the way
            TryMove(p, d, jumps[j], 10, 999);           // steered after a while
            TryMove(p, d, jumps[j], 20, 999);
            TryMove(p, d, jumps[j], 0, 10);             // steered, then let go
            TryMove(p, d, jumps[j], 0, 20);
        }
    }
}

// Try every move from every place found since 'from' (including the new
// places those moves find), until there's nowhere new
static void Grow(int from)
{
    for (int next = from; next < numPlaces; next++)
        TryAll(places[next]);
}

// Squares that opened up since the last look (doors and cracked blocks)
#define MAX_CHANGED 400
static int changedX[MAX_CHANGED], changedY[MAX_CHANGED], numChanged;

static void Changed(int tx, int ty)
{
    if (numChanged < MAX_CHANGED)
    {
        changedX[numChanged] = tx;
        changedY[numChanged] = ty;
        numChanged++;
    }
}

static void OpenAndNote(char door)
{
    for (int ty = 0; ty < lv->h; ty++)
        for (int tx = 0; tx < lv->w; tx++)
            if (map[ty][tx] == door)
                Changed(tx, ty);
    OpenDoors(door);
}

// Is a place near enough to a change that moves from it might now go further?
static BOOL NearChange(Place p)
{
    int tx = p.x / TILE, ty = p.y / TILE;
    for (int i = 0; i < numChanged; i++)
        if (abs(changedX[i] - tx) <= 8 && abs(changedY[i] - ty) <= 7)
            return TRUE;
    return FALSE;
}

// Could Jack, standing here, shoot this box? (His shot is at waist height
// and goes about 14 squares; nothing solid may be in the way.)
static BOOL CanShoot(Place p, float bx, float by, int bw, int bh)
{
    float sy = p.y + 24.0f;
    if (!(sy < by + bh && by < sy + 6))
        return FALSE;
    int ty = (int)((sy + 3) / TILE);
    int hx = (p.x + HERO_W / 2) / TILE, t = (int)((bx + bw / 2) / TILE);
    if (abs(t - hx) > 14)
        return FALSE;
    int a = hx < t ? hx : t, b = hx < t ? t : hx;
    for (int x = a + 1; x < b; x++)
        if (Solid(x, ty))
            return FALSE;
    return TRUE;
}

static BOOL CanShootFromAnywhere(float bx, float by, int bw, int bh)
{
    for (int i = 0; i < numPlaces; i++)
        if (CanShoot(places[i], bx, by, bw, bh))
            return TRUE;
    return FALSE;
}

void CheckLevel(int n)
{
    // The checker plays the level behind the scenes: keep the game as it was
    int  keepState = state, keepScore = score, keepFire = firePower;
    BOOL keepBoots = hasBoots;
    simulating = TRUE;                  // no sounds or sparks
    StartLevel(n);
    memset(checkMark, 0, sizeof checkMark);
    memset(touched, 0, sizeof touched);

    // Let him drop onto the floor first
    simKeys = pretendKeys;
    memset(pretendKeys, 0, sizeof pretendKeys);
    for (int f = 0; f < 300 && !onGround; f++)
        MoveHero();
    simKeys = NULL;
    Place start = { (short)(heroX + 0.5f), (short)(heroY + 0.5f) };

    BOOL have[4] = { FALSE, FALSE, FALSE, FALSE };
    hasBoots = FALSE;

    memset(seen, 0, sizeof seen);
    numPlaces = 0;
    AddPlace(start.x, start.y);
    Grow(0);

    for (int round = 0; round < 40; round++)
    {
        BOOL changed = FALSE, newBoots = FALSE;
        numChanged = 0;

        for (int i = 0; i < numActors; i++)
        {
            Actor *a = &actors[i];
            if (a->kind == A_CRATE && CanShootFromAnywhere(a->x, a->y, a->w, a->h))
            {
                // Shot open: it's out of the way, and what was inside can be picked up
                Changed((int)(a->x / TILE), (int)(a->y / TILE));
                OpenCrate(a);
                changed = TRUE;
            }
            if (a->kind == A_BOSS && !have[3] && CanShootFromAnywhere(a->x, a->y, a->w, a->h))
            {
                have[3] = TRUE;         // beaten: he drops the yellow key
                OpenAndNote('Y');
                changed = TRUE;
            }
            if (!touched[i])
                continue;
            if (a->kind >= A_KEYR && a->kind <= A_KEYY && !have[a->kind - A_KEYR])
            {
                have[a->kind - A_KEYR] = TRUE;
                OpenAndNote("RGBY"[a->kind - A_KEYR]);
                changed = TRUE;
            }
            if (a->kind == A_BOOTS && !hasBoots)
            {
                hasBoots = TRUE;
                changed = newBoots = TRUE;
            }
        }

        // Cracked blocks he can shoot fall, the whole stack at once
        for (int ty = 0; ty < lv->h; ty++)
            for (int tx = 0; tx < lv->w; tx++)
                if (map[ty][tx] == '%' && CanShootFromAnywhere((float)(tx * TILE), (float)(ty * TILE), TILE, TILE))
                {
                    int top = ty, bottom = ty;
                    while (top > ty - 3 && TileAt(tx, top - 1) == '%') top--;
                    while (bottom < ty + 3 && TileAt(tx, bottom + 1) == '%') bottom++;
                    for (int y = top; y <= bottom; y++)
                    {
                        map[y][tx] = '.';
                        Changed(tx, y);
                    }
                    changed = TRUE;
                }
        if (!changed)
            break;

        // Look again: from everywhere if he's got the boots, otherwise just
        // from places near what's opened up
        int before = numPlaces;
        for (int i = 0; i < before; i++)
            if (newBoots || NearChange(places[i]))
                TryAll(places[i]);
        Grow(before);
    }

    // Mark where he can stand (the square his feet are in)...
    for (int i = 0; i < numPlaces; i++)
    {
        int tx = (places[i].x + HERO_W / 2) / TILE, ty = (places[i].y + HERO_H - 1) / TILE;
        if (tx >= 0 && tx < MAX_W && ty >= 0 && ty < MAX_H)
            checkMark[ty][tx] = 1;
    }

    // ...and anything he can't get to
    BOOL exitOK = FALSE, anyExit = FALSE;
    int  missed = 0;
    for (int i = 0; i < numActors; i++)
    {
        const Actor *a = &actors[i];
        if (a->kind == A_NONE || (a->kind >= A_WALKER && a->kind <= A_BOSS))
            continue;
        BOOL got = touched[i];
        if (a->kind == A_EXIT)
        {
            anyExit = TRUE;
            if (got) exitOK = TRUE;
        }
        if (!got)
        {
            missed++;
            int tx = (int)((a->x + a->w / 2) / TILE), ty = (int)((a->y + a->h - 1) / TILE);
            if (tx >= 0 && tx < MAX_W && ty >= 0 && ty < MAX_H)
                checkMark[ty][tx] = 2;
        }
    }

    checkGood = exitOK && missed == 0;
    if (!anyExit)
        lstrcpy(checkSummary, "There's no exit (X) in this level.");
    else if (!exitOK)
        wsprintf(checkSummary, "Jack can't reach the exit! (%d thing%s out of reach)", missed, missed == 1 ? "" : "s");
    else if (missed)
        wsprintf(checkSummary, "The exit can be reached, but %d thing%s can't (flashing).", missed, missed == 1 ? "" : "s");
    else
        lstrcpy(checkSummary, "All good: Jack can reach the exit and everything in the level.");
    checkDone = TRUE;

    free(places);
    places = NULL;
    numPlaces = roomPlaces = 0;
    simulating = FALSE;
    state = keepState;
    score = keepScore;
    firePower = keepFire;
    hasBoots = keepBoots;
}
