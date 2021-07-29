
#ifndef _SDL_cpuinfo_h
#define _SDL_cpuinfo_h

#include "SDL_stdinc.h"

#if defined(_MSC_VER) && (_MSC_VER >= 1500)
#include <intrin.h>
#ifndef _WIN64
#define __MMX__
#define __3dNOW__
#endif
#define __SSE__
#define __SSE2__
#elif defined(__MINGW64_VERSION_MAJOR)
#include <intrin.h>
#else
#ifdef __ALTIVEC__
#if HAVE_ALTIVEC_H && !defined(__APPLE_ALTIVEC__)
#include <altivec.h>
#undef pixel
#endif
#endif
#ifdef __MMX__
#include <mmintrin.h>
#endif
#ifdef __3dNOW__
#include <mm3dnow.h>
#endif
#ifdef __SSE__
#include <xmmintrin.h>
#endif
#ifdef __SSE2__
#include <emmintrin.h>
#endif
#endif

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SDL_CACHELINE_SIZE  128

typedef int SDLCALL tSDL_GetCPUCount(void);

typedef int SDLCALL tSDL_GetCPUCacheLineSize(void);

typedef SDL_bool SDLCALL tSDL_HasRDTSC(void);

typedef SDL_bool SDLCALL tSDL_HasAltiVec(void);

typedef SDL_bool SDLCALL tSDL_HasMMX(void);

typedef SDL_bool SDLCALL tSDL_Has3DNow(void);

typedef SDL_bool SDLCALL tSDL_HasSSE(void);

typedef SDL_bool SDLCALL tSDL_HasSSE2(void);

typedef SDL_bool SDLCALL tSDL_HasSSE3(void);

typedef SDL_bool SDLCALL tSDL_HasSSE41(void);

typedef SDL_bool SDLCALL tSDL_HasSSE42(void);

extern tSDL_GetCPUCount *SDL_GetCPUCount;
extern tSDL_GetCPUCacheLineSize *SDL_GetCPUCacheLineSize;
extern tSDL_HasRDTSC *SDL_HasRDTSC;
extern tSDL_HasAltiVec *SDL_HasAltiVec;
extern tSDL_HasMMX *SDL_HasMMX;
extern tSDL_Has3DNow *SDL_Has3DNow;
extern tSDL_HasSSE *SDL_HasSSE;
extern tSDL_HasSSE2 *SDL_HasSSE2;
extern tSDL_HasSSE3 *SDL_HasSSE3;
extern tSDL_HasSSE41 *SDL_HasSSE41;
extern tSDL_HasSSE42 *SDL_HasSSE42;

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

