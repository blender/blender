/*
 * Original code from jemalloc with this license:
 *
 * Copyright (C) 2002-2013 Jason Evans <jasone@canonware.com>.
 * All rights reserved.
 * Copyright (C) 2007-2012 Mozilla Foundation.  All rights reserved.
 * Copyright (C) 2009-2013 Facebook, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice(s),
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice(s),
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
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
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: adapted from jemalloc.
 */

#ifndef __ATOMIC_OPS_UNIX_H__
#define __ATOMIC_OPS_UNIX_H__

#include "atomic_ops_utils.h"

#if defined(__arm__)
/* Attempt to fix compilation error on Debian armel kernel.
 * arm7 architecture does have both 32 and 64bit atomics, however
 * its gcc doesn't have __GCC_HAVE_SYNC_COMPARE_AND_SWAP_n defined.
 */
#  define JE_FORCE_SYNC_COMPARE_AND_SWAP_1
#  define JE_FORCE_SYNC_COMPARE_AND_SWAP_4
#  define JE_FORCE_SYNC_COMPARE_AND_SWAP_8
#endif

/******************************************************************************/
/* 64-bit operations. */
#if (defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_8) || defined(JE_FORCE_SYNC_COMPARE_AND_SWAP_8))
/* Unsigned */
ATOMIC_INLINE uint64_t atomic_add_and_fetch_uint64(uint64_t *p, uint64_t x)
{
  return __sync_add_and_fetch(p, x);
}

ATOMIC_INLINE uint64_t atomic_sub_and_fetch_uint64(uint64_t *p, uint64_t x)
{
  return __sync_sub_and_fetch(p, x);
}

ATOMIC_INLINE uint64_t atomic_fetch_and_add_uint64(uint64_t *p, uint64_t x)
{
  return __sync_fetch_and_add(p, x);
}

ATOMIC_INLINE uint64_t atomic_fetch_and_sub_uint64(uint64_t *p, uint64_t x)
{
  return __sync_fetch_and_sub(p, x);
}

ATOMIC_INLINE uint64_t atomic_cas_uint64(uint64_t *v, uint64_t old, uint64_t _new)
{
  return __sync_val_compare_and_swap(v, old, _new);
}

/* Signed */
ATOMIC_INLINE int64_t atomic_add_and_fetch_int64(int64_t *p, int64_t x)
{
  return __sync_add_and_fetch(p, x);
}

ATOMIC_INLINE int64_t atomic_sub_and_fetch_int64(int64_t *p, int64_t x)
{
  return __sync_sub_and_fetch(p, x);
}

ATOMIC_INLINE int64_t atomic_fetch_and_add_int64(int64_t *p, int64_t x)
{
  return __sync_fetch_and_add(p, x);
}

ATOMIC_INLINE int64_t atomic_fetch_and_sub_int64(int64_t *p, int64_t x)
{
  return __sync_fetch_and_sub(p, x);
}

ATOMIC_INLINE int64_t atomic_cas_int64(int64_t *v, int64_t old, int64_t _new)
{
  return __sync_val_compare_and_swap(v, old, _new);
}

#elif (defined(__amd64__) || defined(__x86_64__))
/* Unsigned */
ATOMIC_INLINE uint64_t atomic_fetch_and_add_uint64(uint64_t *p, uint64_t x)
{
  asm volatile("lock; xaddq %0, %1;"
               : "+r"(x), "=m"(*p) /* Outputs. */
               : "m"(*p)           /* Inputs. */
  );
  return x;
}

ATOMIC_INLINE uint64_t atomic_fetch_and_sub_uint64(uint64_t *p, uint64_t x)
{
  x = (uint64_t)(-(int64_t)x);
  asm volatile("lock; xaddq %0, %1;"
               : "+r"(x), "=m"(*p) /* Outputs. */
               : "m"(*p)           /* Inputs. */
  );
  return x;
}

ATOMIC_INLINE uint64_t atomic_add_and_fetch_uint64(uint64_t *p, uint64_t x)
{
  return atomic_fetch_and_add_uint64(p, x) + x;
}

ATOMIC_INLINE uint64_t atomic_sub_and_fetch_uint64(uint64_t *p, uint64_t x)
{
  return atomic_fetch_and_sub_uint64(p, x) - x;
}

ATOMIC_INLINE uint64_t atomic_cas_uint64(uint64_t *v, uint64_t old, uint64_t _new)
{
  uint64_t ret;
  asm volatile("lock; cmpxchgq %2,%1" : "=a"(ret), "+m"(*v) : "r"(_new), "0"(old) : "memory");
  return ret;
}

/* Signed */
ATOMIC_INLINE int64_t atomic_fetch_and_add_int64(int64_t *p, int64_t x)
{
  asm volatile("lock; xaddq %0, %1;"
               : "+r"(x), "=m"(*p) /* Outputs. */
               : "m"(*p)           /* Inputs. */
  );
  return x;
}

ATOMIC_INLINE int64_t atomic_fetch_and_sub_int64(int64_t *p, int64_t x)
{
  x = -x;
  asm volatile("lock; xaddq %0, %1;"
               : "+r"(x), "=m"(*p) /* Outputs. */
               : "m"(*p)           /* Inputs. */
  );
  return x;
}

ATOMIC_INLINE int64_t atomic_add_and_fetch_int64(int64_t *p, int64_t x)
{
  return atomic_fetch_and_add_int64(p, x) + x;
}

ATOMIC_INLINE int64_t atomic_sub_and_fetch_int64(int64_t *p, int64_t x)
{
  return atomic_fetch_and_sub_int64(p, x) - x;
}

ATOMIC_INLINE int64_t atomic_cas_int64(int64_t *v, int64_t old, int64_t _new)
{
  int64_t ret;
  asm volatile("lock; cmpxchgq %2,%1" : "=a"(ret), "+m"(*v) : "r"(_new), "0"(old) : "memory");
  return ret;
}
#else
#  error "Missing implementation for 64-bit atomic operations"
#endif

/******************************************************************************/
/* 32-bit operations. */
#if (defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4) || defined(JE_FORCE_SYNC_COMPARE_AND_SWAP_4))
/* Unsigned */
ATOMIC_INLINE uint32_t atomic_add_and_fetch_uint32(uint32_t *p, uint32_t x)
{
  return __sync_add_and_fetch(p, x);
}

ATOMIC_INLINE uint32_t atomic_sub_and_fetch_uint32(uint32_t *p, uint32_t x)
{
  return __sync_sub_and_fetch(p, x);
}

ATOMIC_INLINE uint32_t atomic_cas_uint32(uint32_t *v, uint32_t old, uint32_t _new)
{
  return __sync_val_compare_and_swap(v, old, _new);
}

/* Signed */
ATOMIC_INLINE int32_t atomic_add_and_fetch_int32(int32_t *p, int32_t x)
{
  return __sync_add_and_fetch(p, x);
}

ATOMIC_INLINE int32_t atomic_sub_and_fetch_int32(int32_t *p, int32_t x)
{
  return __sync_sub_and_fetch(p, x);
}

ATOMIC_INLINE int32_t atomic_cas_int32(int32_t *v, int32_t old, int32_t _new)
{
  return __sync_val_compare_and_swap(v, old, _new);
}

#elif (defined(__i386__) || defined(__amd64__) || defined(__x86_64__))
/* Unsigned */
ATOMIC_INLINE uint32_t atomic_add_and_fetch_uint32(uint32_t *p, uint32_t x)
{
  uint32_t ret = x;
  asm volatile("lock; xaddl %0, %1;"
               : "+r"(ret), "=m"(*p) /* Outputs. */
               : "m"(*p)             /* Inputs. */
  );
  return ret + x;
}

ATOMIC_INLINE uint32_t atomic_sub_and_fetch_uint32(uint32_t *p, uint32_t x)
{
  uint32_t ret = (uint32_t)(-(int32_t)x);
  asm volatile("lock; xaddl %0, %1;"
               : "+r"(ret), "=m"(*p) /* Outputs. */
               : "m"(*p)             /* Inputs. */
  );
  return ret - x;
}

ATOMIC_INLINE uint32_t atomic_cas_uint32(uint32_t *v, uint32_t old, uint32_t _new)
{
  uint32_t ret;
  asm volatile("lock; cmpxchgl %2,%1" : "=a"(ret), "+m"(*v) : "r"(_new), "0"(old) : "memory");
  return ret;
}

/* Signed */
ATOMIC_INLINE int32_t atomic_add_and_fetch_int32(int32_t *p, int32_t x)
{
  int32_t ret = x;
  asm volatile("lock; xaddl %0, %1;"
               : "+r"(ret), "=m"(*p) /* Outputs. */
               : "m"(*p)             /* Inputs. */
  );
  return ret + x;
}

ATOMIC_INLINE int32_t atomic_sub_and_fetch_int32(int32_t *p, int32_t x)
{
  int32_t ret = -x;
  asm volatile("lock; xaddl %0, %1;"
               : "+r"(ret), "=m"(*p) /* Outputs. */
               : "m"(*p)             /* Inputs. */
  );
  return ret - x;
}

ATOMIC_INLINE int32_t atomic_cas_int32(int32_t *v, int32_t old, int32_t _new)
{
  int32_t ret;
  asm volatile("lock; cmpxchgl %2,%1" : "=a"(ret), "+m"(*v) : "r"(_new), "0"(old) : "memory");
  return ret;
}

#else
#  error "Missing implementation for 32-bit atomic operations"
#endif

#if (defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4) || defined(JE_FORCE_SYNC_COMPARE_AND_SWAP_4))
/* Unsigned */
ATOMIC_INLINE uint32_t atomic_fetch_and_add_uint32(uint32_t *p, uint32_t x)
{
  return __sync_fetch_and_add(p, x);
}

ATOMIC_INLINE uint32_t atomic_fetch_and_or_uint32(uint32_t *p, uint32_t x)
{
  return __sync_fetch_and_or(p, x);
}

ATOMIC_INLINE uint32_t atomic_fetch_and_and_uint32(uint32_t *p, uint32_t x)
{
  return __sync_fetch_and_and(p, x);
}

/* Signed */
ATOMIC_INLINE int32_t atomic_fetch_and_add_int32(int32_t *p, int32_t x)
{
  return __sync_fetch_and_add(p, x);
}

ATOMIC_INLINE int32_t atomic_fetch_and_or_int32(int32_t *p, int32_t x)
{
  return __sync_fetch_and_or(p, x);
}

ATOMIC_INLINE int32_t atomic_fetch_and_and_int32(int32_t *p, int32_t x)
{
  return __sync_fetch_and_and(p, x);
}

#else
#  error "Missing implementation for 32-bit atomic operations"
#endif

/******************************************************************************/
/* 8-bit operations. */
#if (defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_1) || defined(JE_FORCE_SYNC_COMPARE_AND_SWAP_1))
/* Unsigned */
ATOMIC_INLINE uint8_t atomic_fetch_and_and_uint8(uint8_t *p, uint8_t b)
{
  return __sync_fetch_and_and(p, b);
}
ATOMIC_INLINE uint8_t atomic_fetch_and_or_uint8(uint8_t *p, uint8_t b)
{
  return __sync_fetch_and_or(p, b);
}

/* Signed */
ATOMIC_INLINE int8_t atomic_fetch_and_and_int8(int8_t *p, int8_t b)
{
  return __sync_fetch_and_and(p, b);
}
ATOMIC_INLINE int8_t atomic_fetch_and_or_int8(int8_t *p, int8_t b)
{
  return __sync_fetch_and_or(p, b);
}

#else
#  error "Missing implementation for 8-bit atomic operations"
#endif

#endif /* __ATOMIC_OPS_UNIX_H__ */
