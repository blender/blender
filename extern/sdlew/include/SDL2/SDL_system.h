
#ifndef _SDL_system_h
#define _SDL_system_h

#include "SDL_stdinc.h"

#if defined(__IPHONEOS__) && __IPHONEOS__
#include "SDL_video.h"
#include "SDL_keyboard.h"
#endif

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__IPHONEOS__) && __IPHONEOS__

typedef int SDLCALL tSDL_iPhoneSetAnimationCallback(SDL_Window * window, int interval, void (*callback)(void*), void *callbackParam);
typedef void SDLCALL tSDL_iPhoneSetEventPump(SDL_bool enabled);

#endif

#if defined(__ANDROID__) && __ANDROID__

typedef void * SDLCALL tSDL_AndroidGetJNIEnv();

typedef void * SDLCALL tSDL_AndroidGetActivity();

#define SDL_ANDROID_EXTERNAL_STORAGE_READ   0x01
#define SDL_ANDROID_EXTERNAL_STORAGE_WRITE  0x02

typedef const char * SDLCALL tSDL_AndroidGetInternalStoragePath();

typedef int SDLCALL tSDL_AndroidGetExternalStorageState();

typedef const char * SDLCALL tSDL_AndroidGetExternalStoragePath();

#endif

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

