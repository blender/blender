
#ifndef _SDL_hints_h
#define _SDL_hints_h

#include "SDL_stdinc.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SDL_HINT_FRAMEBUFFER_ACCELERATION   "SDL_FRAMEBUFFER_ACCELERATION"

#define SDL_HINT_RENDER_DRIVER              "SDL_RENDER_DRIVER"

#define SDL_HINT_RENDER_OPENGL_SHADERS      "SDL_RENDER_OPENGL_SHADERS"

#define SDL_HINT_RENDER_SCALE_QUALITY       "SDL_RENDER_SCALE_QUALITY"

#define SDL_HINT_RENDER_VSYNC               "SDL_RENDER_VSYNC"

#define SDL_HINT_VIDEO_X11_XVIDMODE         "SDL_VIDEO_X11_XVIDMODE"

#define SDL_HINT_VIDEO_X11_XINERAMA         "SDL_VIDEO_X11_XINERAMA"

#define SDL_HINT_VIDEO_X11_XRANDR           "SDL_VIDEO_X11_XRANDR"

#define SDL_HINT_GRAB_KEYBOARD              "SDL_GRAB_KEYBOARD"

#define SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS   "SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS"

#define SDL_HINT_IDLE_TIMER_DISABLED "SDL_IOS_IDLE_TIMER_DISABLED"

#define SDL_HINT_ORIENTATIONS "SDL_IOS_ORIENTATIONS"

#define SDL_HINT_XINPUT_ENABLED "SDL_XINPUT_ENABLED"

#define SDL_HINT_GAMECONTROLLERCONFIG "SDL_GAMECONTROLLERCONFIG"

#define SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS "SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS"

#define SDL_HINT_ALLOW_TOPMOST "SDL_ALLOW_TOPMOST"

#define SDL_HINT_TIMER_RESOLUTION "SDL_TIMER_RESOLUTION"

typedef enum
{
    SDL_HINT_DEFAULT,
    SDL_HINT_NORMAL,
    SDL_HINT_OVERRIDE
} SDL_HintPriority;

typedef SDL_bool SDLCALL tSDL_SetHintWithPriority(const char *name,
                                                         const char *value,
                                                         SDL_HintPriority priority);

typedef SDL_bool SDLCALL tSDL_SetHint(const char *name,
                                             const char *value);

typedef const char * SDLCALL tSDL_GetHint(const char *name);

typedef void (*SDL_HintCallback)(void *userdata, const char *name, const char *oldValue, const char *newValue);
typedef void SDLCALL tSDL_AddHintCallback(const char *name,
                                                 SDL_HintCallback callback,
                                                 void *userdata);

typedef void SDLCALL tSDL_DelHintCallback(const char *name,
                                                 SDL_HintCallback callback,
                                                 void *userdata);

typedef void SDLCALL tSDL_ClearHints(void);

extern tSDL_SetHintWithPriority *SDL_SetHintWithPriority;
extern tSDL_SetHint *SDL_SetHint;
extern tSDL_GetHint *SDL_GetHint;
extern tSDL_AddHintCallback *SDL_AddHintCallback;
extern tSDL_DelHintCallback *SDL_DelHintCallback;
extern tSDL_ClearHints *SDL_ClearHints;

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

