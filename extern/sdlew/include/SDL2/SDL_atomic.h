
#ifndef _SDL_atomic_h_
#define _SDL_atomic_h_

#include "SDL_stdinc.h"
#include "SDL_platform.h"

#include "begin_code.h"

#if defined(_MSC_VER) && (_MSC_VER >= 1500)
#include <intrin.h>
#define HAVE_MSC_ATOMICS 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef int SDL_SpinLock;

typedef SDL_bool SDLCALL tSDL_AtomicTryLock(SDL_SpinLock *lock);

typedef void SDLCALL tSDL_AtomicLock(SDL_SpinLock *lock);

typedef void SDLCALL tSDL_AtomicUnlock(SDL_SpinLock *lock);

#if defined(_MSC_VER) && (_MSC_VER > 1200)
void _ReadWriteBarrier(void);
#pragma intrinsic(_ReadWriteBarrier)
#define SDL_CompilerBarrier()   _ReadWriteBarrier()
#elif defined(__GNUC__)
#define SDL_CompilerBarrier()   __asm__ __volatile__ ("" : : : "memory")
#else
#define SDL_CompilerBarrier()   \
{ SDL_SpinLock _tmp = 0; SDL_AtomicLock(&_tmp); SDL_AtomicUnlock(&_tmp); }
#endif

#if defined(__GNUC__) && (defined(__powerpc__) || defined(__ppc__))
#define SDL_MemoryBarrierRelease()   __asm__ __volatile__ ("lwsync" : : : "memory")
#define SDL_MemoryBarrierAcquire()   __asm__ __volatile__ ("lwsync" : : : "memory")
#elif defined(__GNUC__) && defined(__arm__)
#if defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7EM__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__)
#define SDL_MemoryBarrierRelease()   __asm__ __volatile__ ("dmb ish" : : : "memory")
#define SDL_MemoryBarrierAcquire()   __asm__ __volatile__ ("dmb ish" : : : "memory")
#elif defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6T2__) || defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__)
#ifdef __thumb__

typedef void SDLCALL tSDL_MemoryBarrierRelease();
typedef void SDLCALL tSDL_MemoryBarrierAcquire();
#else
#define SDL_MemoryBarrierRelease()   __asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 5" : : "r"(0) : "memory")
#define SDL_MemoryBarrierAcquire()   __asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 5" : : "r"(0) : "memory")
#endif
#else
#define SDL_MemoryBarrierRelease()   __asm__ __volatile__ ("" : : : "memory")
#define SDL_MemoryBarrierAcquire()   __asm__ __volatile__ ("" : : : "memory")
#endif
#else

#define SDL_MemoryBarrierRelease()  SDL_CompilerBarrier()
#define SDL_MemoryBarrierAcquire()  SDL_CompilerBarrier()
#endif

#if defined(SDL_ATOMIC_DISABLED) && SDL_ATOMIC_DISABLED
#define SDL_DISABLE_ATOMIC_INLINE
#endif
#ifndef SDL_DISABLE_ATOMIC_INLINE

#ifdef HAVE_MSC_ATOMICS

#define SDL_AtomicSet(a, v)     _InterlockedExchange((long*)&(a)->value, (v))
#define SDL_AtomicAdd(a, v)     _InterlockedExchangeAdd((long*)&(a)->value, (v))
#define SDL_AtomicCAS(a, oldval, newval) (_InterlockedCompareExchange((long*)&(a)->value, (newval), (oldval)) == (oldval))
#define SDL_AtomicSetPtr(a, v)  _InterlockedExchangePointer((a), (v))
#if _M_IX86
#define SDL_AtomicCASPtr(a, oldval, newval) (_InterlockedCompareExchange((long*)(a), (long)(newval), (long)(oldval)) == (long)(oldval))
#else
#define SDL_AtomicCASPtr(a, oldval, newval) (_InterlockedCompareExchangePointer((a), (newval), (oldval)) == (oldval))
#endif

#elif defined(__MACOSX__)
#include <libkern/OSAtomic.h>

#define SDL_AtomicCAS(a, oldval, newval) OSAtomicCompareAndSwap32Barrier((oldval), (newval), &(a)->value)
#ifdef __LP64__
#define SDL_AtomicCASPtr(a, oldval, newval) OSAtomicCompareAndSwap64Barrier((int64_t)(oldval), (int64_t)(newval), (int64_t*)(a))
#else
#define SDL_AtomicCASPtr(a, oldval, newval) OSAtomicCompareAndSwap32Barrier((int32_t)(oldval), (int32_t)(newval), (int32_t*)(a))
#endif

#elif defined(HAVE_GCC_ATOMICS)

#define SDL_AtomicSet(a, v)     __sync_lock_test_and_set(&(a)->value, v)
#define SDL_AtomicAdd(a, v)     __sync_fetch_and_add(&(a)->value, v)
#define SDL_AtomicSetPtr(a, v)  __sync_lock_test_and_set(a, v)
#define SDL_AtomicCAS(a, oldval, newval) __sync_bool_compare_and_swap(&(a)->value, oldval, newval)
#define SDL_AtomicCASPtr(a, oldval, newval) __sync_bool_compare_and_swap(a, oldval, newval)

#endif

#endif

#ifndef SDL_atomic_t_defined
typedef struct { int value; } SDL_atomic_t;
#endif

#ifndef SDL_AtomicCAS
typedef SDL_bool SDLCALL tSDL_AtomicCAS(SDL_atomic_t *a, int oldval, int newval);
#endif

#ifndef SDL_AtomicSet
SDL_FORCE_INLINE int SDL_AtomicSet(SDL_atomic_t *a, int v)
{
    int value;
    do {
        value = a->value;
    } while (!SDL_AtomicCAS(a, value, v));
    return value;
}
#endif

#ifndef SDL_AtomicGet
SDL_FORCE_INLINE int SDL_AtomicGet(SDL_atomic_t *a)
{
    int value = a->value;
    SDL_CompilerBarrier();
    return value;
}
#endif

#ifndef SDL_AtomicAdd
SDL_FORCE_INLINE int SDL_AtomicAdd(SDL_atomic_t *a, int v)
{
    int value;
    do {
        value = a->value;
    } while (!SDL_AtomicCAS(a, value, (value + v)));
    return value;
}
#endif

#ifndef SDL_AtomicIncRef
#define SDL_AtomicIncRef(a)    SDL_AtomicAdd(a, 1)
#endif

#ifndef SDL_AtomicDecRef
#define SDL_AtomicDecRef(a)    (SDL_AtomicAdd(a, -1) == 1)
#endif

#ifndef SDL_AtomicCASPtr
typedef SDL_bool SDLCALL tSDL_AtomicCASPtr(void* *a, void *oldval, void *newval);
#endif

#ifndef SDL_AtomicSetPtr
SDL_FORCE_INLINE void* SDL_AtomicSetPtr(void* *a, void* v)
{
    void* value;
    do {
        value = *a;
    } while (!SDL_AtomicCASPtr(a, value, v));
    return value;
}
#endif

#ifndef SDL_AtomicGetPtr
SDL_FORCE_INLINE void* SDL_AtomicGetPtr(void* *a)
{
    void* value = *a;
    SDL_CompilerBarrier();
    return value;
}
#endif

extern tSDL_AtomicTryLock *SDL_AtomicTryLock;
extern tSDL_AtomicLock *SDL_AtomicLock;
extern tSDL_AtomicUnlock *SDL_AtomicUnlock;

#ifdef __cplusplus
}
#endif

#include "close_code.h"

#endif

