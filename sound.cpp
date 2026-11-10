// ============================================================================
//  sound.cpp - a little sound mixer, the sound effects and the music
//
//  Windows' PlaySound can only play one sound at a time, so instead we talk
//  to the sound card directly with the waveOut functions. We keep a few short
//  buffers queued up; whenever the card finishes one, SoundPump() fills it
//  with the next slice of sound - the music and any effects added together -
//  and hands it back. The game loop calls SoundPump() all the time.
//
//  Effects are lists of beeps (or bursts of noise) that slide from one pitch
//  to another. The music has two voices, a tune and a bass line, written out
//  as note names below.
// ============================================================================

#include "volt.h"
#include <mmsystem.h>
#include <stdio.h>

#define MIX_RATE    22050               // samples per second
#ifndef MIX_BUFS
#define MIX_BUFS    4                   // buffers queued at once (the browser version uses more)
#endif
#define MIX_LEN     512                 // samples per buffer (about 1/43 of a second)
#define CHANNELS    6                   // effects that can play at the same time
#define FX_VOLUME   150
#define TUNE_VOL    2400
#define BASS_VOL    1900

BOOL soundOn = TRUE;
BOOL musicOn = TRUE;

static HWAVEOUT wave;
static WAVEHDR  hdr[MIX_BUFS];
static short    bufData[MIX_BUFS][MIX_LEN];

// ============================================================================
//  The effects
// ============================================================================

typedef struct { int startHz, endHz, ms; BOOL noise; } Tone;
typedef struct { Tone tones[5]; } EffectDef;

static const EffectDef effectDefs[NUM_SOUNDS] =
{
    {{ { 1400,  400,  60, 0 } }},                                       // shoot
    {{ { 3000, 1000,  50, 1 } }},                                       // hit
    {{ { 2400,  200, 320, 1 } }},                                       // boom
    {{ {  900,  900,  40, 0 }, { 1350, 1350, 70, 0 } }},                // pick up
    {{ {  660,  660,  60, 0 }, {  880,  880, 60, 0 }, { 1320, 1320, 140, 0 } }},   // key
    {{ {  150,  500, 260, 0 } }},                                       // door
    {{ {  420,  140, 160, 0 } }},                                       // hurt
    {{ {  800,  100, 500, 0 }, { 1200,   80, 500, 1 } }},               // die
    {{ {  250,  620,  90, 0 } }},                                       // jump
    {{ {  523,  523, 100, 0 }, {  659,  659, 100, 0 }, { 784, 784, 100, 0 }, { 1047, 1047, 300, 0 } }},  // exit
    {{ { 4000,  800, 130, 1 } }},                                       // brick
    {{ {  400,  800,  80, 0 }, {  600, 1200, 80, 0 }, { 800, 1600, 140, 0 } }},     // power up
    {{ { 1800,  600,  90, 0 } }},                                       // a robot fires
    {{ {  392,  392, 100, 0 }, {  523,  523, 100, 0 }, { 659, 659, 100, 0 }, { 784, 784, 320, 0 } }},   // start
    {{ { 3000,   60, 1300, 1 } }},                                      // big boom
    {{ { 1500,  300, 160, 1 } }},                                       // crate
};

typedef struct
{
    signed char *data;                  // samples, -42 to +42
    int          len;
} Effect;

typedef struct
{
    const Effect *fx;                   // NULL when the channel is free
    int           pos;
} Channel;

static Effect  effects[NUM_SOUNDS];
static Channel chans[CHANNELS];

static void BuildEffect(int n)
{
    const EffectDef *d = &effectDefs[n];
    int samples = 0;
    for (int i = 0; i < 5 && d->tones[i].ms; i++)
        samples += MIX_RATE / 1000 * d->tones[i].ms;

    signed char *p = (signed char *)malloc(samples > 0 ? samples : 1);
    effects[n].data = p;
    effects[n].len  = p ? samples : 0;
    if (p == NULL)
        return;

    double phase = 0;
    int    held  = 0;
    for (int i = 0; i < 5 && d->tones[i].ms; i++)
    {
        const Tone *b = &d->tones[i];
        int len = MIX_RATE / 1000 * b->ms;
        for (int s = 0; s < len; s++)
        {
            double f   = b->startHz + (double)(b->endHz - b->startHz) * s / len;
            double env = 1.0 - 0.75 * s / len;          // each beep fades away
            int    amp = (int)(42 * env);
            phase += f / MIX_RATE;
            if (b->noise)
            {
                // Noise: a new random level each time the "pitch" ticks over
                if (phase >= 1.0)
                {
                    phase -= 1.0;
                    held = (rand() % 2) ? amp : -amp;
                }
                *p++ = (signed char)held;
            }
            else
            {
                if (phase >= 1.0) phase -= 1.0;
                *p++ = (signed char)(phase < 0.5 ? amp : -amp);
            }
        }
    }
}

void Play(int effect)
{
    if (!soundOn || wave == NULL || simulating)
        return;

    // Use a free channel, or else take over the one that has played longest
    int best = 0;
    for (int c = 0; c < CHANNELS; c++)
    {
        if (chans[c].fx == NULL)
        {
            best = c;
            break;
        }
        if (chans[c].pos > chans[best].pos)
            best = c;
    }
    chans[best].fx  = &effects[effect];
    chans[best].pos = 0;
}

void StopSounds(void)
{
    for (int c = 0; c < CHANNELS; c++)
        chans[c].fx = NULL;
}

// ============================================================================
//  The music: three tunes written for Jack Volt. Each line is note names
//  (A4 is the A above middle C, A#4 the black note above it, R a rest), each
//  followed by its length in sixteenths.
// ============================================================================

typedef struct
{
    const char *name;
    int         tempo;                  // milliseconds per sixteenth
    const char *tune, *bass;
} TuneText;

static const TuneText tuneText[3] =
{
    { "Steel City", 115,
      // Am | Am | F | G | Am | Am | F | E
      "E5 2 A4 2 C5 2 E5 2 D5 2 C5 2 B4 2 C5 2  A4 4 E5 4 A5 4 G5 2 E5 2 "
      "F5 2 E5 2 C5 2 A4 2 C5 4 F5 4  G5 4 D5 2 B4 2 G4 4 B4 2 D5 2 "
      "E5 2 A4 2 C5 2 E5 2 A5 2 G5 2 E5 2 C5 2  D5 2 C5 2 B4 2 C5 2 A4 8 "
      "F4 2 A4 2 C5 2 F5 2 E5 2 C5 2 A4 2 C5 2  E5 4 D5 2 B4 2 G#4 4 B4 4",
      "A2 2 A3 2 A2 2 A3 2 A2 2 A3 2 A2 2 A3 2  A2 2 A3 2 A2 2 A3 2 A2 2 A3 2 A2 2 A3 2 "
      "F2 2 F3 2 F2 2 F3 2 F2 2 F3 2 F2 2 F3 2  G2 2 G3 2 G2 2 G3 2 G2 2 G3 2 G2 2 G3 2 "
      "A2 2 A3 2 A2 2 A3 2 A2 2 A3 2 A2 2 A3 2  A2 2 A3 2 A2 2 A3 2 A2 2 A3 2 A2 2 A3 2 "
      "F2 2 F3 2 F2 2 F3 2 F2 2 F3 2 F2 2 F3 2  E2 2 E3 2 E2 2 E3 2 E2 2 E3 2 E2 2 E3 2" },

    { "The Gearworks", 105,
      // Dm | Dm | Bb | C | Dm | Dm | Gm | A
      "D5 2 F5 2 A5 2 F5 2 D5 2 F5 2 A5 4  G5 2 F5 2 E5 2 D5 2 E5 4 C5 4 "
      "D5 2 F5 2 A#5 2 F5 2 D5 2 F5 2 A#5 4  C6 4 A#5 2 A5 2 G5 4 E5 4 "
      "D5 2 F5 2 A5 2 F5 2 D6 4 A5 4  G5 2 F5 2 E5 2 F5 2 D5 8 "
      "G4 2 A#4 2 D5 2 G5 2 F5 2 D5 2 A#4 2 D5 2  C#5 4 E5 4 A5 4 G5 2 E5 2",
      "D2 2 R 2 D3 2 D2 2 R 2 D2 2 D3 2 R 2  D2 2 R 2 D3 2 D2 2 R 2 D2 2 D3 2 R 2 "
      "A#1 2 R 2 A#2 2 A#1 2 R 2 A#1 2 A#2 2 R 2  C2 2 R 2 C3 2 C2 2 R 2 C2 2 C3 2 R 2 "
      "D2 2 R 2 D3 2 D2 2 R 2 D2 2 D3 2 R 2  D2 2 R 2 D3 2 D2 2 R 2 D2 2 D3 2 R 2 "
      "G1 2 R 2 G2 2 G1 2 R 2 G1 2 G2 2 R 2  A1 2 R 2 A2 2 A1 2 R 2 A1 2 A2 2 R 2" },

    { "Fortress", 130,
      // Em | C | D | B | Em | C | D | B
      "E5 4 G5 4 B5 4 G5 4  C6 4 B5 4 G5 4 E5 4  F#5 4 A5 4 D6 4 A5 4  D#5 4 F#5 4 B5 8 "
      "E5 2 E5 2 G5 2 E5 2 B5 4 A5 4  G5 2 E5 2 C5 2 E5 2 G5 4 C6 4 "
      "A5 2 F#5 2 D5 2 F#5 2 A5 4 D6 4  B5 4 A5 2 F#5 2 D#5 4 B4 4",
      "E2 4 E2 4 E3 4 E2 4  C2 4 C2 4 C3 4 C2 4  D2 4 D2 4 D3 4 D2 4  B1 4 B1 4 B2 4 B1 4 "
      "E2 4 E2 4 E3 4 E2 4  C2 4 C2 4 C3 4 C2 4  D2 4 D2 4 D3 4 D2 4  B1 4 B1 4 B2 4 B1 4" },
};

#define MAX_NOTES 200
typedef struct { unsigned char note, len; } Note;     // note 0 = a rest
typedef struct { Note notes[MAX_NOTES]; int count; } Voice;

typedef struct
{
    Voice  v;
    int    index, left, len;             // the note playing, and samples to go
    double phase, freq;
} Player;

static Voice  voices[3][2];              // [tune][0 = tune, 1 = bass]
static Player players[2];
static int    playingTune = -1;

// "C#5 2" -> note number (60 = middle C) and length
static void ParseVoice(const char *s, Voice *v)
{
    static const int semis[7] = { 9, 11, 0, 2, 4, 5, 7 };     // A B C D E F G
    v->count = 0;
    while (*s && v->count < MAX_NOTES)
    {
        while (*s == ' ') s++;
        if (!*s) break;
        int note = 0;
        if (*s == 'R')
            s++;
        else if (*s >= 'A' && *s <= 'G')
        {
            note = semis[*s++ - 'A'];
            if (*s == '#') { note++; s++; }
            note += 12 * (*s++ - '0' + 1);
        }
        while (*s == ' ') s++;
        int len = atoi(s);
        while (*s >= '0' && *s <= '9') s++;
        if (len > 0)
        {
            v->notes[v->count].note = (unsigned char)note;
            v->notes[v->count].len  = (unsigned char)len;
            v->count++;
        }
    }
}

static int VoiceSample(Player *p, int tempo, int volume)
{
    if (p->v.count == 0)
        return 0;
    if (p->left <= 0)
    {
        p->index = (p->index + 1) % p->v.count;
        const Note *n = &p->v.notes[p->index];
        p->len = p->left = MIX_RATE * tempo / 1000 * n->len;
        p->freq = n->note ? 440.0 * pow(2.0, (n->note - 69) / 12.0) : 0;
    }
    p->left--;

    // Each note sounds for most of its length: crisp, not mushy
    if (p->freq == 0 || p->left < p->len / 5)
        return 0;
    p->phase += p->freq / MIX_RATE;
    if (p->phase >= 1.0) p->phase -= 1.0;
    return p->phase < 0.5 ? volume : -volume;
}

// ============================================================================
//  The mixer
// ============================================================================

static void Mix(short *out, int n)
{
    // Start the level's tune from the top whenever a level (re)starts
    static int lastLevel = -1, lastState = -1;
    BOOL starting = (state == ST_PLAYING && (lastState != ST_PLAYING || lastLevel != levelNum));
    lastLevel = levelNum;
    lastState = state;
    if (starting && lv)
    {
        playingTune = lv->music - 1;
        for (int i = 0; i < 2; i++)
        {
            players[i].v = voices[playingTune][i];
            players[i].index = -1;
            players[i].left  = 0;
        }
    }

    BOOL music = musicOn && state == ST_PLAYING && !paused && active && playingTune >= 0;
    int  tempo = (playingTune >= 0) ? tuneText[playingTune].tempo : 100;

    for (int i = 0; i < n; i++)
    {
        int s = 0;
        for (int c = 0; c < CHANNELS; c++)
        {
            Channel *ch = &chans[c];
            if (ch->fx == NULL)
                continue;
            if (ch->pos < ch->fx->len)
                s += ch->fx->data[ch->pos] * FX_VOLUME;
            if (++ch->pos >= ch->fx->len)
                ch->fx = NULL;
        }
        if (music)
        {
            s += VoiceSample(&players[0], tempo, TUNE_VOL);
            s += VoiceSample(&players[1], tempo, BASS_VOL);
        }

        if (s > 32767)  s = 32767;      // keep within what 16 bits can hold
        if (s < -32768) s = -32768;
        out[i] = (short)s;
    }
}

void SoundPump(void)
{
    if (wave == NULL)
        return;
    for (int i = 0; i < MIX_BUFS; i++)
        if (!(hdr[i].dwFlags & WHDR_INQUEUE))   // finished playing (or never started)
        {
            Mix(bufData[i], MIX_LEN);
            waveOutWrite(wave, &hdr[i], sizeof(WAVEHDR));
        }
}

void SoundInit(void)
{
    for (int i = 0; i < NUM_SOUNDS; i++)
        BuildEffect(i);
    for (int t = 0; t < 3; t++)
    {
        ParseVoice(tuneText[t].tune, &voices[t][0]);
        ParseVoice(tuneText[t].bass, &voices[t][1]);
    }

    WAVEFORMATEX fmt;
    memset(&fmt, 0, sizeof fmt);
    fmt.wFormatTag      = WAVE_FORMAT_PCM;
    fmt.nChannels       = 1;
    fmt.nSamplesPerSec  = MIX_RATE;
    fmt.wBitsPerSample  = 16;
    fmt.nBlockAlign     = 2;
    fmt.nAvgBytesPerSec = MIX_RATE * 2;

    if (waveOutOpen(&wave, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
    {
        wave = NULL;                    // no sound card: the game still works, silently
        return;
    }

    for (int i = 0; i < MIX_BUFS; i++)
    {
        memset(&hdr[i], 0, sizeof hdr[i]);
        hdr[i].lpData         = (LPSTR)bufData[i];
        hdr[i].dwBufferLength = MIX_LEN * sizeof(short);
        waveOutPrepareHeader(wave, &hdr[i], sizeof(WAVEHDR));
    }
}

void SoundShutdown(void)
{
    if (wave == NULL)
        return;
    waveOutReset(wave);                 // stop, and hand back all the buffers
    for (int i = 0; i < MIX_BUFS; i++)
        waveOutUnprepareHeader(wave, &hdr[i], sizeof(WAVEHDR));
    waveOutClose(wave);
    wave = NULL;
}

// For testing: write a tune to a .wav file (not used by the game)
BOOL TuneToWav(int t, const char *path, int seconds);
BOOL TuneToWav(int t, const char *path, int seconds)
{
    FILE *f = fopen(path, "wb");
    if (!f) return FALSE;
    int n = MIX_RATE * seconds;
    int bytes = n * 2;
    int hdrv[] = { 0x46464952, 36 + bytes, 0x45564157, 0x20746d66, 16, 0x00010001,
                   MIX_RATE, MIX_RATE * 2, 0x00100002, 0x61746164, bytes };
    fwrite(hdrv, 4, 11, f);
    for (int i = 0; i < 2; i++)
    {
        players[i].v = voices[t][i];
        players[i].index = -1;
        players[i].left = 0;
    }
    for (int i = 0; i < n; i++)
    {
        short s = (short)(VoiceSample(&players[0], tuneText[t].tempo, TUNE_VOL) +
                          VoiceSample(&players[1], tuneText[t].tempo, BASS_VOL));
        fwrite(&s, 2, 1, f);
    }
    fclose(f);
    return TRUE;
}
