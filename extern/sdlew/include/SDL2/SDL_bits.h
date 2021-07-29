
#ifndef _SDL_bits_h
#define _SDL_bits_h

#include "SDL_stdinc.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

SDL_FORCE_INLINE int
SDL_MostSignificantBitIndex32(Uint32 x)
{
#if defined(__GNUC__) && __GNUC__ >= 4

    if (x == 0) {
        return -1;
    }
    return 31 - __builtin_clz(x);
#else

    const Uint32 b[] = {0x2, 0xC, 0xF0, 0xFF00, 0xFFFF0000};
    const int    S[] = {1, 2, 4, 8, 16};

    int msbIndex = 0;
    int i;

    if (x == 0) {
        return -1;
    }

    for (i = 4; i >= 0; i--)
    {
        if (x & b[i])
        {
            x >>= S[i];
            msbIndex |= S[i];
        }
    }

    return msbIndex;
#endif
}

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

