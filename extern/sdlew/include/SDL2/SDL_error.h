
#ifndef _SDL_error_h
#define _SDL_error_h

#include "SDL_stdinc.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int SDLCALL tSDL_SetError(const char *fmt, ...);
typedef const char * SDLCALL tSDL_GetError(void);
typedef void SDLCALL tSDL_ClearError(void);

#define SDL_OutOfMemory()   SDL_Error(SDL_ENOMEM)
#define SDL_Unsupported()   SDL_Error(SDL_UNSUPPORTED)
#define SDL_InvalidParamError(param)    SDL_SetError("Parameter '%s' is invalid", (param))
typedef enum
{
    SDL_ENOMEM,
    SDL_EFREAD,
    SDL_EFWRITE,
    SDL_EFSEEK,
    SDL_UNSUPPORTED,
    SDL_LASTERROR
} SDL_errorcode;

typedef int SDLCALL tSDL_Error(SDL_errorcode code);

extern tSDL_SetError *SDL_SetError;
extern tSDL_GetError *SDL_GetError;
extern tSDL_ClearError *SDL_ClearError;
extern tSDL_Error *SDL_Error;

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

