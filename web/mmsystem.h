// web/mmsystem.h - the sound card, for the browser version (see platform.cpp)
#pragma once
typedef void *HWAVEOUT;
typedef struct { WORD wFormatTag, nChannels; DWORD nSamplesPerSec, nAvgBytesPerSec;
                 WORD nBlockAlign, wBitsPerSample, cbSize; } WAVEFORMATEX;
typedef struct { LPSTR lpData; DWORD dwBufferLength, dwBytesRecorded, dwUser, dwFlags, dwLoops;
                 void *lpNext; DWORD reserved; } WAVEHDR;
#define WAVE_FORMAT_PCM 1
#define WAVE_MAPPER ((UINT)-1)
#define CALLBACK_NULL 0
#define MMSYSERR_NOERROR 0
#define WHDR_DONE 1
#define WHDR_PREPARED 2
#define WHDR_INQUEUE 16
UINT waveOutOpen(HWAVEOUT *w, UINT dev, WAVEFORMATEX *f, DWORD cb, DWORD inst, DWORD flags);
UINT waveOutPrepareHeader(HWAVEOUT w, WAVEHDR *h, UINT size);
UINT waveOutUnprepareHeader(HWAVEOUT w, WAVEHDR *h, UINT size);
UINT waveOutWrite(HWAVEOUT w, WAVEHDR *h, UINT size);
UINT waveOutReset(HWAVEOUT w);
UINT waveOutClose(HWAVEOUT w);
