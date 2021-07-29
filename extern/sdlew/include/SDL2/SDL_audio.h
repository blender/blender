
#ifndef _SDL_audio_h
#define _SDL_audio_h

#include "SDL_stdinc.h"
#include "SDL_error.h"
#include "SDL_endian.h"
#include "SDL_mutex.h"
#include "SDL_thread.h"
#include "SDL_rwops.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Uint16 SDL_AudioFormat;

#define SDL_AUDIO_MASK_BITSIZE       (0xFF)
#define SDL_AUDIO_MASK_DATATYPE      (1<<8)
#define SDL_AUDIO_MASK_ENDIAN        (1<<12)
#define SDL_AUDIO_MASK_SIGNED        (1<<15)
#define SDL_AUDIO_BITSIZE(x)         (x & SDL_AUDIO_MASK_BITSIZE)
#define SDL_AUDIO_ISFLOAT(x)         (x & SDL_AUDIO_MASK_DATATYPE)
#define SDL_AUDIO_ISBIGENDIAN(x)     (x & SDL_AUDIO_MASK_ENDIAN)
#define SDL_AUDIO_ISSIGNED(x)        (x & SDL_AUDIO_MASK_SIGNED)
#define SDL_AUDIO_ISINT(x)           (!SDL_AUDIO_ISFLOAT(x))
#define SDL_AUDIO_ISLITTLEENDIAN(x)  (!SDL_AUDIO_ISBIGENDIAN(x))
#define SDL_AUDIO_ISUNSIGNED(x)      (!SDL_AUDIO_ISSIGNED(x))

#define AUDIO_U8        0x0008
#define AUDIO_S8        0x8008
#define AUDIO_U16LSB    0x0010
#define AUDIO_S16LSB    0x8010
#define AUDIO_U16MSB    0x1010
#define AUDIO_S16MSB    0x9010
#define AUDIO_U16       AUDIO_U16LSB
#define AUDIO_S16       AUDIO_S16LSB

#define AUDIO_S32LSB    0x8020
#define AUDIO_S32MSB    0x9020
#define AUDIO_S32       AUDIO_S32LSB

#define AUDIO_F32LSB    0x8120
#define AUDIO_F32MSB    0x9120
#define AUDIO_F32       AUDIO_F32LSB

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
#define AUDIO_U16SYS    AUDIO_U16LSB
#define AUDIO_S16SYS    AUDIO_S16LSB
#define AUDIO_S32SYS    AUDIO_S32LSB
#define AUDIO_F32SYS    AUDIO_F32LSB
#else
#define AUDIO_U16SYS    AUDIO_U16MSB
#define AUDIO_S16SYS    AUDIO_S16MSB
#define AUDIO_S32SYS    AUDIO_S32MSB
#define AUDIO_F32SYS    AUDIO_F32MSB
#endif

#define SDL_AUDIO_ALLOW_FREQUENCY_CHANGE    0x00000001
#define SDL_AUDIO_ALLOW_FORMAT_CHANGE       0x00000002
#define SDL_AUDIO_ALLOW_CHANNELS_CHANGE     0x00000004
#define SDL_AUDIO_ALLOW_ANY_CHANGE          (SDL_AUDIO_ALLOW_FREQUENCY_CHANGE|SDL_AUDIO_ALLOW_FORMAT_CHANGE|SDL_AUDIO_ALLOW_CHANNELS_CHANGE)

typedef void (SDLCALL * SDL_AudioCallback) (void *userdata, Uint8 * stream,
                                            int len);

typedef struct SDL_AudioSpec
{
    int freq;
    SDL_AudioFormat format;
    Uint8 channels;
    Uint8 silence;
    Uint16 samples;
    Uint16 padding;
    Uint32 size;
    SDL_AudioCallback callback;
    void *userdata;
} SDL_AudioSpec;

struct SDL_AudioCVT;
typedef void (SDLCALL * SDL_AudioFilter) (struct SDL_AudioCVT * cvt,
                                          SDL_AudioFormat format);

#ifdef __GNUC__

#define SDL_AUDIOCVT_PACKED __attribute__((packed))
#else
#define SDL_AUDIOCVT_PACKED
#endif

typedef struct SDL_AudioCVT
{
    int needed;
    SDL_AudioFormat src_format;
    SDL_AudioFormat dst_format;
    double rate_incr;
    Uint8 *buf;
    int len;
    int len_cvt;
    int len_mult;
    double len_ratio;
    SDL_AudioFilter filters[10];
    int filter_index;
} SDL_AUDIOCVT_PACKED SDL_AudioCVT;

typedef int SDLCALL tSDL_GetNumAudioDrivers(void);
typedef const char * SDLCALL tSDL_GetAudioDriver(int index);

typedef int SDLCALL tSDL_AudioInit(const char *driver_name);
typedef void SDLCALL tSDL_AudioQuit(void);

typedef const char * SDLCALL tSDL_GetCurrentAudioDriver(void);

typedef int SDLCALL tSDL_OpenAudio(SDL_AudioSpec * desired,
                                          SDL_AudioSpec * obtained);

typedef Uint32 SDL_AudioDeviceID;

typedef int SDLCALL tSDL_GetNumAudioDevices(int iscapture);

typedef const char * SDLCALL tSDL_GetAudioDeviceName(int index,
                                                           int iscapture);

typedef SDL_AudioDeviceID SDLCALL tSDL_OpenAudioDevice(const char
                                                              *device,
                                                              int iscapture,
                                                              const
                                                              SDL_AudioSpec *
                                                              desired,
                                                              SDL_AudioSpec *
                                                              obtained,
                                                              int
                                                              allowed_changes);

typedef enum
{
    SDL_AUDIO_STOPPED = 0,
    SDL_AUDIO_PLAYING,
    SDL_AUDIO_PAUSED
} SDL_AudioStatus;
typedef SDL_AudioStatus SDLCALL tSDL_GetAudioStatus(void);

extern DECLSPEC SDL_AudioStatus SDLCALL
SDL_GetAudioDeviceStatus(SDL_AudioDeviceID dev);

typedef void SDLCALL tSDL_PauseAudio(int pause_on);
typedef void SDLCALL tSDL_PauseAudioDevice(SDL_AudioDeviceID dev,
                                                  int pause_on);

typedef SDL_AudioSpec * SDLCALL tSDL_LoadWAV_RW(SDL_RWops * src,
                                                      int freesrc,
                                                      SDL_AudioSpec * spec,
                                                      Uint8 ** audio_buf,
                                                      Uint32 * audio_len);

#define SDL_LoadWAV(file, spec, audio_buf, audio_len) \
    SDL_LoadWAV_RW(SDL_RWFromFile(file, "rb"),1, spec,audio_buf,audio_len)

typedef void SDLCALL tSDL_FreeWAV(Uint8 * audio_buf);

typedef int SDLCALL tSDL_BuildAudioCVT(SDL_AudioCVT * cvt,
                                              SDL_AudioFormat src_format,
                                              Uint8 src_channels,
                                              int src_rate,
                                              SDL_AudioFormat dst_format,
                                              Uint8 dst_channels,
                                              int dst_rate);

typedef int SDLCALL tSDL_ConvertAudio(SDL_AudioCVT * cvt);

#define SDL_MIX_MAXVOLUME 128

typedef void SDLCALL tSDL_MixAudio(Uint8 * dst, const Uint8 * src,
                                          Uint32 len, int volume);

typedef void SDLCALL tSDL_MixAudioFormat(Uint8 * dst,
                                                const Uint8 * src,
                                                SDL_AudioFormat format,
                                                Uint32 len, int volume);

typedef void SDLCALL tSDL_LockAudio(void);
typedef void SDLCALL tSDL_LockAudioDevice(SDL_AudioDeviceID dev);
typedef void SDLCALL tSDL_UnlockAudio(void);
typedef void SDLCALL tSDL_UnlockAudioDevice(SDL_AudioDeviceID dev);

typedef void SDLCALL tSDL_CloseAudio(void);
typedef void SDLCALL tSDL_CloseAudioDevice(SDL_AudioDeviceID dev);

extern tSDL_GetNumAudioDrivers *SDL_GetNumAudioDrivers;
extern tSDL_GetAudioDriver *SDL_GetAudioDriver;
extern tSDL_AudioInit *SDL_AudioInit;
extern tSDL_AudioQuit *SDL_AudioQuit;
extern tSDL_GetCurrentAudioDriver *SDL_GetCurrentAudioDriver;
extern tSDL_OpenAudio *SDL_OpenAudio;
extern tSDL_GetNumAudioDevices *SDL_GetNumAudioDevices;
extern tSDL_GetAudioDeviceName *SDL_GetAudioDeviceName;
extern tSDL_OpenAudioDevice *SDL_OpenAudioDevice;
extern tSDL_GetAudioStatus *SDL_GetAudioStatus;
extern tSDL_PauseAudio *SDL_PauseAudio;
extern tSDL_PauseAudioDevice *SDL_PauseAudioDevice;
extern tSDL_LoadWAV_RW *SDL_LoadWAV_RW;
extern tSDL_FreeWAV *SDL_FreeWAV;
extern tSDL_BuildAudioCVT *SDL_BuildAudioCVT;
extern tSDL_ConvertAudio *SDL_ConvertAudio;
extern tSDL_MixAudio *SDL_MixAudio;
extern tSDL_MixAudioFormat *SDL_MixAudioFormat;
extern tSDL_LockAudio *SDL_LockAudio;
extern tSDL_LockAudioDevice *SDL_LockAudioDevice;
extern tSDL_UnlockAudio *SDL_UnlockAudio;
extern tSDL_UnlockAudioDevice *SDL_UnlockAudioDevice;
extern tSDL_CloseAudio *SDL_CloseAudio;
extern tSDL_CloseAudioDevice *SDL_CloseAudioDevice;

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

