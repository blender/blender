
#ifndef _SDL_messagebox_h
#define _SDL_messagebox_h

#include "SDL_stdinc.h"
#include "SDL_video.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    SDL_MESSAGEBOX_ERROR        = 0x00000010,
    SDL_MESSAGEBOX_WARNING      = 0x00000020,
    SDL_MESSAGEBOX_INFORMATION  = 0x00000040
} SDL_MessageBoxFlags;

typedef enum
{
    SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT = 0x00000001,
    SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT = 0x00000002
} SDL_MessageBoxButtonFlags;

typedef struct
{
    Uint32 flags;
    int buttonid;
    const char * text;
} SDL_MessageBoxButtonData;

typedef struct
{
    Uint8 r, g, b;
} SDL_MessageBoxColor;

typedef enum
{
    SDL_MESSAGEBOX_COLOR_BACKGROUND,
    SDL_MESSAGEBOX_COLOR_TEXT,
    SDL_MESSAGEBOX_COLOR_BUTTON_BORDER,
    SDL_MESSAGEBOX_COLOR_BUTTON_BACKGROUND,
    SDL_MESSAGEBOX_COLOR_BUTTON_SELECTED,
    SDL_MESSAGEBOX_COLOR_MAX
} SDL_MessageBoxColorType;

typedef struct
{
    SDL_MessageBoxColor colors[SDL_MESSAGEBOX_COLOR_MAX];
} SDL_MessageBoxColorScheme;

typedef struct
{
    Uint32 flags;
    SDL_Window *window;
    const char *title;
    const char *message;

    int numbuttons;
    const SDL_MessageBoxButtonData *buttons;

    const SDL_MessageBoxColorScheme *colorScheme;
} SDL_MessageBoxData;

typedef int SDLCALL tSDL_ShowMessageBox(const SDL_MessageBoxData *messageboxdata, int *buttonid);

typedef int SDLCALL tSDL_ShowSimpleMessageBox(Uint32 flags, const char *title, const char *message, SDL_Window *window);

extern tSDL_ShowMessageBox *SDL_ShowMessageBox;
extern tSDL_ShowSimpleMessageBox *SDL_ShowSimpleMessageBox;

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

