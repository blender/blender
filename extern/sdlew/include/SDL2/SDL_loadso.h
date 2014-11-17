
#ifndef _SDL_loadso_h
#define _SDL_loadso_h

#include "SDL_stdinc.h"
#include "SDL_error.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void * SDLCALL tSDL_LoadObject(const char *sofile);

typedef void * SDLCALL tSDL_LoadFunction(void *handle,
                                               const char *name);

typedef void SDLCALL tSDL_UnloadObject(void *handle);

extern tSDL_LoadObject *SDL_LoadObject;
extern tSDL_LoadFunction *SDL_LoadFunction;
extern tSDL_UnloadObject *SDL_UnloadObject;

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

