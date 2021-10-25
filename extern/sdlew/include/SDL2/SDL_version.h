
#ifndef _SDL_version_h
#define _SDL_version_h

#include "SDL_stdinc.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SDL_version
{
    Uint8 major;
    Uint8 minor;
    Uint8 patch;
} SDL_version;

#define SDL_MAJOR_VERSION   2
#define SDL_MINOR_VERSION   0
#define SDL_PATCHLEVEL      0

#define SDL_VERSION(x)                          \
{                                   \
    (x)->major = SDL_MAJOR_VERSION;                 \
    (x)->minor = SDL_MINOR_VERSION;                 \
    (x)->patch = SDL_PATCHLEVEL;                    \
}

#define SDL_VERSIONNUM(X, Y, Z)                     \
    ((X)*1000 + (Y)*100 + (Z))

#define SDL_COMPILEDVERSION \
    SDL_VERSIONNUM(SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL)

#define SDL_VERSION_ATLEAST(X, Y, Z) \
    (SDL_COMPILEDVERSION >= SDL_VERSIONNUM(X, Y, Z))

typedef void SDLCALL tSDL_GetVersion(SDL_version * ver);

typedef const char * SDLCALL tSDL_GetRevision(void);

typedef int SDLCALL tSDL_GetRevisionNumber(void);

extern tSDL_GetVersion *SDL_GetVersion;
extern tSDL_GetRevision *SDL_GetRevision;
extern tSDL_GetRevisionNumber *SDL_GetRevisionNumber;

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

