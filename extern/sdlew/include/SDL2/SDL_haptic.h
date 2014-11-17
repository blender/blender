
#ifndef _SDL_haptic_h
#define _SDL_haptic_h

#include "SDL_stdinc.h"
#include "SDL_error.h"
#include "SDL_joystick.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

struct _SDL_Haptic;
typedef struct _SDL_Haptic SDL_Haptic;

#define SDL_HAPTIC_CONSTANT   (1<<0)

#define SDL_HAPTIC_SINE       (1<<1)

#define SDL_HAPTIC_LEFTRIGHT     (1<<2)

#define SDL_HAPTIC_TRIANGLE   (1<<3)

#define SDL_HAPTIC_SAWTOOTHUP (1<<4)

#define SDL_HAPTIC_SAWTOOTHDOWN (1<<5)

#define SDL_HAPTIC_RAMP       (1<<6)

#define SDL_HAPTIC_SPRING     (1<<7)

#define SDL_HAPTIC_DAMPER     (1<<8)

#define SDL_HAPTIC_INERTIA    (1<<9)

#define SDL_HAPTIC_FRICTION   (1<<10)

#define SDL_HAPTIC_CUSTOM     (1<<11)

#define SDL_HAPTIC_GAIN       (1<<12)

#define SDL_HAPTIC_AUTOCENTER (1<<13)

#define SDL_HAPTIC_STATUS     (1<<14)

#define SDL_HAPTIC_PAUSE      (1<<15)

#define SDL_HAPTIC_POLAR      0

#define SDL_HAPTIC_CARTESIAN  1

#define SDL_HAPTIC_SPHERICAL  2

#define SDL_HAPTIC_INFINITY   4294967295U

typedef struct SDL_HapticDirection
{
    Uint8 type;
    Sint32 dir[3];
} SDL_HapticDirection;

typedef struct SDL_HapticConstant
{

    Uint16 type;
    SDL_HapticDirection direction;

    Uint32 length;
    Uint16 delay;

    Uint16 button;
    Uint16 interval;

    Sint16 level;

    Uint16 attack_length;
    Uint16 attack_level;
    Uint16 fade_length;
    Uint16 fade_level;
} SDL_HapticConstant;

typedef struct SDL_HapticPeriodic
{

    Uint16 type;
    SDL_HapticDirection direction;

    Uint32 length;
    Uint16 delay;

    Uint16 button;
    Uint16 interval;

    Uint16 period;
    Sint16 magnitude;
    Sint16 offset;
    Uint16 phase;

    Uint16 attack_length;
    Uint16 attack_level;
    Uint16 fade_length;
    Uint16 fade_level;
} SDL_HapticPeriodic;

typedef struct SDL_HapticCondition
{

    Uint16 type;
    SDL_HapticDirection direction;

    Uint32 length;
    Uint16 delay;

    Uint16 button;
    Uint16 interval;

    Uint16 right_sat[3];
    Uint16 left_sat[3];
    Sint16 right_coeff[3];
    Sint16 left_coeff[3];
    Uint16 deadband[3];
    Sint16 center[3];
} SDL_HapticCondition;

typedef struct SDL_HapticRamp
{

    Uint16 type;
    SDL_HapticDirection direction;

    Uint32 length;
    Uint16 delay;

    Uint16 button;
    Uint16 interval;

    Sint16 start;
    Sint16 end;

    Uint16 attack_length;
    Uint16 attack_level;
    Uint16 fade_length;
    Uint16 fade_level;
} SDL_HapticRamp;

typedef struct SDL_HapticLeftRight
{

    Uint16 type;

    Uint32 length;

    Uint16 large_magnitude;
    Uint16 small_magnitude;
} SDL_HapticLeftRight;

typedef struct SDL_HapticCustom
{

    Uint16 type;
    SDL_HapticDirection direction;

    Uint32 length;
    Uint16 delay;

    Uint16 button;
    Uint16 interval;

    Uint8 channels;
    Uint16 period;
    Uint16 samples;
    Uint16 *data;

    Uint16 attack_length;
    Uint16 attack_level;
    Uint16 fade_length;
    Uint16 fade_level;
} SDL_HapticCustom;

typedef union SDL_HapticEffect
{

    Uint16 type;
    SDL_HapticConstant constant;
    SDL_HapticPeriodic periodic;
    SDL_HapticCondition condition;
    SDL_HapticRamp ramp;
    SDL_HapticLeftRight leftright;
    SDL_HapticCustom custom;
} SDL_HapticEffect;

typedef int SDLCALL tSDL_NumHaptics(void);

typedef const char * SDLCALL tSDL_HapticName(int device_index);

typedef SDL_Haptic * SDLCALL tSDL_HapticOpen(int device_index);

typedef int SDLCALL tSDL_HapticOpened(int device_index);

typedef int SDLCALL tSDL_HapticIndex(SDL_Haptic * haptic);

typedef int SDLCALL tSDL_MouseIsHaptic(void);

typedef SDL_Haptic * SDLCALL tSDL_HapticOpenFromMouse(void);

typedef int SDLCALL tSDL_JoystickIsHaptic(SDL_Joystick * joystick);

typedef SDL_Haptic * SDLCALL tSDL_HapticOpenFromJoystick(SDL_Joystick *
                                                               joystick);

typedef void SDLCALL tSDL_HapticClose(SDL_Haptic * haptic);

typedef int SDLCALL tSDL_HapticNumEffects(SDL_Haptic * haptic);

typedef int SDLCALL tSDL_HapticNumEffectsPlaying(SDL_Haptic * haptic);

extern DECLSPEC unsigned int SDLCALL SDL_HapticQuery(SDL_Haptic * haptic);

typedef int SDLCALL tSDL_HapticNumAxes(SDL_Haptic * haptic);

typedef int SDLCALL tSDL_HapticEffectSupported(SDL_Haptic * haptic,
                                                      SDL_HapticEffect *
                                                      effect);

typedef int SDLCALL tSDL_HapticNewEffect(SDL_Haptic * haptic,
                                                SDL_HapticEffect * effect);

typedef int SDLCALL tSDL_HapticUpdateEffect(SDL_Haptic * haptic,
                                                   int effect,
                                                   SDL_HapticEffect * data);

typedef int SDLCALL tSDL_HapticRunEffect(SDL_Haptic * haptic,
                                                int effect,
                                                Uint32 iterations);

typedef int SDLCALL tSDL_HapticStopEffect(SDL_Haptic * haptic,
                                                 int effect);

typedef void SDLCALL tSDL_HapticDestroyEffect(SDL_Haptic * haptic,
                                                     int effect);

typedef int SDLCALL tSDL_HapticGetEffectStatus(SDL_Haptic * haptic,
                                                      int effect);

typedef int SDLCALL tSDL_HapticSetGain(SDL_Haptic * haptic, int gain);

typedef int SDLCALL tSDL_HapticSetAutocenter(SDL_Haptic * haptic,
                                                    int autocenter);

typedef int SDLCALL tSDL_HapticPause(SDL_Haptic * haptic);

typedef int SDLCALL tSDL_HapticUnpause(SDL_Haptic * haptic);

typedef int SDLCALL tSDL_HapticStopAll(SDL_Haptic * haptic);

typedef int SDLCALL tSDL_HapticRumbleSupported(SDL_Haptic * haptic);

typedef int SDLCALL tSDL_HapticRumbleInit(SDL_Haptic * haptic);

typedef int SDLCALL tSDL_HapticRumblePlay(SDL_Haptic * haptic, float strength, Uint32 length );

typedef int SDLCALL tSDL_HapticRumbleStop(SDL_Haptic * haptic);

extern tSDL_NumHaptics *SDL_NumHaptics;
extern tSDL_HapticName *SDL_HapticName;
extern tSDL_HapticOpen *SDL_HapticOpen;
extern tSDL_HapticOpened *SDL_HapticOpened;
extern tSDL_HapticIndex *SDL_HapticIndex;
extern tSDL_MouseIsHaptic *SDL_MouseIsHaptic;
extern tSDL_HapticOpenFromMouse *SDL_HapticOpenFromMouse;
extern tSDL_JoystickIsHaptic *SDL_JoystickIsHaptic;
extern tSDL_HapticOpenFromJoystick *SDL_HapticOpenFromJoystick;
extern tSDL_HapticClose *SDL_HapticClose;
extern tSDL_HapticNumEffects *SDL_HapticNumEffects;
extern tSDL_HapticNumEffectsPlaying *SDL_HapticNumEffectsPlaying;
extern tSDL_HapticNumAxes *SDL_HapticNumAxes;
extern tSDL_HapticEffectSupported *SDL_HapticEffectSupported;
extern tSDL_HapticNewEffect *SDL_HapticNewEffect;
extern tSDL_HapticUpdateEffect *SDL_HapticUpdateEffect;
extern tSDL_HapticRunEffect *SDL_HapticRunEffect;
extern tSDL_HapticStopEffect *SDL_HapticStopEffect;
extern tSDL_HapticDestroyEffect *SDL_HapticDestroyEffect;
extern tSDL_HapticGetEffectStatus *SDL_HapticGetEffectStatus;
extern tSDL_HapticSetGain *SDL_HapticSetGain;
extern tSDL_HapticSetAutocenter *SDL_HapticSetAutocenter;
extern tSDL_HapticPause *SDL_HapticPause;
extern tSDL_HapticUnpause *SDL_HapticUnpause;
extern tSDL_HapticStopAll *SDL_HapticStopAll;
extern tSDL_HapticRumbleSupported *SDL_HapticRumbleSupported;
extern tSDL_HapticRumbleInit *SDL_HapticRumbleInit;
extern tSDL_HapticRumblePlay *SDL_HapticRumblePlay;
extern tSDL_HapticRumbleStop *SDL_HapticRumbleStop;

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

