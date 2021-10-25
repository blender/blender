
#ifndef _SDL_power_h
#define _SDL_power_h

#include "SDL_stdinc.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    SDL_POWERSTATE_UNKNOWN,
    SDL_POWERSTATE_ON_BATTERY,
    SDL_POWERSTATE_NO_BATTERY,
    SDL_POWERSTATE_CHARGING,
    SDL_POWERSTATE_CHARGED
} SDL_PowerState;

typedef SDL_PowerState SDLCALL tSDL_GetPowerInfo(int *secs, int *pct);

extern tSDL_GetPowerInfo *SDL_GetPowerInfo;

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

