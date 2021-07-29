
#ifndef _SDL_touch_h
#define _SDL_touch_h

#include "SDL_stdinc.h"
#include "SDL_error.h"
#include "SDL_video.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Sint64 SDL_TouchID;
typedef Sint64 SDL_FingerID;

typedef struct SDL_Finger
{
    SDL_FingerID id;
    float x;
    float y;
    float pressure;
} SDL_Finger;

#define SDL_TOUCH_MOUSEID ((Uint32)-1)

typedef int SDLCALL tSDL_GetNumTouchDevices(void);

typedef SDL_TouchID SDLCALL tSDL_GetTouchDevice(int index);

typedef int SDLCALL tSDL_GetNumTouchFingers(SDL_TouchID touchID);

typedef SDL_Finger * SDLCALL tSDL_GetTouchFinger(SDL_TouchID touchID, int index);

extern tSDL_GetNumTouchDevices *SDL_GetNumTouchDevices;
extern tSDL_GetTouchDevice *SDL_GetTouchDevice;
extern tSDL_GetNumTouchFingers *SDL_GetNumTouchFingers;
extern tSDL_GetTouchFinger *SDL_GetTouchFinger;

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

