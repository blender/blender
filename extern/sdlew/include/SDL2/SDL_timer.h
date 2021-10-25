
#ifndef _SDL_timer_h
#define _SDL_timer_h

#include "SDL_stdinc.h"
#include "SDL_error.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Uint32 SDLCALL tSDL_GetTicks(void);

typedef Uint64 SDLCALL tSDL_GetPerformanceCounter(void);

typedef Uint64 SDLCALL tSDL_GetPerformanceFrequency(void);

typedef void SDLCALL tSDL_Delay(Uint32 ms);

typedef Uint32 (SDLCALL * SDL_TimerCallback) (Uint32 interval, void *param);

typedef int SDL_TimerID;

typedef SDL_TimerID SDLCALL tSDL_AddTimer(Uint32 interval,
                                                 SDL_TimerCallback callback,
                                                 void *param);

typedef SDL_bool SDLCALL tSDL_RemoveTimer(SDL_TimerID id);

extern tSDL_GetTicks *SDL_GetTicks;
extern tSDL_GetPerformanceCounter *SDL_GetPerformanceCounter;
extern tSDL_GetPerformanceFrequency *SDL_GetPerformanceFrequency;
extern tSDL_Delay *SDL_Delay;
extern tSDL_AddTimer *SDL_AddTimer;
extern tSDL_RemoveTimer *SDL_RemoveTimer;

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

