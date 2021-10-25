
#ifndef _SDL_mutex_h
#define _SDL_mutex_h

#include "SDL_stdinc.h"
#include "SDL_error.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SDL_MUTEX_TIMEDOUT  1

#define SDL_MUTEX_MAXWAIT   (~(Uint32)0)

struct SDL_mutex;
typedef struct SDL_mutex SDL_mutex;

typedef SDL_mutex * SDLCALL tSDL_CreateMutex(void);

#define SDL_mutexP(m)   SDL_LockMutex(m)
typedef int SDLCALL tSDL_LockMutex(SDL_mutex * mutex);

typedef int SDLCALL tSDL_TryLockMutex(SDL_mutex * mutex);

#define SDL_mutexV(m)   SDL_UnlockMutex(m)
typedef int SDLCALL tSDL_UnlockMutex(SDL_mutex * mutex);

typedef void SDLCALL tSDL_DestroyMutex(SDL_mutex * mutex);

struct SDL_semaphore;
typedef struct SDL_semaphore SDL_sem;

typedef SDL_sem * SDLCALL tSDL_CreateSemaphore(Uint32 initial_value);

typedef void SDLCALL tSDL_DestroySemaphore(SDL_sem * sem);

typedef int SDLCALL tSDL_SemWait(SDL_sem * sem);

typedef int SDLCALL tSDL_SemTryWait(SDL_sem * sem);

typedef int SDLCALL tSDL_SemWaitTimeout(SDL_sem * sem, Uint32 ms);

typedef int SDLCALL tSDL_SemPost(SDL_sem * sem);

typedef Uint32 SDLCALL tSDL_SemValue(SDL_sem * sem);

struct SDL_cond;
typedef struct SDL_cond SDL_cond;

typedef SDL_cond * SDLCALL tSDL_CreateCond(void);

typedef void SDLCALL tSDL_DestroyCond(SDL_cond * cond);

typedef int SDLCALL tSDL_CondSignal(SDL_cond * cond);

typedef int SDLCALL tSDL_CondBroadcast(SDL_cond * cond);

typedef int SDLCALL tSDL_CondWait(SDL_cond * cond, SDL_mutex * mutex);

typedef int SDLCALL tSDL_CondWaitTimeout(SDL_cond * cond,
                                                SDL_mutex * mutex, Uint32 ms);

extern tSDL_CreateMutex *SDL_CreateMutex;
extern tSDL_LockMutex *SDL_LockMutex;
extern tSDL_TryLockMutex *SDL_TryLockMutex;
extern tSDL_UnlockMutex *SDL_UnlockMutex;
extern tSDL_DestroyMutex *SDL_DestroyMutex;
extern tSDL_CreateSemaphore *SDL_CreateSemaphore;
extern tSDL_DestroySemaphore *SDL_DestroySemaphore;
extern tSDL_SemWait *SDL_SemWait;
extern tSDL_SemTryWait *SDL_SemTryWait;
extern tSDL_SemWaitTimeout *SDL_SemWaitTimeout;
extern tSDL_SemPost *SDL_SemPost;
extern tSDL_SemValue *SDL_SemValue;
extern tSDL_CreateCond *SDL_CreateCond;
extern tSDL_DestroyCond *SDL_DestroyCond;
extern tSDL_CondSignal *SDL_CondSignal;
extern tSDL_CondBroadcast *SDL_CondBroadcast;
extern tSDL_CondWait *SDL_CondWait;
extern tSDL_CondWaitTimeout *SDL_CondWaitTimeout;

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

