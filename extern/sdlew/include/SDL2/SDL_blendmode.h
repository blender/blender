
#ifndef _SDL_blendmode_h
#define _SDL_blendmode_h

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    SDL_BLENDMODE_NONE = 0x00000000,
    SDL_BLENDMODE_BLEND = 0x00000001,
    SDL_BLENDMODE_ADD = 0x00000002,
    SDL_BLENDMODE_MOD = 0x00000004
} SDL_BlendMode;

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

