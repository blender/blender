
#ifndef _SDL_thread_h
#define _SDL_thread_h

#include "SDL_stdinc.h"
#include "SDL_error.h"

#include "SDL_atomic.h"
#include "SDL_mutex.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

struct SDL_Thread;
typedef struct SDL_Thread SDL_Thread;

typedef unsigned long SDL_threadID;

typedef unsigned int SDL_TLSID;

typedef enum {
    SDL_THREAD_PRIORITY_LOW,
    SDL_THREAD_PRIORITY_NORMAL,
    SDL_THREAD_PRIORITY_HIGH
} SDL_ThreadPriority;

typedef int (SDLCALL * SDL_ThreadFunction) (void *data);

#if defined(__WIN32__) && !defined(HAVE_LIBC)

#define SDL_PASSED_BEGINTHREAD_ENDTHREAD
#include <process.h>

typedef uintptr_t(__cdecl * pfnSDL_CurrentBeginThread) (void *, unsigned,
                                                        unsigned (__stdcall *
                                                                  func) (void
                                                                         *),
                                                        void *arg, unsigned,
                                                        unsigned *threadID);
typedef void (__cdecl * pfnSDL_CurrentEndThread) (unsigned code);

extern DECLSPEC SDL_Thread *SDLCALL
SDL_CreateThread(SDL_ThreadFunction fn, const char *name, void *data,
                 pfnSDL_CurrentBeginThread pfnBeginThread,
                 pfnSDL_CurrentEndThread pfnEndThread);

#define SDL_CreateThread(fn, name, data) SDL_CreateThread(fn, name, data, (pfnSDL_CurrentBeginThread)_beginthreadex, (pfnSDL_CurrentEndThread)_endthreadex)

#else

extern DECLSPEC SDL_Thread *SDLCALL
SDL_CreateThread(SDL_ThreadFunction fn, const char *name, void *data);

#endif

typedef const char * SDLCALL tSDL_GetThreadName(SDL_Thread *thread);

typedef SDL_threadID SDLCALL tSDL_ThreadID(void);

typedef SDL_threadID SDLCALL tSDL_GetThreadID(SDL_Thread * thread);

typedef int SDLCALL tSDL_SetThreadPriority(SDL_ThreadPriority priority);

typedef void SDLCALL tSDL_WaitThread(SDL_Thread * thread, int *status);

typedef SDL_TLSID SDLCALL tSDL_TLSCreate(void);

typedef void * SDLCALL tSDL_TLSGet(SDL_TLSID id);

typedef int SDLCALL tSDL_TLSSet(SDL_TLSID id, const void *value, void (*destructor)(void*));

extern tSDL_GetThreadName *SDL_GetThreadName;
extern tSDL_ThreadID *SDL_ThreadID;
extern tSDL_GetThreadID *SDL_GetThreadID;
extern tSDL_SetThreadPriority *SDL_SetThreadPriority;
extern tSDL_WaitThread *SDL_WaitThread;
extern tSDL_TLSCreate *SDL_TLSCreate;
extern tSDL_TLSGet *SDL_TLSGet;
extern tSDL_TLSSet *SDL_TLSSet;

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

