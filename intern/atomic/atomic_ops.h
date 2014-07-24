/*
 * Adopted from jemalloc with this license:
 *
 * Copyright (C) 2002-2013 Jason Evans <jasone@canonware.com>.
 * All rights reserved.
 * Copyright (C) 2007-2012 Mozilla Foundation.  All rights reserved.
 * Copyright (C) 2009-2013 Facebook, Inc.  All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice(s),
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice(s),
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.

 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
 * EVENT SHALL THE COPYRIGHT HOLDER(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __ATOMIC_OPS_H__
#define __ATOMIC_OPS_H__

#include <assert.h>

#if defined (__APPLE__)
#  include <libkern/OSAtomic.h>
#elif defined(_MSC_VER)
#  include <windows.h>
#elif defined(__arm__)
/* Attempt to fix compilation error on Debian armel kernel.
 * arm7 architecture does have both 32 and 64bit atomics, however
 * it's gcc doesn't have __GCC_HAVE_SYNC_COMPARE_AND_SWAP_n defined.
 */
#  define JE_FORCE_SYNC_COMPARE_AND_SWAP_8
#  define JE_FORCE_SYNC_COMPARE_AND_SWAP_4
#endif

/* needed for int types */
#include "../../source/blender/blenlib/BLI_sys_types.h"
#include <stdlib.h>
#include <stddef.h>

/* little macro so inline keyword works */
#if defined(_MSC_VER)
#  define ATOMIC_INLINE static __forceinline
#else
#  if (defined(__APPLE__) && defined(__ppc__))
/* static inline __attribute__ here breaks osx ppc gcc42 build */
#    define ATOMIC_INLINE static __attribute__((always_inline))
#  else
#    define ATOMIC_INLINE static inline __attribute__((always_inline))
#  endif
#endif

/* This is becoming a bit nastier that it was originally foreseen,
 * consider using autoconfig detection instead.
 */
#if defined(_M_X64) || defined(__amd64__) || defined(__x86_64__) || defined(__s390x__) || defined(__powerpc64__) || defined(__aarch64__) || (defined(__sparc__) && defined(__arch64__)) || defined(__alpha__) || defined(__mips64)
#  define LG_SIZEOF_PTR 3
#  define LG_SIZEOF_INT 2
#else
#  define LG_SIZEOF_PTR 2
#  define LG_SIZEOF_INT 2
#endif

/******************************************************************************/
/* 64-bit operations. */
#if (LG_SIZEOF_PTR == 3 || LG_SIZEOF_INT == 3)
#  ifdef __GCC_HAVE_SYNC_COMPARE_AND_SWAP_8
ATOMIC_INLINE uint64_t
atomic_add_uint64(uint64_t *p, uint64_t x)
{
	return (__sync_add_and_fetch(p, x));
}

ATOMIC_INLINE uint64_t
atomic_sub_uint64(uint64_t *p, uint64_t x)
{
	return (__sync_sub_and_fetch(p, x));
}
#elif (defined(_MSC_VER))
ATOMIC_INLINE uint64_t
atomic_add_uint64(uint64_t *p, uint64_t x)
{
	return (InterlockedExchangeAdd64(p, x));
}

ATOMIC_INLINE uint64_t
atomic_sub_uint64(uint64_t *p, uint64_t x)
{
	return (InterlockedExchangeAdd64(p, -((int64_t)x)));
}
#elif (defined(__APPLE__))
ATOMIC_INLINE uint64_t
atomic_add_uint64(uint64_t *p, uint64_t x)
{
	return (uint64_t)(OSAtomicAdd64((int64_t)x, (int64_t *)p));
}

ATOMIC_INLINE uint64_t
atomic_sub_uint64(uint64_t *p, uint64_t x)
{
	return (uint64_t)(OSAtomicAdd64(-((int64_t)x), (int64_t *)p));
}
#  elif (defined(__amd64__) || defined(__x86_64__))
ATOMIC_INLINE uint64_t
atomic_add_uint64(uint64_t *p, uint64_t x)
{
	asm volatile (
	    "lock; xaddq %0, %1;"
	    : "+r" (x), "=m" (*p) /* Outputs. */
	    : "m" (*p) /* Inputs. */
	    );
	return (x);
}

ATOMIC_INLINE uint64_t
atomic_sub_uint64(uint64_t *p, uint64_t x)
{
	x = (uint64_t)(-(int64_t)x);
	asm volatile (
	    "lock; xaddq %0, %1;"
	    : "+r" (x), "=m" (*p) /* Outputs. */
	    : "m" (*p) /* Inputs. */
	    );
	return (x);
}
#  elif (defined(JEMALLOC_ATOMIC9))
ATOMIC_INLINE uint64_t
atomic_add_uint64(uint64_t *p, uint64_t x)
{
	/*
	 * atomic_fetchadd_64() doesn't exist, but we only ever use this
	 * function on LP64 systems, so atomic_fetchadd_long() will do.
	 */
	assert(sizeof(uint64_t) == sizeof(unsigned long));

	return (atomic_fetchadd_long(p, (unsigned long)x) + x);
}

ATOMIC_INLINE uint64_t
atomic_sub_uint64(uint64_t *p, uint64_t x)
{
	assert(sizeof(uint64_t) == sizeof(unsigned long));

	return (atomic_fetchadd_long(p, (unsigned long)(-(long)x)) - x);
}
#  elif (defined(JE_FORCE_SYNC_COMPARE_AND_SWAP_8))
ATOMIC_INLINE uint64_t
atomic_add_uint64(uint64_t *p, uint64_t x)
{
	return (__sync_add_and_fetch(p, x));
}

ATOMIC_INLINE uint64_t
atomic_sub_uint64(uint64_t *p, uint64_t x)
{
	return (__sync_sub_and_fetch(p, x));
}
#  else
#    error "Missing implementation for 64-bit atomic operations"
#  endif
#endif

/******************************************************************************/
/* 32-bit operations. */
#ifdef __GCC_HAVE_SYNC_COMPARE_AND_SWAP_4
ATOMIC_INLINE uint32_t
atomic_add_uint32(uint32_t *p, uint32_t x)
{
	return (__sync_add_and_fetch(p, x));
}

ATOMIC_INLINE uint32_t
atomic_sub_uint32(uint32_t *p, uint32_t x)
{
	return (__sync_sub_and_fetch(p, x));
}
#elif (defined(_MSC_VER))
ATOMIC_INLINE uint32_t
atomic_add_uint32(uint32_t *p, uint32_t x)
{
	return (InterlockedExchangeAdd(p, x));
}

ATOMIC_INLINE uint32_t
atomic_sub_uint32(uint32_t *p, uint32_t x)
{
	return (InterlockedExchangeAdd(p, -((int32_t)x)));
}
#elif (defined(__APPLE__))
ATOMIC_INLINE uint32_t
atomic_add_uint32(uint32_t *p, uint32_t x)
{
	return (uint32_t)(OSAtomicAdd32((int32_t)x, (int32_t *)p));
}

ATOMIC_INLINE uint32_t
atomic_sub_uint32(uint32_t *p, uint32_t x)
{
	return (uint32_t)(OSAtomicAdd32(-((int32_t)x), (int32_t *)p));
}
#elif (defined(__i386__) || defined(__amd64__) || defined(__x86_64__))
ATOMIC_INLINE uint32_t
atomic_add_uint32(uint32_t *p, uint32_t x)
{
	asm volatile (
	    "lock; xaddl %0, %1;"
	    : "+r" (x), "=m" (*p) /* Outputs. */
	    : "m" (*p) /* Inputs. */
	    );
	return (x);
}

ATOMIC_INLINE uint32_t
atomic_sub_uint32(uint32_t *p, uint32_t x)
{
	x = (uint32_t)(-(int32_t)x);
	asm volatile (
	    "lock; xaddl %0, %1;"
	    : "+r" (x), "=m" (*p) /* Outputs. */
	    : "m" (*p) /* Inputs. */
	    );
	return (x);
}
#elif (defined(JEMALLOC_ATOMIC9))
ATOMIC_INLINE uint32_t
atomic_add_uint32(uint32_t *p, uint32_t x)
{
	return (atomic_fetchadd_32(p, x) + x);
}

ATOMIC_INLINE uint32_t
atomic_sub_uint32(uint32_t *p, uint32_t x)
{
	return (atomic_fetchadd_32(p, (uint32_t)(-(int32_t)x)) - x);
}
#elif (defined(JE_FORCE_SYNC_COMPARE_AND_SWAP_4))
ATOMIC_INLINE uint32_t
atomic_add_uint32(uint32_t *p, uint32_t x)
{
	return (__sync_add_and_fetch(p, x));
}

ATOMIC_INLINE uint32_t
atomic_sub_uint32(uint32_t *p, uint32_t x)
{
	return (__sync_sub_and_fetch(p, x));
}
#else
#  error "Missing implementation for 32-bit atomic operations"
#endif

/******************************************************************************/
/* size_t operations. */
ATOMIC_INLINE size_t
atomic_add_z(size_t *p, size_t x)
{
	assert(sizeof(size_t) == 1 << LG_SIZEOF_PTR);

#if (LG_SIZEOF_PTR == 3)
	return ((size_t)atomic_add_uint64((uint64_t *)p, (uint64_t)x));
#elif (LG_SIZEOF_PTR == 2)
	return ((size_t)atomic_add_uint32((uint32_t *)p, (uint32_t)x));
#endif
}

ATOMIC_INLINE size_t
atomic_sub_z(size_t *p, size_t x)
{
	assert(sizeof(size_t) == 1 << LG_SIZEOF_PTR);

#if (LG_SIZEOF_PTR == 3)
	return ((size_t)atomic_add_uint64((uint64_t *)p,
	    (uint64_t)-((int64_t)x)));
#elif (LG_SIZEOF_PTR == 2)
	return ((size_t)atomic_add_uint32((uint32_t *)p,
	    (uint32_t)-((int32_t)x)));
#endif
}

/******************************************************************************/
/* unsigned operations. */
ATOMIC_INLINE unsigned
atomic_add_u(unsigned *p, unsigned x)
{
	assert(sizeof(unsigned) == 1 << LG_SIZEOF_INT);

#if (LG_SIZEOF_INT == 3)
	return ((unsigned)atomic_add_uint64((uint64_t *)p, (uint64_t)x));
#elif (LG_SIZEOF_INT == 2)
	return ((unsigned)atomic_add_uint32((uint32_t *)p, (uint32_t)x));
#endif
}

ATOMIC_INLINE unsigned
atomic_sub_u(unsigned *p, unsigned x)
{
	assert(sizeof(unsigned) == 1 << LG_SIZEOF_INT);

#if (LG_SIZEOF_INT == 3)
	return ((unsigned)atomic_add_uint64((uint64_t *)p,
	    (uint64_t)-((int64_t)x)));
#elif (LG_SIZEOF_INT == 2)
	return ((unsigned)atomic_add_uint32((uint32_t *)p,
	    (uint32_t)-((int32_t)x)));
#endif
}

#endif /* __ATOMIC_OPS_H__ */
