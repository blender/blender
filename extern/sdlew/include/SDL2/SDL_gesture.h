
#ifndef _SDL_gesture_h
#define _SDL_gesture_h

#include "SDL_stdinc.h"
#include "SDL_error.h"
#include "SDL_video.h"

#include "SDL_touch.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Sint64 SDL_GestureID;

typedef int SDLCALL tSDL_RecordGesture(SDL_TouchID touchId);

typedef int SDLCALL tSDL_SaveAllDollarTemplates(SDL_RWops *src);

typedef int SDLCALL tSDL_SaveDollarTemplate(SDL_GestureID gestureId,SDL_RWops *src);

typedef int SDLCALL tSDL_LoadDollarTemplates(SDL_TouchID touchId, SDL_RWops *src);

extern tSDL_RecordGesture *SDL_RecordGesture;
extern tSDL_SaveAllDollarTemplates *SDL_SaveAllDollarTemplates;
extern tSDL_SaveDollarTemplate *SDL_SaveDollarTemplate;
extern tSDL_LoadDollarTemplates *SDL_LoadDollarTemplates;

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

