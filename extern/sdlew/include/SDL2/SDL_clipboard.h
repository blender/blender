
#ifndef _SDL_clipboard_h
#define _SDL_clipboard_h

#include "SDL_stdinc.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int SDLCALL tSDL_SetClipboardText(const char *text);

typedef char * SDLCALL tSDL_GetClipboardText(void);

typedef SDL_bool SDLCALL tSDL_HasClipboardText(void);

extern tSDL_SetClipboardText *SDL_SetClipboardText;
extern tSDL_GetClipboardText *SDL_GetClipboardText;
extern tSDL_HasClipboardText *SDL_HasClipboardText;

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

