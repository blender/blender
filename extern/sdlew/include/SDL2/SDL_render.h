
#ifndef _SDL_render_h
#define _SDL_render_h

#include "SDL_stdinc.h"
#include "SDL_rect.h"
#include "SDL_video.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    SDL_RENDERER_SOFTWARE = 0x00000001,
    SDL_RENDERER_ACCELERATED = 0x00000002,
    SDL_RENDERER_PRESENTVSYNC = 0x00000004,
    SDL_RENDERER_TARGETTEXTURE = 0x00000008
} SDL_RendererFlags;

typedef struct SDL_RendererInfo
{
    const char *name;
    Uint32 flags;
    Uint32 num_texture_formats;
    Uint32 texture_formats[16];
    int max_texture_width;
    int max_texture_height;
} SDL_RendererInfo;

typedef enum
{
    SDL_TEXTUREACCESS_STATIC,
    SDL_TEXTUREACCESS_STREAMING,
    SDL_TEXTUREACCESS_TARGET
} SDL_TextureAccess;

typedef enum
{
    SDL_TEXTUREMODULATE_NONE = 0x00000000,
    SDL_TEXTUREMODULATE_COLOR = 0x00000001,
    SDL_TEXTUREMODULATE_ALPHA = 0x00000002
} SDL_TextureModulate;

typedef enum
{
    SDL_FLIP_NONE = 0x00000000,
    SDL_FLIP_HORIZONTAL = 0x00000001,
    SDL_FLIP_VERTICAL = 0x00000002
} SDL_RendererFlip;

struct SDL_Renderer;
typedef struct SDL_Renderer SDL_Renderer;

struct SDL_Texture;
typedef struct SDL_Texture SDL_Texture;

typedef int SDLCALL tSDL_GetNumRenderDrivers(void);

typedef int SDLCALL tSDL_GetRenderDriverInfo(int index,
                                                    SDL_RendererInfo * info);

typedef int SDLCALL tSDL_CreateWindowAndRenderer(
                                int width, int height, Uint32 window_flags,
                                SDL_Window **window, SDL_Renderer **renderer);

typedef SDL_Renderer * SDLCALL tSDL_CreateRenderer(SDL_Window * window,
                                               int index, Uint32 flags);

typedef SDL_Renderer * SDLCALL tSDL_CreateSoftwareRenderer(SDL_Surface * surface);

typedef SDL_Renderer * SDLCALL tSDL_GetRenderer(SDL_Window * window);

typedef int SDLCALL tSDL_GetRendererInfo(SDL_Renderer * renderer,
                                                SDL_RendererInfo * info);

typedef int SDLCALL tSDL_GetRendererOutputSize(SDL_Renderer * renderer,
                                                      int *w, int *h);

typedef SDL_Texture * SDLCALL tSDL_CreateTexture(SDL_Renderer * renderer,
                                                        Uint32 format,
                                                        int access, int w,
                                                        int h);

typedef SDL_Texture * SDLCALL tSDL_CreateTextureFromSurface(SDL_Renderer * renderer, SDL_Surface * surface);

typedef int SDLCALL tSDL_QueryTexture(SDL_Texture * texture,
                                             Uint32 * format, int *access,
                                             int *w, int *h);

typedef int SDLCALL tSDL_SetTextureColorMod(SDL_Texture * texture,
                                                   Uint8 r, Uint8 g, Uint8 b);

typedef int SDLCALL tSDL_GetTextureColorMod(SDL_Texture * texture,
                                                   Uint8 * r, Uint8 * g,
                                                   Uint8 * b);

typedef int SDLCALL tSDL_SetTextureAlphaMod(SDL_Texture * texture,
                                                   Uint8 alpha);

typedef int SDLCALL tSDL_GetTextureAlphaMod(SDL_Texture * texture,
                                                   Uint8 * alpha);

typedef int SDLCALL tSDL_SetTextureBlendMode(SDL_Texture * texture,
                                                    SDL_BlendMode blendMode);

typedef int SDLCALL tSDL_GetTextureBlendMode(SDL_Texture * texture,
                                                    SDL_BlendMode *blendMode);

typedef int SDLCALL tSDL_UpdateTexture(SDL_Texture * texture,
                                              const SDL_Rect * rect,
                                              const void *pixels, int pitch);

typedef int SDLCALL tSDL_LockTexture(SDL_Texture * texture,
                                            const SDL_Rect * rect,
                                            void **pixels, int *pitch);

typedef void SDLCALL tSDL_UnlockTexture(SDL_Texture * texture);

typedef SDL_bool SDLCALL tSDL_RenderTargetSupported(SDL_Renderer *renderer);

typedef int SDLCALL tSDL_SetRenderTarget(SDL_Renderer *renderer,
                                                SDL_Texture *texture);

typedef SDL_Texture * SDLCALL tSDL_GetRenderTarget(SDL_Renderer *renderer);

typedef int SDLCALL tSDL_RenderSetLogicalSize(SDL_Renderer * renderer, int w, int h);

typedef void SDLCALL tSDL_RenderGetLogicalSize(SDL_Renderer * renderer, int *w, int *h);

typedef int SDLCALL tSDL_RenderSetViewport(SDL_Renderer * renderer,
                                                  const SDL_Rect * rect);

typedef void SDLCALL tSDL_RenderGetViewport(SDL_Renderer * renderer,
                                                   SDL_Rect * rect);

typedef int SDLCALL tSDL_RenderSetClipRect(SDL_Renderer * renderer,
                                                  const SDL_Rect * rect);

typedef void SDLCALL tSDL_RenderGetClipRect(SDL_Renderer * renderer,
                                                   SDL_Rect * rect);

typedef int SDLCALL tSDL_RenderSetScale(SDL_Renderer * renderer,
                                               float scaleX, float scaleY);

typedef void SDLCALL tSDL_RenderGetScale(SDL_Renderer * renderer,
                                               float *scaleX, float *scaleY);

extern DECLSPEC int SDL_SetRenderDrawColor(SDL_Renderer * renderer,
                                           Uint8 r, Uint8 g, Uint8 b,
                                           Uint8 a);

extern DECLSPEC int SDL_GetRenderDrawColor(SDL_Renderer * renderer,
                                           Uint8 * r, Uint8 * g, Uint8 * b,
                                           Uint8 * a);

typedef int SDLCALL tSDL_SetRenderDrawBlendMode(SDL_Renderer * renderer,
                                                       SDL_BlendMode blendMode);

typedef int SDLCALL tSDL_GetRenderDrawBlendMode(SDL_Renderer * renderer,
                                                       SDL_BlendMode *blendMode);

typedef int SDLCALL tSDL_RenderClear(SDL_Renderer * renderer);

typedef int SDLCALL tSDL_RenderDrawPoint(SDL_Renderer * renderer,
                                                int x, int y);

typedef int SDLCALL tSDL_RenderDrawPoints(SDL_Renderer * renderer,
                                                 const SDL_Point * points,
                                                 int count);

typedef int SDLCALL tSDL_RenderDrawLine(SDL_Renderer * renderer,
                                               int x1, int y1, int x2, int y2);

typedef int SDLCALL tSDL_RenderDrawLines(SDL_Renderer * renderer,
                                                const SDL_Point * points,
                                                int count);

typedef int SDLCALL tSDL_RenderDrawRect(SDL_Renderer * renderer,
                                               const SDL_Rect * rect);

typedef int SDLCALL tSDL_RenderDrawRects(SDL_Renderer * renderer,
                                                const SDL_Rect * rects,
                                                int count);

typedef int SDLCALL tSDL_RenderFillRect(SDL_Renderer * renderer,
                                               const SDL_Rect * rect);

typedef int SDLCALL tSDL_RenderFillRects(SDL_Renderer * renderer,
                                                const SDL_Rect * rects,
                                                int count);

typedef int SDLCALL tSDL_RenderCopy(SDL_Renderer * renderer,
                                           SDL_Texture * texture,
                                           const SDL_Rect * srcrect,
                                           const SDL_Rect * dstrect);

typedef int SDLCALL tSDL_RenderCopyEx(SDL_Renderer * renderer,
                                           SDL_Texture * texture,
                                           const SDL_Rect * srcrect,
                                           const SDL_Rect * dstrect,
                                           const double angle,
                                           const SDL_Point *center,
                                           const SDL_RendererFlip flip);

typedef int SDLCALL tSDL_RenderReadPixels(SDL_Renderer * renderer,
                                                 const SDL_Rect * rect,
                                                 Uint32 format,
                                                 void *pixels, int pitch);

typedef void SDLCALL tSDL_RenderPresent(SDL_Renderer * renderer);

typedef void SDLCALL tSDL_DestroyTexture(SDL_Texture * texture);

typedef void SDLCALL tSDL_DestroyRenderer(SDL_Renderer * renderer);

typedef int SDLCALL tSDL_GL_BindTexture(SDL_Texture *texture, float *texw, float *texh);

typedef int SDLCALL tSDL_GL_UnbindTexture(SDL_Texture *texture);

extern tSDL_GetNumRenderDrivers *SDL_GetNumRenderDrivers;
extern tSDL_GetRenderDriverInfo *SDL_GetRenderDriverInfo;
extern tSDL_CreateWindowAndRenderer *SDL_CreateWindowAndRenderer;
extern tSDL_CreateRenderer *SDL_CreateRenderer;
extern tSDL_CreateSoftwareRenderer *SDL_CreateSoftwareRenderer;
extern tSDL_GetRenderer *SDL_GetRenderer;
extern tSDL_GetRendererInfo *SDL_GetRendererInfo;
extern tSDL_GetRendererOutputSize *SDL_GetRendererOutputSize;
extern tSDL_CreateTexture *SDL_CreateTexture;
extern tSDL_CreateTextureFromSurface *SDL_CreateTextureFromSurface;
extern tSDL_QueryTexture *SDL_QueryTexture;
extern tSDL_SetTextureColorMod *SDL_SetTextureColorMod;
extern tSDL_GetTextureColorMod *SDL_GetTextureColorMod;
extern tSDL_SetTextureAlphaMod *SDL_SetTextureAlphaMod;
extern tSDL_GetTextureAlphaMod *SDL_GetTextureAlphaMod;
extern tSDL_SetTextureBlendMode *SDL_SetTextureBlendMode;
extern tSDL_GetTextureBlendMode *SDL_GetTextureBlendMode;
extern tSDL_UpdateTexture *SDL_UpdateTexture;
extern tSDL_LockTexture *SDL_LockTexture;
extern tSDL_UnlockTexture *SDL_UnlockTexture;
extern tSDL_RenderTargetSupported *SDL_RenderTargetSupported;
extern tSDL_SetRenderTarget *SDL_SetRenderTarget;
extern tSDL_GetRenderTarget *SDL_GetRenderTarget;
extern tSDL_RenderSetLogicalSize *SDL_RenderSetLogicalSize;
extern tSDL_RenderGetLogicalSize *SDL_RenderGetLogicalSize;
extern tSDL_RenderSetViewport *SDL_RenderSetViewport;
extern tSDL_RenderGetViewport *SDL_RenderGetViewport;
extern tSDL_RenderSetClipRect *SDL_RenderSetClipRect;
extern tSDL_RenderGetClipRect *SDL_RenderGetClipRect;
extern tSDL_RenderSetScale *SDL_RenderSetScale;
extern tSDL_RenderGetScale *SDL_RenderGetScale;
extern tSDL_SetRenderDrawBlendMode *SDL_SetRenderDrawBlendMode;
extern tSDL_GetRenderDrawBlendMode *SDL_GetRenderDrawBlendMode;
extern tSDL_RenderClear *SDL_RenderClear;
extern tSDL_RenderDrawPoint *SDL_RenderDrawPoint;
extern tSDL_RenderDrawPoints *SDL_RenderDrawPoints;
extern tSDL_RenderDrawLine *SDL_RenderDrawLine;
extern tSDL_RenderDrawLines *SDL_RenderDrawLines;
extern tSDL_RenderDrawRect *SDL_RenderDrawRect;
extern tSDL_RenderDrawRects *SDL_RenderDrawRects;
extern tSDL_RenderFillRect *SDL_RenderFillRect;
extern tSDL_RenderFillRects *SDL_RenderFillRects;
extern tSDL_RenderCopy *SDL_RenderCopy;
extern tSDL_RenderCopyEx *SDL_RenderCopyEx;
extern tSDL_RenderReadPixels *SDL_RenderReadPixels;
extern tSDL_RenderPresent *SDL_RenderPresent;
extern tSDL_DestroyTexture *SDL_DestroyTexture;
extern tSDL_DestroyRenderer *SDL_DestroyRenderer;
extern tSDL_GL_BindTexture *SDL_GL_BindTexture;
extern tSDL_GL_UnbindTexture *SDL_GL_UnbindTexture;

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

