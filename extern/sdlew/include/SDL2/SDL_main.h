
#ifndef _SDL_main_h
#define _SDL_main_h

#include "SDL_stdinc.h"

#ifndef SDL_MAIN_HANDLED
#if defined(__WIN32__)

#define SDL_MAIN_AVAILABLE

#elif defined(__IPHONEOS__)

#define SDL_MAIN_NEEDED

#elif defined(__ANDROID__)

#define SDL_MAIN_NEEDED

#endif
#endif

#ifdef __cplusplus
#define C_LINKAGE   "C"
#else
#define C_LINKAGE
#endif

#if defined(SDL_MAIN_NEEDED) || defined(SDL_MAIN_AVAILABLE)
#define main    SDL_main
#endif

extern C_LINKAGE int SDL_main(int argc, char *argv[]);

#include "begin_code.h"
#ifdef __cplusplus
extern "C" {
#endif

extern DECLSPEC void SDL_SetMainReady(void);

#ifdef __WIN32__

typedef int SDLCALL tSDL_RegisterApp(char *name, Uint32 style,
                                            void *hInst);
typedef void SDLCALL tSDL_UnregisterApp(void);

#endif

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

