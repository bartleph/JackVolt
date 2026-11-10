// ============================================================================
//  web/windows.h - a stand-in for Windows, for the browser version
//
//  The game is written for Windows. To run it in a web browser it's compiled
//  to WebAssembly, and this file takes the place of the real windows.h: it
//  has the same names, but they lead to web/platform.cpp, which passes the
//  work on to the browser (drawing, text, sound, keys, saving files).
// ============================================================================
#pragma once
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

typedef int BOOL;
typedef unsigned int DWORD, UINT, COLORREF;
typedef unsigned short WORD;
typedef unsigned char BYTE;
typedef short SHORT;
typedef long LONG, LRESULT, LPARAM;
typedef unsigned long WPARAM;
typedef char *LPSTR;
typedef const char *LPCSTR;
typedef void *HWND, *HDC, *HFONT, *HANDLE, *HBITMAP, *HGDIOBJ, *HCURSOR, *HINSTANCE;

#define TRUE  1
#define FALSE 0
#define WINAPI
#define CALLBACK
#define MAX_PATH 260

#define RGB(r, g, b)  ((COLORREF)(((r) & 255) | (((g) & 255) << 8) | (((b) & 255) << 16)))
#define GetRValue(c)  ((int)((c) & 255))
#define GetGValue(c)  ((int)(((c) >> 8) & 255))
#define GetBValue(c)  ((int)(((c) >> 16) & 255))

#define lstrcpy        strcpy
#define lstrlen(s)     ((int)strlen(s))
#define lstrcmpi       strcasecmp
#define wsprintf       sprintf
static inline char *lstrcpyn(char *d, const char *s, int n) { strncpy(d, s, n); d[n - 1] = 0; return d; }

// Keys
#define VK_TAB 0x09
#define VK_RETURN 0x0D
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_MENU 0x12
#define VK_ESCAPE 0x1B
#define VK_SPACE 0x20
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_END 0x23
#define VK_HOME 0x24
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_DELETE 0x2E
#define VK_F11 0x7A
#define VK_OEM_4 0xDB
#define VK_OEM_6 0xDD
#define MK_LBUTTON 1
#define MK_RBUTTON 2
SHORT GetAsyncKeyState(int vk);
SHORT GetKeyState(int vk);

// Message boxes
#define MB_ICONWARNING 0
#define MB_ICONERROR 0
#define MB_ICONQUESTION 0
#define MB_ICONINFORMATION 0
#define MB_YESNO 0x10000
#define MB_YESNOCANCEL 0x20000
#define IDYES 6
#define IDNO 7
#define IDCANCEL 2
int MessageBox(HWND owner, const char *text, const char *caption, UINT type);

// The mouse pointer: nothing to do in a browser
#define IDC_WAIT  ((const char *)32514)
#define IDC_ARROW ((const char *)32512)
static inline HCURSOR SetCursor(HCURSOR c)              { return c; }
static inline HCURSOR LoadCursor(void *, const char *)  { return 0; }
static inline HWND    SetCapture(HWND h)                { return h; }
static inline BOOL    ReleaseCapture(void)              { return TRUE; }

// Files: kept in the browser's storage, by name
#define INVALID_HANDLE_VALUE ((HANDLE)-1)
#define INVALID_FILE_SIZE    ((DWORD)-1)
#define GENERIC_READ 1
#define GENERIC_WRITE 2
#define FILE_SHARE_READ 1
#define OPEN_EXISTING 3
#define CREATE_ALWAYS 2
#define FILE_ATTRIBUTE_NORMAL 0
HANDLE CreateFile(const char *path, int access, int share, void *sec, int how, int attr, void *t);
DWORD  GetFileSize(HANDLE h, DWORD *high);
BOOL   ReadFile(HANDLE h, void *buf, DWORD n, DWORD *got, void *o);
BOOL   WriteFile(HANDLE h, const void *buf, DWORD n, DWORD *put, void *o);
BOOL   CloseHandle(HANDLE h);
DWORD  GetModuleFileName(void *m, char *path, DWORD size);

// Drawing: the picture is ordinary memory; text is drawn by the browser
typedef struct { DWORD biSize; LONG biWidth, biHeight; WORD biPlanes, biBitCount;
                 DWORD biCompression, biSizeImage; LONG a, b; DWORD c, d; } BITMAPINFOHEADER;
typedef struct { BITMAPINFOHEADER bmiHeader; DWORD bmiColors[1]; } BITMAPINFO;
typedef struct { LONG cx, cy; } SIZE;
#define BI_RGB 0
#define DIB_RGB_COLORS 0
#define TRANSPARENT 1
#define FW_BOLD 700
#define ANSI_CHARSET 0
#define OUT_DEFAULT_PRECIS 0
#define CLIP_DEFAULT_PRECIS 0
#define NONANTIALIASED_QUALITY 0
#define FIXED_PITCH 0
#define FF_MODERN 0
HDC     CreateCompatibleDC(HDC dc);
HBITMAP CreateDIBSection(HDC dc, BITMAPINFO *bi, int usage, void **bits, void *s, int o);
HGDIOBJ SelectObject(HDC dc, HGDIOBJ o);
COLORREF SetTextColor(HDC dc, COLORREF c);
BOOL    TextOut(HDC dc, int x, int y, const char *s, int n);
BOOL    GetTextExtentPoint32(HDC dc, const char *s, int n, SIZE *size);
HFONT   CreateFont(int h, int, int, int, int, int, int, int, int, int, int, int, int, const char *face);
static inline BOOL DeleteObject(HGDIOBJ)    { return TRUE; }
static inline BOOL DeleteDC(HDC)            { return TRUE; }
static inline int  SetBkMode(HDC, int)      { return 0; }
static inline BOOL GdiFlush(void)           { return TRUE; }
