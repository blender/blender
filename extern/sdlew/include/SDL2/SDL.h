
#ifndef _SDL_H
#define _SDL_H

#include "SDL_main.h"
#include "SDL_stdinc.h"
#include "SDL_assert.h"
#include "SDL_atomic.h"
#include "SDL_audio.h"
#include "SDL_clipboard.h"
#include "SDL_cpuinfo.h"
#include "SDL_endian.h"
#include "SDL_error.h"
#include "SDL_events.h"
#include "SDL_joystick.h"
#include "SDL_gamecontroller.h"
#include "SDL_haptic.h"
#include "SDL_hints.h"
#include "SDL_loadso.h"
#include "SDL_log.h"
#include "SDL_messagebox.h"
#include "SDL_mutex.h"
#include "SDL_power.h"
#include "SDL_render.h"
#include "SDL_rwops.h"
#include "SDL_system.h"
#include "SDL_thread.h"
#include "SDL_timer.h"
#include "SDL_version.h"
#include "SDL_video.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SDL_INIT_TIMER          0x00000001
#define SDL_INIT_AUDIO          0x00000010
#define SDL_INIT_VIDEO          0x00000020
#define SDL_INIT_JOYSTICK       0x00000200
#define SDL_INIT_HAPTIC         0x00001000
#define SDL_INIT_GAMECONTROLLER 0x00002000
#define SDL_INIT_EVENTS         0x00004000
#define SDL_INIT_NOPARACHUTE    0x00100000
#define SDL_INIT_EVERYTHING ( \
                SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS | \
                SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_GAMECONTROLLER \
            )

typedef int SDLCALL tSDL_Init(Uint32 flags);

typedef int SDLCALL tSDL_InitSubSystem(Uint32 flags);

typedef void SDLCALL tSDL_QuitSubSystem(Uint32 flags);

typedef Uint32 SDLCALL tSDL_WasInit(Uint32 flags);

typedef void SDLCALL tSDL_Quit(void);

extern tSDL_Init *SDL_Init;
extern tSDL_InitSubSystem *SDL_InitSubSystem;
extern tSDL_QuitSubSystem *SDL_QuitSubSystem;
extern tSDL_WasInit *SDL_WasInit;
extern tSDL_Quit *SDL_Quit;

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

