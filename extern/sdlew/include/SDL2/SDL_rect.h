
#ifndef _SDL_rect_h
#define _SDL_rect_h

#include "SDL_stdinc.h"
#include "SDL_error.h"
#include "SDL_pixels.h"
#include "SDL_rwops.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    int x;
    int y;
} SDL_Point;

typedef struct SDL_Rect
{
    int x, y;
    int w, h;
} SDL_Rect;

SDL_FORCE_INLINE SDL_bool SDL_RectEmpty(const SDL_Rect *r)
{
    return ((!r) || (r->w <= 0) || (r->h <= 0)) ? SDL_TRUE : SDL_FALSE;
}

SDL_FORCE_INLINE SDL_bool SDL_RectEquals(const SDL_Rect *a, const SDL_Rect *b)
{
    return (a && b && (a->x == b->x) && (a->y == b->y) &&
            (a->w == b->w) && (a->h == b->h)) ? SDL_TRUE : SDL_FALSE;
}

typedef SDL_bool SDLCALL tSDL_HasIntersection(const SDL_Rect * A,
                                                     const SDL_Rect * B);

typedef SDL_bool SDLCALL tSDL_IntersectRect(const SDL_Rect * A,
                                                   const SDL_Rect * B,
                                                   SDL_Rect * result);

typedef void SDLCALL tSDL_UnionRect(const SDL_Rect * A,
                                           const SDL_Rect * B,
                                           SDL_Rect * result);

typedef SDL_bool SDLCALL tSDL_EnclosePoints(const SDL_Point * points,
                                                   int count,
                                                   const SDL_Rect * clip,
                                                   SDL_Rect * result);

typedef SDL_bool SDLCALL tSDL_IntersectRectAndLine(const SDL_Rect *
                                                          rect, int *X1,
                                                          int *Y1, int *X2,
                                                          int *Y2);

extern tSDL_HasIntersection *SDL_HasIntersection;
extern tSDL_IntersectRect *SDL_IntersectRect;
extern tSDL_UnionRect *SDL_UnionRect;
extern tSDL_EnclosePoints *SDL_EnclosePoints;
extern tSDL_IntersectRectAndLine *SDL_IntersectRectAndLine;

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

