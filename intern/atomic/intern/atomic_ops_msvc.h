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

#ifndef __ATOMIC_OPS_MSVC_H__
#define __ATOMIC_OPS_MSVC_H__

#include "atomic_ops_utils.h"

#define NOGDI
#ifndef NOMINMAX
#  define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN

#include <intrin.h>
#include <windows.h>

#if defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wincompatible-pointer-types"
#endif

/* 64-bit operations. */
#if (LG_SIZEOF_PTR == 8 || LG_SIZEOF_INT == 8)
/* Unsigned */
ATOMIC_INLINE uint64_t atomic_add_and_fetch_uint64(uint64_t *p, uint64_t x)
{
  return InterlockedExchangeAdd64((int64_t *)p, (int64_t)x) + x;
}

ATOMIC_INLINE uint64_t atomic_sub_and_fetch_uint64(uint64_t *p, uint64_t x)
{
  return InterlockedExchangeAdd64((int64_t *)p, -((int64_t)x)) - x;
}

ATOMIC_INLINE uint64_t atomic_cas_uint64(uint64_t *v, uint64_t old, uint64_t _new)
{
  return InterlockedCompareExchange64((int64_t *)v, _new, old);
}

ATOMIC_INLINE uint64_t atomic_fetch_and_add_uint64(uint64_t *p, uint64_t x)
{
  return InterlockedExchangeAdd64((int64_t *)p, (int64_t)x);
}

ATOMIC_INLINE uint64_t atomic_fetch_and_sub_uint64(uint64_t *p, uint64_t x)
{
  return InterlockedExchangeAdd64((int64_t *)p, -((int64_t)x));
}

/* Signed */
ATOMIC_INLINE int64_t atomic_add_and_fetch_int64(int64_t *p, int64_t x)
{
  return InterlockedExchangeAdd64(p, x) + x;
}

ATOMIC_INLINE int64_t atomic_sub_and_fetch_int64(int64_t *p, int64_t x)
{
  return InterlockedExchangeAdd64(p, -x) - x;
}

ATOMIC_INLINE int64_t atomic_cas_int64(int64_t *v, int64_t old, int64_t _new)
{
  return InterlockedCompareExchange64(v, _new, old);
}

ATOMIC_INLINE int64_t atomic_fetch_and_add_int64(int64_t *p, int64_t x)
{
  return InterlockedExchangeAdd64(p, x);
}

ATOMIC_INLINE int64_t atomic_fetch_and_sub_int64(int64_t *p, int64_t x)
{
  return InterlockedExchangeAdd64(p, -x);
}
#endif

/******************************************************************************/
/* 32-bit operations. */
/* Unsigned */
ATOMIC_INLINE uint32_t atomic_add_and_fetch_uint32(uint32_t *p, uint32_t x)
{
  return InterlockedExchangeAdd(p, x) + x;
}

ATOMIC_INLINE uint32_t atomic_sub_and_fetch_uint32(uint32_t *p, uint32_t x)
{
  return InterlockedExchangeAdd(p, -((int32_t)x)) - x;
}

ATOMIC_INLINE uint32_t atomic_cas_uint32(uint32_t *v, uint32_t old, uint32_t _new)
{
  return InterlockedCompareExchange((long *)v, _new, old);
}

ATOMIC_INLINE uint32_t atomic_fetch_and_add_uint32(uint32_t *p, uint32_t x)
{
  return InterlockedExchangeAdd(p, x);
}

ATOMIC_INLINE uint32_t atomic_fetch_and_or_uint32(uint32_t *p, uint32_t x)
{
  return InterlockedOr((long *)p, x);
}

ATOMIC_INLINE uint32_t atomic_fetch_and_and_uint32(uint32_t *p, uint32_t x)
{
  return InterlockedAnd((long *)p, x);
}

/* Signed */
ATOMIC_INLINE int32_t atomic_add_and_fetch_int32(int32_t *p, int32_t x)
{
  return InterlockedExchangeAdd((long *)p, x) + x;
}

ATOMIC_INLINE int32_t atomic_sub_and_fetch_int32(int32_t *p, int32_t x)
{
  return InterlockedExchangeAdd((long *)p, -x) - x;
}

ATOMIC_INLINE int32_t atomic_cas_int32(int32_t *v, int32_t old, int32_t _new)
{
  return InterlockedCompareExchange((long *)v, _new, old);
}

ATOMIC_INLINE int32_t atomic_fetch_and_add_int32(int32_t *p, int32_t x)
{
  return InterlockedExchangeAdd((long *)p, x);
}

ATOMIC_INLINE int32_t atomic_fetch_and_or_int32(int32_t *p, int32_t x)
{
  return InterlockedOr((long *)p, x);
}

ATOMIC_INLINE int32_t atomic_fetch_and_and_int32(int32_t *p, int32_t x)
{
  return InterlockedAnd((long *)p, x);
}

/******************************************************************************/
/* 8-bit operations. */

/* Unsigned */
#pragma intrinsic(_InterlockedAnd8)
ATOMIC_INLINE uint8_t atomic_fetch_and_and_uint8(uint8_t *p, uint8_t b)
{
#if (LG_SIZEOF_PTR == 8 || LG_SIZEOF_INT == 8)
  return InterlockedAnd8((char *)p, (char)b);
#else
  return _InterlockedAnd8((char *)p, (char)b);
#endif
}

#pragma intrinsic(_InterlockedOr8)
ATOMIC_INLINE uint8_t atomic_fetch_and_or_uint8(uint8_t *p, uint8_t b)
{
#if (LG_SIZEOF_PTR == 8 || LG_SIZEOF_INT == 8)
  return InterlockedOr8((char *)p, (char)b);
#else
  return _InterlockedOr8((char *)p, (char)b);
#endif
}

/* Signed */
#pragma intrinsic(_InterlockedAnd8)
ATOMIC_INLINE int8_t atomic_fetch_and_and_int8(int8_t *p, int8_t b)
{
#if (LG_SIZEOF_PTR == 8 || LG_SIZEOF_INT == 8)
  return InterlockedAnd8((char *)p, (char)b);
#else
  return _InterlockedAnd8((char *)p, (char)b);
#endif
}

#pragma intrinsic(_InterlockedOr8)
ATOMIC_INLINE int8_t atomic_fetch_and_or_int8(int8_t *p, int8_t b)
{
#if (LG_SIZEOF_PTR == 8 || LG_SIZEOF_INT == 8)
  return InterlockedOr8((char *)p, (char)b);
#else
  return _InterlockedOr8((char *)p, (char)b);
#endif
}

#if defined(__clang__)
#  pragma GCC diagnostic pop
#endif

#endif /* __ATOMIC_OPS_MSVC_H__ */
