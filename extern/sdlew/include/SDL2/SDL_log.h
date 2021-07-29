
#ifndef _SDL_log_h
#define _SDL_log_h

#include "SDL_stdinc.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SDL_MAX_LOG_MESSAGE 4096

enum
{
    SDL_LOG_CATEGORY_APPLICATION,
    SDL_LOG_CATEGORY_ERROR,
    SDL_LOG_CATEGORY_ASSERT,
    SDL_LOG_CATEGORY_SYSTEM,
    SDL_LOG_CATEGORY_AUDIO,
    SDL_LOG_CATEGORY_VIDEO,
    SDL_LOG_CATEGORY_RENDER,
    SDL_LOG_CATEGORY_INPUT,
    SDL_LOG_CATEGORY_TEST,

    SDL_LOG_CATEGORY_RESERVED1,
    SDL_LOG_CATEGORY_RESERVED2,
    SDL_LOG_CATEGORY_RESERVED3,
    SDL_LOG_CATEGORY_RESERVED4,
    SDL_LOG_CATEGORY_RESERVED5,
    SDL_LOG_CATEGORY_RESERVED6,
    SDL_LOG_CATEGORY_RESERVED7,
    SDL_LOG_CATEGORY_RESERVED8,
    SDL_LOG_CATEGORY_RESERVED9,
    SDL_LOG_CATEGORY_RESERVED10,

    SDL_LOG_CATEGORY_CUSTOM
};

typedef enum
{
    SDL_LOG_PRIORITY_VERBOSE = 1,
    SDL_LOG_PRIORITY_DEBUG,
    SDL_LOG_PRIORITY_INFO,
    SDL_LOG_PRIORITY_WARN,
    SDL_LOG_PRIORITY_ERROR,
    SDL_LOG_PRIORITY_CRITICAL,
    SDL_NUM_LOG_PRIORITIES
} SDL_LogPriority;

typedef void SDLCALL tSDL_LogSetAllPriority(SDL_LogPriority priority);

typedef void SDLCALL tSDL_LogSetPriority(int category,
                                                SDL_LogPriority priority);

typedef SDL_LogPriority SDLCALL tSDL_LogGetPriority(int category);

typedef void SDLCALL tSDL_LogResetPriorities(void);

typedef void SDLCALL tSDL_Log(const char *fmt, ...);

typedef void SDLCALL tSDL_LogVerbose(int category, const char *fmt, ...);

typedef void SDLCALL tSDL_LogDebug(int category, const char *fmt, ...);

typedef void SDLCALL tSDL_LogInfo(int category, const char *fmt, ...);

typedef void SDLCALL tSDL_LogWarn(int category, const char *fmt, ...);

typedef void SDLCALL tSDL_LogError(int category, const char *fmt, ...);

typedef void SDLCALL tSDL_LogCritical(int category, const char *fmt, ...);

typedef void SDLCALL tSDL_LogMessage(int category,
                                            SDL_LogPriority priority,
                                            const char *fmt, ...);

typedef void SDLCALL tSDL_LogMessageV(int category,
                                             SDL_LogPriority priority,
                                             const char *fmt, va_list ap);

typedef void (*SDL_LogOutputFunction)(void *userdata, int category, SDL_LogPriority priority, const char *message);

typedef void SDLCALL tSDL_LogGetOutputFunction(SDL_LogOutputFunction *callback, void **userdata);

typedef void SDLCALL tSDL_LogSetOutputFunction(SDL_LogOutputFunction callback, void *userdata);

extern tSDL_LogSetAllPriority *SDL_LogSetAllPriority;
extern tSDL_LogSetPriority *SDL_LogSetPriority;
extern tSDL_LogGetPriority *SDL_LogGetPriority;
extern tSDL_LogResetPriorities *SDL_LogResetPriorities;
extern tSDL_Log *SDL_Log;
extern tSDL_LogVerbose *SDL_LogVerbose;
extern tSDL_LogDebug *SDL_LogDebug;
extern tSDL_LogInfo *SDL_LogInfo;
extern tSDL_LogWarn *SDL_LogWarn;
extern tSDL_LogError *SDL_LogError;
extern tSDL_LogCritical *SDL_LogCritical;
extern tSDL_LogMessage *SDL_LogMessage;
extern tSDL_LogMessageV *SDL_LogMessageV;
extern tSDL_LogGetOutputFunction *SDL_LogGetOutputFunction;
extern tSDL_LogSetOutputFunction *SDL_LogSetOutputFunction;

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

