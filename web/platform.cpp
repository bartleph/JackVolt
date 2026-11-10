// ============================================================================
//  web/platform.cpp - the browser version's link between the game and the
//  web page
//
//  The game itself is unchanged: this file provides the few Windows things it
//  uses (see web/windows.h), by calling on the JavaScript in index.html, and
//  gives the page the functions it calls - one per frame, one per key, and
//  so on. In Windows, main.cpp, pad.cpp and inputbox.cpp do this job.
// ============================================================================

#include "../volt.h"
#include <mmsystem.h>

#define JS(name) __attribute__((import_module("env"), import_name(#name)))
#define EXPORT(name) extern "C" __attribute__((export_name(#name)))

// ---- What the page provides (in index.html)
extern "C" {
JS(js_text)    void js_text(int x, int y, const char *s, int len, int height, int colour);
JS(js_measure) int  js_measure(const char *s, int len, int height);
JS(js_message) int  js_message(const char *text, int type);
JS(js_prompt)  void js_prompt(const char *prompt, const char *initial);
JS(js_size)    int  js_size(const char *name);
JS(js_read)    void js_read(const char *name, char *buf);
JS(js_write)   int  js_write(const char *name, const char *data, int len);
}

HWND hwndMain;
void QuitGame(void) {}                  // a web page can't close itself

// ============================================================================
//  Keys and the gamepad
// ============================================================================

static unsigned char keys[256];

SHORT GetAsyncKeyState(int vk) { return keys[vk & 255] ? (SHORT)0x8000 : 0; }
SHORT GetKeyState(int vk)      { return keys[vk & 255] ? (SHORT)-128 : 0; }

// The page reads gamepads with the browser's Gamepad API and passes the
// buttons on in the same form as XInput's (see pad.cpp)
#define PAD_UP    0x0001
#define PAD_DOWN  0x0002
#define PAD_LEFT  0x0004
#define PAD_RIGHT 0x0008
#define PAD_START 0x0010
#define PAD_BACK  0x0020
#define PAD_A     0x1000
#define PAD_B     0x2000
#define PAD_X     0x4000
#define PAD_Y     0x8000
static WORD padHeld, padLast;

void PadPoll(void) {}

BOOL PadHeld(int vk)
{
    switch (vk)
    {
    case VK_LEFT:    return (padHeld & PAD_LEFT)  != 0;
    case VK_RIGHT:   return (padHeld & PAD_RIGHT) != 0;
    case VK_UP:      return (padHeld & PAD_UP)    != 0;
    case VK_DOWN:    return (padHeld & PAD_DOWN)  != 0;
    case VK_SPACE:   return (padHeld & PAD_A)     != 0;
    case VK_CONTROL: return (padHeld & (PAD_X | PAD_B)) != 0;
    }
    return FALSE;
}

int PadPressed(void)
{
    WORD pressed = padHeld & ~padLast;
    if (pressed & PAD_START) return VK_RETURN;
    if (pressed & PAD_BACK)  return VK_ESCAPE;
    if (pressed & PAD_Y)     return 'P';
    return 0;
}

// ============================================================================
//  Message boxes and typing a name
// ============================================================================

int MessageBox(HWND, const char *text, const char *, UINT type)
{
    return js_message(text, (type & MB_YESNOCANCEL) ? 2 : (type & MB_YESNO) ? 1 : 0);
}

BOOL InputBox(HWND, const char *, const char *, char *, int)
{
    return FALSE;                       // not used in the browser: see WebAskText
}

// The page shows a box to type in, and calls web_text_done with the answer
static int  askWhat, askNumber;
static char answer[64];

void WebAskText(int what, int number, const char *prompt, const char *initial)
{
    askWhat   = what;
    askNumber = number;
    js_prompt(prompt, initial);
}

EXPORT(web_text_buffer) char *web_text_buffer(void) { return answer; }

EXPORT(web_text_done) void web_text_done(void)
{
    answer[sizeof answer - 1] = 0;
    if (askWhat == 1)
        RenameScore(askNumber, "Jack", answer);     // a new high score's name
    else if (askWhat == 2)
        SetLevelName(answer);                       // the level editor's R
    askWhat = 0;
}

// ============================================================================
//  Files: the page keeps them in the browser's storage, by name
// ============================================================================

typedef struct
{
    BOOL  used, writing;
    char  name[64];
    char *data;
    int   size, pos, room;
} FakeFile;
static FakeFile files[4];

DWORD GetModuleFileName(void *, char *path, DWORD)
{
    lstrcpy(path, "C:\\JackVolt\\Volt.exe");
    return lstrlen(path);
}

HANDLE CreateFile(const char *path, int access, int, void *, int, int, void *)
{
    const char *name = strrchr(path, '\\');
    name = name ? name + 1 : path;
    for (int i = 0; i < 4; i++)
        if (!files[i].used)
        {
            FakeFile *f = &files[i];
            memset(f, 0, sizeof *f);
            lstrcpyn(f->name, name, sizeof f->name);
            if (access == GENERIC_READ)
            {
                f->size = js_size(f->name);
                if (f->size < 0)
                    return INVALID_HANDLE_VALUE;    // no such file
                f->data = (char *)malloc(f->size + 1);
                js_read(f->name, f->data);
            }
            else
                f->writing = TRUE;
            f->used = TRUE;
            return (HANDLE)(f);
        }
    return INVALID_HANDLE_VALUE;
}

DWORD GetFileSize(HANDLE h, DWORD *) { return ((FakeFile *)h)->size; }

BOOL ReadFile(HANDLE h, void *buf, DWORD n, DWORD *got, void *)
{
    FakeFile *f = (FakeFile *)h;
    if ((int)n > f->size - f->pos) n = f->size - f->pos;
    memcpy(buf, f->data + f->pos, n);
    f->pos += n;
    *got = n;
    return TRUE;
}

BOOL WriteFile(HANDLE h, const void *buf, DWORD n, DWORD *put, void *)
{
    FakeFile *f = (FakeFile *)h;
    if (f->size + (int)n > f->room)
    {
        f->room = (f->size + n) * 2 + 1024;
        f->data = (char *)realloc(f->data, f->room);
    }
    memcpy(f->data + f->size, buf, n);
    f->size += n;
    *put = n;
    return TRUE;
}

BOOL CloseHandle(HANDLE h)
{
    FakeFile *f = (FakeFile *)h;
    BOOL ok = TRUE;
    if (f->writing)
        ok = js_write(f->name, f->data ? f->data : "", f->size);
    free(f->data);
    f->used = FALSE;
    return ok;
}

// ============================================================================
//  Drawing: the picture is memory; text is passed to the page
// ============================================================================

static DWORD *dib;
static unsigned char rgba[GAME_W * GAME_H * 4];
static int   curFont = 14;
static COLORREF textColour;

HDC CreateCompatibleDC(HDC) { return (HDC)1; }

HBITMAP CreateDIBSection(HDC, BITMAPINFO *bi, int, void **bits, void *, int)
{
    dib = (DWORD *)calloc(bi->bmiHeader.biWidth * -bi->bmiHeader.biHeight, 4);
    *bits = dib;
    return (HBITMAP)0x100000;
}

// Fonts are just their height; anything else selected (the picture) is ignored
HFONT CreateFont(int h, int, int, int, int, int, int, int, int, int, int, int, int, const char *)
{
    return (HFONT)(long)h;
}

HGDIOBJ SelectObject(HDC, HGDIOBJ o)
{
    long v = (long)o;
    if (v > 0 && v < 1000)
    {
        HGDIOBJ old = (HGDIOBJ)(long)curFont;
        curFont = (int)v;
        return old;
    }
    return o;
}

COLORREF SetTextColor(HDC, COLORREF c)
{
    COLORREF old = textColour;
    textColour = c;
    return old;
}

BOOL TextOut(HDC, int x, int y, const char *s, int n)
{
    js_text(x, y, s, n, curFont, (int)textColour);
    return TRUE;
}

BOOL GetTextExtentPoint32(HDC, const char *s, int n, SIZE *size)
{
    size->cx = js_measure(s, n, curFont);
    size->cy = curFont;
    return TRUE;
}

// ============================================================================
//  Sound: the page asks for samples; the game's mixer keeps its buffers full
// ============================================================================

static WAVEHDR *queue[16];
static int      queued, queuePos;
static float    samples[4096];

UINT waveOutOpen(HWAVEOUT *w, UINT, WAVEFORMATEX *, DWORD, DWORD, DWORD) { *w = (HWAVEOUT)1; return 0; }
UINT waveOutPrepareHeader(HWAVEOUT, WAVEHDR *, UINT)   { return 0; }
UINT waveOutUnprepareHeader(HWAVEOUT, WAVEHDR *, UINT) { return 0; }
UINT waveOutReset(HWAVEOUT)                            { queued = 0; return 0; }
UINT waveOutClose(HWAVEOUT)                            { return 0; }

UINT waveOutWrite(HWAVEOUT, WAVEHDR *h, UINT)
{
    if (queued < 16)
    {
        h->dwFlags |= WHDR_INQUEUE;
        queue[queued++] = h;
    }
    return 0;
}

// ============================================================================
//  What the page calls
// ============================================================================

EXPORT(web_init) void web_init(int seed)
{
    srand(seed);
    SetupBackBuffer(NULL);
    MakeFonts();
    SoundInit();
    LoadScores();
    LoadLevels();
    LoadSprites();
    ShowTitle();
}

// One 1/60th of a second of the game
EXPORT(web_step) void web_step(void)
{
    int key = PadPressed();
    padLast = padHeld;
    if (key)
        PadKey(key);
    Update();
    SoundPump();
}

// Draw a frame; returns the pixels, 4 bytes each (red, green, blue, alpha).
// Text is passed to the page with js_text as it's drawn, to go on top.
EXPORT(web_render) unsigned char *web_render(void)
{
    Render();
    for (int i = 0; i < GAME_W * GAME_H; i++)
    {
        DWORD c = dib[i];
        rgba[i * 4 + 0] = (unsigned char)(c >> 16);
        rgba[i * 4 + 1] = (unsigned char)(c >> 8);
        rgba[i * 4 + 2] = (unsigned char)c;
        rgba[i * 4 + 3] = 255;
    }
    return rgba;
}

EXPORT(web_key) void web_key(int vk, int down, int repeat)
{
    keys[vk & 255] = (unsigned char)(down != 0);
    if (down)
        KeyPressed(vk, repeat);
}

EXPORT(web_mouse_down) void web_mouse_down(int x, int y, int right) { MousePressed(x, y, right); }
EXPORT(web_mouse_move) void web_mouse_move(int x, int y, int buttons) { MouseMoved(x, y, buttons); }
EXPORT(web_mouse_up)   void web_mouse_up(void)                     { MouseReleased(); }
EXPORT(web_wheel)      void web_wheel(int delta, int sideways)     { MouseWheel(delta, sideways); }
EXPORT(web_pad)        void web_pad(int buttons)                   { padHeld = (WORD)buttons; }
EXPORT(web_active)     void web_active(int on)                     { active = on; if (!on) memset(keys, 0, sizeof keys); }
EXPORT(web_state)      int  web_state(void)                        { return state; }
EXPORT(web_paused)     int  web_paused(void)                       { return paused; }

// The Files panel: write out the levels or artwork as they are now, or load
// them again after the page has changed them
EXPORT(web_export) void web_export(int what) { if (what == 1) SaveLevels(); else SaveSprites(); }
EXPORT(web_reload) void web_reload(void)
{
    LoadLevels();
    LoadSprites();
    LoadScores();
    levelNum = 0;
    lv = &levels[0];
    ShowTitle();
}

// n samples of sound (22050 a second), as numbers from -1 to 1
EXPORT(web_audio) float *web_audio(int n)
{
    if (n > 4096) n = 4096;
    for (int i = 0; i < n; i++)
    {
        if (queued == 0)
        {
            SoundPump();                // ran dry: top up
            if (queued == 0)
            {
                samples[i] = 0;
                continue;
            }
        }
        WAVEHDR *h = queue[0];
        samples[i] = ((short *)h->lpData)[queuePos++] / 32768.0f;
        if (queuePos * 2 >= (int)h->dwBufferLength)
        {
            h->dwFlags &= ~WHDR_INQUEUE; // played: the mixer can fill it again
            memmove(queue, queue + 1, (queued - 1) * sizeof queue[0]);
            queued--;
            queuePos = 0;
            SoundPump();
        }
    }
    return samples;
}
