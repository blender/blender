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
 * The Original Code is adapted from jemalloc.
 * Modifications Copyright (C) 2016 Blender Foundation
 */

/** \file
 * \ingroup intern_atomic
 */

#ifndef __ATOMIC_OPS_UNIX_H__
#define __ATOMIC_OPS_UNIX_H__

#include "atomic_ops_utils.h"

#if defined(__arm__) || defined(__riscv)
/* Attempt to fix compilation error on Debian armel and RISC-V kernels.
 * Both architectures do have both 32 and 64bit atomics, however
 * its gcc doesn't have __GCC_HAVE_SYNC_COMPARE_AND_SWAP_n defined.
 */
#  define JE_FORCE_SYNC_COMPARE_AND_SWAP_1
#  define JE_FORCE_SYNC_COMPARE_AND_SWAP_2
#  define JE_FORCE_SYNC_COMPARE_AND_SWAP_4
#  define JE_FORCE_SYNC_COMPARE_AND_SWAP_8
#endif

/* Define the `ATOMIC_FORCE_USE_FALLBACK` to force lock-based fallback implementation to be used
 * (even on platforms where there is native implementation available via compiler.
 * Useful for development purposes. */
#undef ATOMIC_FORCE_USE_FALLBACK

/* -------------------------------------------------------------------- */
/** \name Spin-lock implementation
 *
 * Used to implement atomics on unsupported platforms.
 * The spin implementation is shared for all platforms to make sure it compiles and tested.
 * \{ */

typedef struct AtomicSpinLock {
  volatile int lock;

  /* Pad the structure size to a cache-line, to avoid unwanted sharing with other data. */
  int pad[32 - sizeof(int)];
} __attribute__((aligned(32))) AtomicSpinLock;

ATOMIC_INLINE void atomic_spin_lock(volatile AtomicSpinLock *lock)
{
  while (__sync_lock_test_and_set(&lock->lock, 1)) {
    while (lock->lock) {
    }
  }
}

ATOMIC_INLINE void atomic_spin_unlock(volatile AtomicSpinLock *lock)
{
  __sync_lock_release(&lock->lock);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Common part of x64 implementation
 * \{ */

/* TODO(sergey): On x64 platform both read and write of a variable aligned to its type size is
 * atomic, so in theory it is possible to avoid memory barrier and gain performance. The downside
 * of that would be that it will impose requirement to value which is being operated on. */
#define __atomic_impl_load_generic(v) (__sync_synchronize(), *(v))
#define __atomic_impl_store_generic(p, v) \
  do { \
    *(p) = (v); \
    __sync_synchronize(); \
  } while (0)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Common part of locking fallback implementation
 * \{ */

/* Global lock, shared by all atomic operations implementations.
 *
 * Could be split into per-size locks, although added complexity and being more error-proone does
 * not seem to worth it for a fall-back implementation. */
static _ATOMIC_MAYBE_UNUSED AtomicSpinLock _atomic_global_lock = {0};

#define ATOMIC_LOCKING_OP_AND_FETCH_DEFINE(_type, _op_name, _op) \
  ATOMIC_INLINE _type##_t atomic_##_op_name##_and_fetch_##_type(_type##_t *p, _type##_t x) \
  { \
    atomic_spin_lock(&_atomic_global_lock); \
    const _type##_t original_value = *(p); \
    const _type##_t new_value = original_value _op(x); \
    *(p) = new_value; \
    atomic_spin_unlock(&_atomic_global_lock); \
    return new_value; \
  }

#define ATOMIC_LOCKING_FETCH_AND_OP_DEFINE(_type, _op_name, _op) \
  ATOMIC_INLINE _type##_t atomic_fetch_and_##_op_name##_##_type(_type##_t *p, _type##_t x) \
  { \
    atomic_spin_lock(&_atomic_global_lock); \
    const _type##_t original_value = *(p); \
    *(p) = original_value _op(x); \
    atomic_spin_unlock(&_atomic_global_lock); \
    return original_value; \
  }

#define ATOMIC_LOCKING_ADD_AND_FETCH_DEFINE(_type) \
  ATOMIC_LOCKING_OP_AND_FETCH_DEFINE(_type, add, +)

#define ATOMIC_LOCKING_SUB_AND_FETCH_DEFINE(_type) \
  ATOMIC_LOCKING_OP_AND_FETCH_DEFINE(_type, sub, -)

#define ATOMIC_LOCKING_FETCH_AND_ADD_DEFINE(_type) \
  ATOMIC_LOCKING_FETCH_AND_OP_DEFINE(_type, add, +)

#define ATOMIC_LOCKING_FETCH_AND_SUB_DEFINE(_type) \
  ATOMIC_LOCKING_FETCH_AND_OP_DEFINE(_type, sub, -)

#define ATOMIC_LOCKING_FETCH_AND_OR_DEFINE(_type) ATOMIC_LOCKING_FETCH_AND_OP_DEFINE(_type, or, |)

#define ATOMIC_LOCKING_FETCH_AND_AND_DEFINE(_type) \
  ATOMIC_LOCKING_FETCH_AND_OP_DEFINE(_type, and, &)

#define ATOMIC_LOCKING_CAS_DEFINE(_type) \
  ATOMIC_INLINE _type##_t atomic_cas_##_type(_type##_t *v, _type##_t old, _type##_t _new) \
  { \
    atomic_spin_lock(&_atomic_global_lock); \
    const _type##_t original_value = *v; \
    if (*v == old) { \
      *v = _new; \
    } \
    atomic_spin_unlock(&_atomic_global_lock); \
    return original_value; \
  }

#define ATOMIC_LOCKING_LOAD_DEFINE(_type) \
  ATOMIC_INLINE _type##_t atomic_load_##_type(const _type##_t *v) \
  { \
    atomic_spin_lock(&_atomic_global_lock); \
    const _type##_t value = *v; \
    atomic_spin_unlock(&_atomic_global_lock); \
    return value; \
  }

#define ATOMIC_LOCKING_STORE_DEFINE(_type) \
  ATOMIC_INLINE void atomic_store_##_type(_type##_t *p, const _type##_t v) \
  { \
    atomic_spin_lock(&_atomic_global_lock); \
    *p = v; \
    atomic_spin_unlock(&_atomic_global_lock); \
  }

/** \} */

/* -------------------------------------------------------------------- */
/** \name 64-bit operations
 * \{ */

#if !defined(ATOMIC_FORCE_USE_FALLBACK) && \
    (defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_8) || defined(JE_FORCE_SYNC_COMPARE_AND_SWAP_8))
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

ATOMIC_INLINE uint64_t atomic_load_uint64(const uint64_t *v)
{
  return __atomic_load_n(v, __ATOMIC_SEQ_CST);
}

ATOMIC_INLINE void atomic_store_uint64(uint64_t *p, uint64_t v)
{
  __atomic_store(p, &v, __ATOMIC_SEQ_CST);
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

ATOMIC_INLINE int64_t atomic_load_int64(const int64_t *v)
{
  return __atomic_load_n(v, __ATOMIC_SEQ_CST);
}

ATOMIC_INLINE void atomic_store_int64(int64_t *p, int64_t v)
{
  __atomic_store(p, &v, __ATOMIC_SEQ_CST);
}

#elif !defined(ATOMIC_FORCE_USE_FALLBACK) && (defined(__amd64__) || defined(__x86_64__))
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

ATOMIC_INLINE uint64_t atomic_load_uint64(const uint64_t *v)
{
  return __atomic_impl_load_generic(v);
}

ATOMIC_INLINE void atomic_store_uint64(uint64_t *p, uint64_t v)
{
  __atomic_impl_store_generic(p, v);
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

ATOMIC_INLINE int64_t atomic_load_int64(const int64_t *v)
{
  return __atomic_impl_load_generic(v);
}

ATOMIC_INLINE void atomic_store_int64(int64_t *p, int64_t v)
{
  __atomic_impl_store_generic(p, v);
}

#else

/* Unsigned */

ATOMIC_LOCKING_ADD_AND_FETCH_DEFINE(uint64)
ATOMIC_LOCKING_SUB_AND_FETCH_DEFINE(uint64)

ATOMIC_LOCKING_FETCH_AND_ADD_DEFINE(uint64)
ATOMIC_LOCKING_FETCH_AND_SUB_DEFINE(uint64)

ATOMIC_LOCKING_CAS_DEFINE(uint64)

ATOMIC_LOCKING_LOAD_DEFINE(uint64)
ATOMIC_LOCKING_STORE_DEFINE(uint64)

/* Signed */
ATOMIC_LOCKING_ADD_AND_FETCH_DEFINE(int64)
ATOMIC_LOCKING_SUB_AND_FETCH_DEFINE(int64)

ATOMIC_LOCKING_FETCH_AND_ADD_DEFINE(int64)
ATOMIC_LOCKING_FETCH_AND_SUB_DEFINE(int64)

ATOMIC_LOCKING_CAS_DEFINE(int64)

ATOMIC_LOCKING_LOAD_DEFINE(int64)
ATOMIC_LOCKING_STORE_DEFINE(int64)

#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name 32-bit operations
 * \{ */

#if !defined(ATOMIC_FORCE_USE_FALLBACK) && \
    (defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4) || defined(JE_FORCE_SYNC_COMPARE_AND_SWAP_4))
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

ATOMIC_INLINE uint32_t atomic_load_uint32(const uint32_t *v)
{
  return __atomic_load_n(v, __ATOMIC_SEQ_CST);
}

ATOMIC_INLINE void atomic_store_uint32(uint32_t *p, uint32_t v)
{
  __atomic_store(p, &v, __ATOMIC_SEQ_CST);
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

ATOMIC_INLINE int32_t atomic_load_int32(const int32_t *v)
{
  return __atomic_load_n(v, __ATOMIC_SEQ_CST);
}

ATOMIC_INLINE void atomic_store_int32(int32_t *p, int32_t v)
{
  __atomic_store(p, &v, __ATOMIC_SEQ_CST);
}

#elif !defined(ATOMIC_FORCE_USE_FALLBACK) && \
    (defined(__i386__) || defined(__amd64__) || defined(__x86_64__))
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

ATOMIC_INLINE uint32_t atomic_load_uint32(const uint32_t *v)
{
  return __atomic_load_n(v, __ATOMIC_SEQ_CST);
}

ATOMIC_INLINE void atomic_store_uint32(uint32_t *p, uint32_t v)
{
  __atomic_store(p, &v, __ATOMIC_SEQ_CST);
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

ATOMIC_INLINE int32_t atomic_load_int32(const int32_t *v)
{
  return __atomic_load_n(v, __ATOMIC_SEQ_CST);
}

ATOMIC_INLINE void atomic_store_int32(int32_t *p, int32_t v)
{
  __atomic_store(p, &v, __ATOMIC_SEQ_CST);
}

#else

/* Unsigned */

ATOMIC_LOCKING_ADD_AND_FETCH_DEFINE(uint32)
ATOMIC_LOCKING_SUB_AND_FETCH_DEFINE(uint32)

ATOMIC_LOCKING_CAS_DEFINE(uint32)

ATOMIC_LOCKING_LOAD_DEFINE(uint32)
ATOMIC_LOCKING_STORE_DEFINE(uint32)

/* Signed */

ATOMIC_LOCKING_ADD_AND_FETCH_DEFINE(int32)
ATOMIC_LOCKING_SUB_AND_FETCH_DEFINE(int32)

ATOMIC_LOCKING_CAS_DEFINE(int32)

ATOMIC_LOCKING_LOAD_DEFINE(int32)
ATOMIC_LOCKING_STORE_DEFINE(int32)

#endif

#if !defined(ATOMIC_FORCE_USE_FALLBACK) && \
    (defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4) || defined(JE_FORCE_SYNC_COMPARE_AND_SWAP_4))
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

/* Unsigned */
ATOMIC_LOCKING_FETCH_AND_ADD_DEFINE(uint32)
ATOMIC_LOCKING_FETCH_AND_OR_DEFINE(uint32)
ATOMIC_LOCKING_FETCH_AND_AND_DEFINE(uint32)

/* Signed */
ATOMIC_LOCKING_FETCH_AND_ADD_DEFINE(int32)
ATOMIC_LOCKING_FETCH_AND_OR_DEFINE(int32)
ATOMIC_LOCKING_FETCH_AND_AND_DEFINE(int32)

#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name 16-bit operations
 * \{ */

#if !defined(ATOMIC_FORCE_USE_FALLBACK) && \
    (defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_2) || defined(JE_FORCE_SYNC_COMPARE_AND_SWAP_2))

/* Signed */
ATOMIC_INLINE int16_t atomic_fetch_and_and_int16(int16_t *p, int16_t b)
{
  return __sync_fetch_and_and(p, b);
}
ATOMIC_INLINE int16_t atomic_fetch_and_or_int16(int16_t *p, int16_t b)
{
  return __sync_fetch_and_or(p, b);
}

#else

ATOMIC_LOCKING_FETCH_AND_AND_DEFINE(int16)
ATOMIC_LOCKING_FETCH_AND_OR_DEFINE(int16)

#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name 8-bit operations
 * \{ */

#if !defined(ATOMIC_FORCE_USE_FALLBACK) && \
    (defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_1) || defined(JE_FORCE_SYNC_COMPARE_AND_SWAP_1))

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

/* Unsigned */
ATOMIC_LOCKING_FETCH_AND_AND_DEFINE(uint8)
ATOMIC_LOCKING_FETCH_AND_OR_DEFINE(uint8)

/* Signed */
ATOMIC_LOCKING_FETCH_AND_AND_DEFINE(int8)
ATOMIC_LOCKING_FETCH_AND_OR_DEFINE(int8)

#endif

/** \} */

#undef __atomic_impl_load_generic
#undef __atomic_impl_store_generic

#undef ATOMIC_LOCKING_OP_AND_FETCH_DEFINE
#undef ATOMIC_LOCKING_FETCH_AND_OP_DEFINE
#undef ATOMIC_LOCKING_ADD_AND_FETCH_DEFINE
#undef ATOMIC_LOCKING_SUB_AND_FETCH_DEFINE
#undef ATOMIC_LOCKING_FETCH_AND_ADD_DEFINE
#undef ATOMIC_LOCKING_FETCH_AND_SUB_DEFINE
#undef ATOMIC_LOCKING_FETCH_AND_OR_DEFINE
#undef ATOMIC_LOCKING_FETCH_AND_AND_DEFINE
#undef ATOMIC_LOCKING_CAS_DEFINE
#undef ATOMIC_LOCKING_LOAD_DEFINE
#undef ATOMIC_LOCKING_STORE_DEFINE

#endif /* __ATOMIC_OPS_UNIX_H__ */
