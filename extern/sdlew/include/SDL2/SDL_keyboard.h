
#ifndef _SDL_keyboard_h
#define _SDL_keyboard_h

#include "SDL_stdinc.h"
#include "SDL_error.h"
#include "SDL_keycode.h"
#include "SDL_video.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SDL_Keysym
{
    SDL_Scancode scancode;
    SDL_Keycode sym;
    Uint16 mod;
    Uint32 unused;
} SDL_Keysym;

typedef SDL_Window * SDLCALL tSDL_GetKeyboardFocus(void);

typedef const Uint8 * SDLCALL tSDL_GetKeyboardState(int *numkeys);

typedef SDL_Keymod SDLCALL tSDL_GetModState(void);

typedef void SDLCALL tSDL_SetModState(SDL_Keymod modstate);

typedef SDL_Keycode SDLCALL tSDL_GetKeyFromScancode(SDL_Scancode scancode);

typedef SDL_Scancode SDLCALL tSDL_GetScancodeFromKey(SDL_Keycode key);

typedef const char * SDLCALL tSDL_GetScancodeName(SDL_Scancode scancode);

typedef SDL_Scancode SDLCALL tSDL_GetScancodeFromName(const char *name);

typedef const char * SDLCALL tSDL_GetKeyName(SDL_Keycode key);

typedef SDL_Keycode SDLCALL tSDL_GetKeyFromName(const char *name);

typedef void SDLCALL tSDL_StartTextInput(void);

typedef SDL_bool SDLCALL tSDL_IsTextInputActive(void);

typedef void SDLCALL tSDL_StopTextInput(void);

typedef void SDLCALL tSDL_SetTextInputRect(SDL_Rect *rect);

typedef SDL_bool SDLCALL tSDL_HasScreenKeyboardSupport(void);

typedef SDL_bool SDLCALL tSDL_IsScreenKeyboardShown(SDL_Window *window);

extern tSDL_GetKeyboardFocus *SDL_GetKeyboardFocus;
extern tSDL_GetKeyboardState *SDL_GetKeyboardState;
extern tSDL_GetModState *SDL_GetModState;
extern tSDL_SetModState *SDL_SetModState;
extern tSDL_GetKeyFromScancode *SDL_GetKeyFromScancode;
extern tSDL_GetScancodeFromKey *SDL_GetScancodeFromKey;
extern tSDL_GetScancodeName *SDL_GetScancodeName;
extern tSDL_GetScancodeFromName *SDL_GetScancodeFromName;
extern tSDL_GetKeyName *SDL_GetKeyName;
extern tSDL_GetKeyFromName *SDL_GetKeyFromName;
extern tSDL_StartTextInput *SDL_StartTextInput;
extern tSDL_IsTextInputActive *SDL_IsTextInputActive;
extern tSDL_StopTextInput *SDL_StopTextInput;
extern tSDL_SetTextInputRect *SDL_SetTextInputRect;
extern tSDL_HasScreenKeyboardSupport *SDL_HasScreenKeyboardSupport;
extern tSDL_IsScreenKeyboardShown *SDL_IsScreenKeyboardShown;

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

