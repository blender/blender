
#ifndef _SDL_mouse_h
#define _SDL_mouse_h

#include "SDL_stdinc.h"
#include "SDL_error.h"
#include "SDL_video.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SDL_Cursor SDL_Cursor;

typedef enum
{
    SDL_SYSTEM_CURSOR_ARROW,
    SDL_SYSTEM_CURSOR_IBEAM,
    SDL_SYSTEM_CURSOR_WAIT,
    SDL_SYSTEM_CURSOR_CROSSHAIR,
    SDL_SYSTEM_CURSOR_WAITARROW,
    SDL_SYSTEM_CURSOR_SIZENWSE,
    SDL_SYSTEM_CURSOR_SIZENESW,
    SDL_SYSTEM_CURSOR_SIZEWE,
    SDL_SYSTEM_CURSOR_SIZENS,
    SDL_SYSTEM_CURSOR_SIZEALL,
    SDL_SYSTEM_CURSOR_NO,
    SDL_SYSTEM_CURSOR_HAND,
    SDL_NUM_SYSTEM_CURSORS
} SDL_SystemCursor;

typedef SDL_Window * SDLCALL tSDL_GetMouseFocus(void);

typedef Uint32 SDLCALL tSDL_GetMouseState(int *x, int *y);

typedef Uint32 SDLCALL tSDL_GetRelativeMouseState(int *x, int *y);

typedef void SDLCALL tSDL_WarpMouseInWindow(SDL_Window * window,
                                                   int x, int y);

typedef int SDLCALL tSDL_SetRelativeMouseMode(SDL_bool enabled);

typedef SDL_bool SDLCALL tSDL_GetRelativeMouseMode(void);

typedef SDL_Cursor * SDLCALL tSDL_CreateCursor(const Uint8 * data,
                                                     const Uint8 * mask,
                                                     int w, int h, int hot_x,
                                                     int hot_y);

typedef SDL_Cursor * SDLCALL tSDL_CreateColorCursor(SDL_Surface *surface,
                                                          int hot_x,
                                                          int hot_y);

typedef SDL_Cursor * SDLCALL tSDL_CreateSystemCursor(SDL_SystemCursor id);

typedef void SDLCALL tSDL_SetCursor(SDL_Cursor * cursor);

typedef SDL_Cursor * SDLCALL tSDL_GetCursor(void);

typedef SDL_Cursor * SDLCALL tSDL_GetDefaultCursor(void);

typedef void SDLCALL tSDL_FreeCursor(SDL_Cursor * cursor);

typedef int SDLCALL tSDL_ShowCursor(int toggle);

#define SDL_BUTTON(X)       (1 << ((X)-1))
#define SDL_BUTTON_LEFT     1
#define SDL_BUTTON_MIDDLE   2
#define SDL_BUTTON_RIGHT    3
#define SDL_BUTTON_X1       4
#define SDL_BUTTON_X2       5
#define SDL_BUTTON_LMASK    SDL_BUTTON(SDL_BUTTON_LEFT)
#define SDL_BUTTON_MMASK    SDL_BUTTON(SDL_BUTTON_MIDDLE)
#define SDL_BUTTON_RMASK    SDL_BUTTON(SDL_BUTTON_RIGHT)
#define SDL_BUTTON_X1MASK   SDL_BUTTON(SDL_BUTTON_X1)
#define SDL_BUTTON_X2MASK   SDL_BUTTON(SDL_BUTTON_X2)

extern tSDL_GetMouseFocus *SDL_GetMouseFocus;
extern tSDL_GetMouseState *SDL_GetMouseState;
extern tSDL_GetRelativeMouseState *SDL_GetRelativeMouseState;
extern tSDL_WarpMouseInWindow *SDL_WarpMouseInWindow;
extern tSDL_SetRelativeMouseMode *SDL_SetRelativeMouseMode;
extern tSDL_GetRelativeMouseMode *SDL_GetRelativeMouseMode;
extern tSDL_CreateCursor *SDL_CreateCursor;
extern tSDL_CreateColorCursor *SDL_CreateColorCursor;
extern tSDL_CreateSystemCursor *SDL_CreateSystemCursor;
extern tSDL_SetCursor *SDL_SetCursor;
extern tSDL_GetCursor *SDL_GetCursor;
extern tSDL_GetDefaultCursor *SDL_GetDefaultCursor;
extern tSDL_FreeCursor *SDL_FreeCursor;
extern tSDL_ShowCursor *SDL_ShowCursor;

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

