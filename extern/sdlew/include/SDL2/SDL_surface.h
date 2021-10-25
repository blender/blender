
#ifndef _SDL_surface_h
#define _SDL_surface_h

#include "SDL_stdinc.h"
#include "SDL_pixels.h"
#include "SDL_rect.h"
#include "SDL_blendmode.h"
#include "SDL_rwops.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SDL_SWSURFACE       0
#define SDL_PREALLOC        0x00000001
#define SDL_RLEACCEL        0x00000002
#define SDL_DONTFREE        0x00000004

#define SDL_MUSTLOCK(S) (((S)->flags & SDL_RLEACCEL) != 0)

typedef struct SDL_Surface
{
    Uint32 flags;
    SDL_PixelFormat *format;
    int w, h;
    int pitch;
    void *pixels;

    void *userdata;

    int locked;
    void *lock_data;

    SDL_Rect clip_rect;

    struct SDL_BlitMap *map;

    int refcount;
} SDL_Surface;

typedef int (*SDL_blit) (struct SDL_Surface * src, SDL_Rect * srcrect,
                         struct SDL_Surface * dst, SDL_Rect * dstrect);

typedef SDL_Surface * SDLCALL tSDL_CreateRGBSurface
    (Uint32 flags, int width, int height, int depth,
     Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);
typedef SDL_Surface * SDLCALL tSDL_CreateRGBSurfaceFrom(void *pixels,
                                                              int width,
                                                              int height,
                                                              int depth,
                                                              int pitch,
                                                              Uint32 Rmask,
                                                              Uint32 Gmask,
                                                              Uint32 Bmask,
                                                              Uint32 Amask);
typedef void SDLCALL tSDL_FreeSurface(SDL_Surface * surface);

typedef int SDLCALL tSDL_SetSurfacePalette(SDL_Surface * surface,
                                                  SDL_Palette * palette);

typedef int SDLCALL tSDL_LockSurface(SDL_Surface * surface);

typedef void SDLCALL tSDL_UnlockSurface(SDL_Surface * surface);

typedef SDL_Surface * SDLCALL tSDL_LoadBMP_RW(SDL_RWops * src,
                                                    int freesrc);

#define SDL_LoadBMP(file)   SDL_LoadBMP_RW(SDL_RWFromFile(file, "rb"), 1)

typedef int SDLCALL tSDL_SaveBMP_RW
    (SDL_Surface * surface, SDL_RWops * dst, int freedst);

#define SDL_SaveBMP(surface, file) \
        SDL_SaveBMP_RW(surface, SDL_RWFromFile(file, "wb"), 1)

typedef int SDLCALL tSDL_SetSurfaceRLE(SDL_Surface * surface,
                                              int flag);

typedef int SDLCALL tSDL_SetColorKey(SDL_Surface * surface,
                                            int flag, Uint32 key);

typedef int SDLCALL tSDL_GetColorKey(SDL_Surface * surface,
                                            Uint32 * key);

typedef int SDLCALL tSDL_SetSurfaceColorMod(SDL_Surface * surface,
                                                   Uint8 r, Uint8 g, Uint8 b);

typedef int SDLCALL tSDL_GetSurfaceColorMod(SDL_Surface * surface,
                                                   Uint8 * r, Uint8 * g,
                                                   Uint8 * b);

typedef int SDLCALL tSDL_SetSurfaceAlphaMod(SDL_Surface * surface,
                                                   Uint8 alpha);

typedef int SDLCALL tSDL_GetSurfaceAlphaMod(SDL_Surface * surface,
                                                   Uint8 * alpha);

typedef int SDLCALL tSDL_SetSurfaceBlendMode(SDL_Surface * surface,
                                                    SDL_BlendMode blendMode);

typedef int SDLCALL tSDL_GetSurfaceBlendMode(SDL_Surface * surface,
                                                    SDL_BlendMode *blendMode);

typedef SDL_bool SDLCALL tSDL_SetClipRect(SDL_Surface * surface,
                                                 const SDL_Rect * rect);

typedef void SDLCALL tSDL_GetClipRect(SDL_Surface * surface,
                                             SDL_Rect * rect);

typedef SDL_Surface * SDLCALL tSDL_ConvertSurface
    (SDL_Surface * src, SDL_PixelFormat * fmt, Uint32 flags);
typedef SDL_Surface * SDLCALL tSDL_ConvertSurfaceFormat
    (SDL_Surface * src, Uint32 pixel_format, Uint32 flags);

typedef int SDLCALL tSDL_ConvertPixels(int width, int height,
                                              Uint32 src_format,
                                              const void * src, int src_pitch,
                                              Uint32 dst_format,
                                              void * dst, int dst_pitch);

typedef int SDLCALL tSDL_FillRect
    (SDL_Surface * dst, const SDL_Rect * rect, Uint32 color);
typedef int SDLCALL tSDL_FillRects
    (SDL_Surface * dst, const SDL_Rect * rects, int count, Uint32 color);

#define SDL_BlitSurface SDL_UpperBlit

typedef int SDLCALL tSDL_UpperBlit
    (SDL_Surface * src, const SDL_Rect * srcrect,
     SDL_Surface * dst, SDL_Rect * dstrect);

typedef int SDLCALL tSDL_LowerBlit
    (SDL_Surface * src, SDL_Rect * srcrect,
     SDL_Surface * dst, SDL_Rect * dstrect);

typedef int SDLCALL tSDL_SoftStretch(SDL_Surface * src,
                                            const SDL_Rect * srcrect,
                                            SDL_Surface * dst,
                                            const SDL_Rect * dstrect);

#define SDL_BlitScaled SDL_UpperBlitScaled

typedef int SDLCALL tSDL_UpperBlitScaled
    (SDL_Surface * src, const SDL_Rect * srcrect,
    SDL_Surface * dst, SDL_Rect * dstrect);

typedef int SDLCALL tSDL_LowerBlitScaled
    (SDL_Surface * src, SDL_Rect * srcrect,
    SDL_Surface * dst, SDL_Rect * dstrect);

extern tSDL_CreateRGBSurface *SDL_CreateRGBSurface;
extern tSDL_CreateRGBSurfaceFrom *SDL_CreateRGBSurfaceFrom;
extern tSDL_FreeSurface *SDL_FreeSurface;
extern tSDL_SetSurfacePalette *SDL_SetSurfacePalette;
extern tSDL_LockSurface *SDL_LockSurface;
extern tSDL_UnlockSurface *SDL_UnlockSurface;
extern tSDL_LoadBMP_RW *SDL_LoadBMP_RW;
extern tSDL_SaveBMP_RW *SDL_SaveBMP_RW;
extern tSDL_SetSurfaceRLE *SDL_SetSurfaceRLE;
extern tSDL_SetColorKey *SDL_SetColorKey;
extern tSDL_GetColorKey *SDL_GetColorKey;
extern tSDL_SetSurfaceColorMod *SDL_SetSurfaceColorMod;
extern tSDL_GetSurfaceColorMod *SDL_GetSurfaceColorMod;
extern tSDL_SetSurfaceAlphaMod *SDL_SetSurfaceAlphaMod;
extern tSDL_GetSurfaceAlphaMod *SDL_GetSurfaceAlphaMod;
extern tSDL_SetSurfaceBlendMode *SDL_SetSurfaceBlendMode;
extern tSDL_GetSurfaceBlendMode *SDL_GetSurfaceBlendMode;
extern tSDL_SetClipRect *SDL_SetClipRect;
extern tSDL_GetClipRect *SDL_GetClipRect;
extern tSDL_ConvertSurface *SDL_ConvertSurface;
extern tSDL_ConvertSurfaceFormat *SDL_ConvertSurfaceFormat;
extern tSDL_ConvertPixels *SDL_ConvertPixels;
extern tSDL_FillRect *SDL_FillRect;
extern tSDL_FillRects *SDL_FillRects;
extern tSDL_UpperBlit *SDL_UpperBlit;
extern tSDL_LowerBlit *SDL_LowerBlit;
extern tSDL_SoftStretch *SDL_SoftStretch;
extern tSDL_UpperBlitScaled *SDL_UpperBlitScaled;
extern tSDL_LowerBlitScaled *SDL_LowerBlitScaled;

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

