
#ifndef _SDL_shape_h
#define _SDL_shape_h

#include "SDL_stdinc.h"
#include "SDL_pixels.h"
#include "SDL_rect.h"
#include "SDL_surface.h"
#include "SDL_video.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SDL_NONSHAPEABLE_WINDOW -1
#define SDL_INVALID_SHAPE_ARGUMENT -2
#define SDL_WINDOW_LACKS_SHAPE -3

typedef SDL_Window * SDLCALL tSDL_CreateShapedWindow(const char *title,unsigned int x,unsigned int y,unsigned int w,unsigned int h,Uint32 flags);

typedef SDL_bool SDLCALL tSDL_IsShapedWindow(const SDL_Window *window);

typedef enum {

    ShapeModeDefault,

    ShapeModeBinarizeAlpha,

    ShapeModeReverseBinarizeAlpha,

    ShapeModeColorKey
} WindowShapeMode;

#define SDL_SHAPEMODEALPHA(mode) (mode == ShapeModeDefault || mode == ShapeModeBinarizeAlpha || mode == ShapeModeReverseBinarizeAlpha)

typedef union {

    Uint8 binarizationCutoff;
    SDL_Color colorKey;
} SDL_WindowShapeParams;

typedef struct SDL_WindowShapeMode {

    WindowShapeMode mode;

    SDL_WindowShapeParams parameters;
} SDL_WindowShapeMode;

typedef int SDLCALL tSDL_SetWindowShape(SDL_Window *window,SDL_Surface *shape,SDL_WindowShapeMode *shape_mode);

typedef int SDLCALL tSDL_GetShapedWindowMode(SDL_Window *window,SDL_WindowShapeMode *shape_mode);

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif
