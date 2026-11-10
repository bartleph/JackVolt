// ============================================================================
//  levels.cpp - reading levels.txt, and some small file helpers
//
//  levels.txt sits next to the .exe. The first time the game runs it writes
//  the standard levels there; after that, edit it in Notepad to change them
//  or add your own (up to 16). Each level looks like this:
//
//      LEVEL Steel City Streets
//      TILESET 1          walls and floors: 1 city, 2 factory, 3 fortress
//      BACKDROP 1         the picture behind: 1 city, 2 factory, 3 fortress
//      MUSIC 1            which tune, 1 to 3
//      MAP
//      ...up to 64 lines of up to 128 characters...
//      END
//
//  The map key is at the top of levels.txt.
// ============================================================================

#include "volt.h"

Level levels[MAX_LEVELS];
int   numLevels;

// ============================================================================
//  Small file and text helpers
// ============================================================================

// Full path of a file in the same folder as the .exe
void ExePath(const char *fileName, char *path)
{
    GetModuleFileName(NULL, path, MAX_PATH);
    char *slash = strrchr(path, '\\');
    char *name  = slash ? slash + 1 : path;
    if ((name - path) + lstrlen(fileName) < MAX_PATH)
        lstrcpy(name, fileName);
}

// Read a whole file into a new zero-terminated buffer. Free it with free().
char *ReadTextFile(const char *path)
{
    HANDLE h = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return NULL;

    DWORD size = GetFileSize(h, NULL);
    char *buf = NULL;
    if (size != INVALID_FILE_SIZE && size < 4 * 1024 * 1024)
    {
        buf = (char *)malloc(size + 1);
        DWORD got = 0;
        if (buf && ReadFile(h, buf, size, &got, NULL) && got == size)
            buf[size] = 0;
        else
        {
            free(buf);
            buf = NULL;
        }
    }
    CloseHandle(h);
    return buf;
}

BOOL WriteTextFile(const char *path, const char *text)
{
    HANDLE h = CreateFile(path, GENERIC_WRITE, 0, NULL,
                          CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return FALSE;
    DWORD len = (DWORD)strlen(text), written = 0;
    BOOL ok = WriteFile(h, text, len, &written, NULL) && written == len;
    CloseHandle(h);
    return ok;
}

// Does the line start with this word? (not case-sensitive)
static BOOL StartsWith(const char *line, const char *word)
{
    while (*word)
    {
        char a = *line++, b = *word++;
        if (a >= 'a' && a <= 'z') a -= 32;
        if (a != b) return FALSE;
    }
    return TRUE;
}

// Copy the next line of text into 'line' (without its line ending).
// Returns where the following line starts.
const char *NextLine(const char *text, char *line, int size)
{
    int len = 0;
    while (*text && *text != '\n' && *text != '\r')
    {
        if (len < size - 1) line[len++] = *text;
        text++;
    }
    if (*text == '\r') text++;
    if (*text == '\n') text++;
    line[len] = 0;
    return text;
}

static const char *SkipSpaces(const char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

static int Number(const char *s, int lo, int hi, int otherwise)
{
    int n = atoi(SkipSpaces(s));
    return (n >= lo && n <= hi) ? n : otherwise;
}

// ============================================================================
//  levels.txt
// ============================================================================

// A finished level must have somewhere for Jack to start
static BOOL HasStart(const Level *l)
{
    for (int y = 0; y < l->h; y++)
        if (strchr(l->rows[y], 'P'))
            return TRUE;
    return FALSE;
}

int ParseLevelsInto(const char *text, Level *levels, int maxLevels)
{
    char  line[512];
    int   count = 0;
    Level *l = NULL;
    BOOL  inMap = FALSE;

    while (*text)
    {
        text = NextLine(text, line, sizeof line);

        if (inMap)
        {
            if (StartsWith(SkipSpaces(line), "END"))
            {
                inMap = FALSE;
                if (l->h > 0 && HasStart(l))
                    count++;             // keep it
                l = NULL;
                continue;
            }
            if (l->h >= MAX_H)
                continue;
            int len = lstrlen(line);
            if (len > MAX_W) len = MAX_W;
            char *row = l->rows[l->h++];
            memset(row, '.', MAX_W);
            row[MAX_W] = 0;
            for (int x = 0; x < len; x++)
                row[x] = (line[x] == ' ') ? '.' : line[x];
            if (len > l->w)
                l->w = len;
            continue;
        }

        const char *p = SkipSpaces(line);
        if (StartsWith(p, "LEVEL") && count < maxLevels)
        {
            l = &levels[count];
            memset(l, 0, sizeof *l);
            lstrcpyn(l->name, SkipSpaces(p + 5), sizeof l->name);
            l->tileset = l->backdrop = l->music = 1;
        }
        else if (l && StartsWith(p, "TILESET"))   l->tileset  = Number(p + 7, 1, 3, 1);
        else if (l && StartsWith(p, "BACKDROP"))  l->backdrop = Number(p + 8, 1, 3, 1);
        else if (l && StartsWith(p, "MUSIC"))     l->music    = Number(p + 5, 1, 3, 1);
        else if (l && StartsWith(p, "MAP"))       inMap = TRUE;
    }

    // Narrow maps are filled out with wall, so the screen is never empty
    for (int i = 0; i < count; i++)
        for (int y = 0; y < levels[i].h; y++)
            for (int x = levels[i].w; x < MAX_W; x++)
                levels[i].rows[y][x] = '#';
    return count;
}

int ParseLevels(const char *text)
{
    return ParseLevelsInto(text, levels, MAX_LEVELS);
}

void LoadLevels(void)
{
    char path[MAX_PATH];
    ExePath(LEVEL_FILE, path);
    char *text = ReadTextFile(path);
    if (text == NULL)
    {
        WriteTextFile(path, defaultLevels);     // first run: make the file
        numLevels = ParseLevels(defaultLevels);
        return;
    }
    numLevels = ParseLevels(text);
    free(text);
    if (numLevels == 0)
    {
        MessageBox(hwndMain, "There are no levels in " LEVEL_FILE " that can be played,\n"
                   "so the standard levels will be used.", "Jack Volt", MB_ICONWARNING);
        numLevels = ParseLevels(defaultLevels);
    }
}
