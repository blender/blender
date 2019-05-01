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

/** \file
 * \ingroup Atomic
 *
 * \brief Provides wrapper around system-specific atomic primitives,
 * and some extensions (faked-atomic operations over float numbers).
 */

#ifndef __ATOMIC_OPS_H__
#define __ATOMIC_OPS_H__

#if defined(__arm__)
/* Attempt to fix compilation error on Debian armel kernel.
 * arm7 architecture does have both 32 and 64bit atomics, however
 * it's gcc doesn't have __GCC_HAVE_SYNC_COMPARE_AND_SWAP_n defined.
 */
#  define JE_FORCE_SYNC_COMPARE_AND_SWAP_1
#  define JE_FORCE_SYNC_COMPARE_AND_SWAP_4
#  define JE_FORCE_SYNC_COMPARE_AND_SWAP_8
#endif

#include "intern/atomic_ops_utils.h"

/******************************************************************************/
/* Function prototypes. */

#if (LG_SIZEOF_PTR == 8 || LG_SIZEOF_INT == 8)
ATOMIC_INLINE uint64_t atomic_add_and_fetch_uint64(uint64_t *p, uint64_t x);
ATOMIC_INLINE uint64_t atomic_sub_and_fetch_uint64(uint64_t *p, uint64_t x);
ATOMIC_INLINE uint64_t atomic_fetch_and_add_uint64(uint64_t *p, uint64_t x);
ATOMIC_INLINE uint64_t atomic_fetch_and_sub_uint64(uint64_t *p, uint64_t x);
ATOMIC_INLINE uint64_t atomic_cas_uint64(uint64_t *v, uint64_t old, uint64_t _new);

ATOMIC_INLINE int64_t atomic_add_and_fetch_int64(int64_t *p, int64_t x);
ATOMIC_INLINE int64_t atomic_sub_and_fetch_int64(int64_t *p, int64_t x);
ATOMIC_INLINE int64_t atomic_fetch_and_add_int64(int64_t *p, int64_t x);
ATOMIC_INLINE int64_t atomic_fetch_and_sub_int64(int64_t *p, int64_t x);
ATOMIC_INLINE int64_t atomic_cas_int64(int64_t *v, int64_t old, int64_t _new);
#endif

ATOMIC_INLINE uint32_t atomic_add_and_fetch_uint32(uint32_t *p, uint32_t x);
ATOMIC_INLINE uint32_t atomic_sub_and_fetch_uint32(uint32_t *p, uint32_t x);
ATOMIC_INLINE uint32_t atomic_cas_uint32(uint32_t *v, uint32_t old, uint32_t _new);

ATOMIC_INLINE uint32_t atomic_fetch_and_add_uint32(uint32_t *p, uint32_t x);
ATOMIC_INLINE uint32_t atomic_fetch_and_or_uint32(uint32_t *p, uint32_t x);
ATOMIC_INLINE uint32_t atomic_fetch_and_and_uint32(uint32_t *p, uint32_t x);

ATOMIC_INLINE int32_t atomic_add_and_fetch_int32(int32_t *p, int32_t x);
ATOMIC_INLINE int32_t atomic_sub_and_fetch_int32(int32_t *p, int32_t x);
ATOMIC_INLINE int32_t atomic_cas_int32(int32_t *v, int32_t old, int32_t _new);

ATOMIC_INLINE int32_t atomic_fetch_and_add_int32(int32_t *p, int32_t x);
ATOMIC_INLINE int32_t atomic_fetch_and_or_int32(int32_t *p, int32_t x);
ATOMIC_INLINE int32_t atomic_fetch_and_and_int32(int32_t *p, int32_t x);

ATOMIC_INLINE uint8_t atomic_fetch_and_or_uint8(uint8_t *p, uint8_t b);
ATOMIC_INLINE uint8_t atomic_fetch_and_and_uint8(uint8_t *p, uint8_t b);

ATOMIC_INLINE int8_t atomic_fetch_and_or_int8(int8_t *p, int8_t b);
ATOMIC_INLINE int8_t atomic_fetch_and_and_int8(int8_t *p, int8_t b);

ATOMIC_INLINE char atomic_fetch_and_or_char(char *p, char b);
ATOMIC_INLINE char atomic_fetch_and_and_char(char *p, char b);

ATOMIC_INLINE size_t atomic_add_and_fetch_z(size_t *p, size_t x);
ATOMIC_INLINE size_t atomic_sub_and_fetch_z(size_t *p, size_t x);
ATOMIC_INLINE size_t atomic_fetch_and_add_z(size_t *p, size_t x);
ATOMIC_INLINE size_t atomic_fetch_and_sub_z(size_t *p, size_t x);
ATOMIC_INLINE size_t atomic_cas_z(size_t *v, size_t old, size_t _new);
/* Uses CAS loop, see warning below. */
ATOMIC_INLINE size_t atomic_fetch_and_update_max_z(size_t *p, size_t x);

ATOMIC_INLINE unsigned int atomic_add_and_fetch_u(unsigned int *p, unsigned int x);
ATOMIC_INLINE unsigned int atomic_sub_and_fetch_u(unsigned int *p, unsigned int x);
ATOMIC_INLINE unsigned int atomic_fetch_and_add_u(unsigned int *p, unsigned int x);
ATOMIC_INLINE unsigned int atomic_fetch_and_sub_u(unsigned int *p, unsigned int x);
ATOMIC_INLINE unsigned int atomic_cas_u(unsigned int *v, unsigned int old, unsigned int _new);

ATOMIC_INLINE void *atomic_cas_ptr(void **v, void *old, void *_new);

ATOMIC_INLINE float atomic_cas_float(float *v, float old, float _new);

/* WARNING! Float 'atomics' are really faked ones, those are actually closer to some kind of
 * spinlock-sync'ed operation, which means they are only efficient if collisions are highly
 * unlikely (i.e. if probability of two threads working on the same pointer at the same time is
 * very low). */
ATOMIC_INLINE float atomic_add_and_fetch_fl(float *p, const float x);

/******************************************************************************/
/* Include system-dependent implementations. */

/* Note that we are using _unix flavor as fallback here
 * (it will raise precompiler errors as needed). */
#if defined(_MSC_VER)
#  include "intern/atomic_ops_msvc.h"
#else
#  include "intern/atomic_ops_unix.h"
#endif

/* Include 'fake' atomic extensions, built over real atomic primitives. */
#include "intern/atomic_ops_ext.h"

#endif /* __ATOMIC_OPS_H__ */
