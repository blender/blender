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

#ifndef __ATOMIC_OPS_EXT_H__
#define __ATOMIC_OPS_EXT_H__

#include "atomic_ops_utils.h"

/******************************************************************************/
/* size_t operations. */
ATOMIC_STATIC_ASSERT(sizeof(size_t) == LG_SIZEOF_PTR, "sizeof(size_t) != LG_SIZEOF_PTR");

ATOMIC_INLINE size_t atomic_add_and_fetch_z(size_t *p, size_t x)
{
#if (LG_SIZEOF_PTR == 8)
  return (size_t)atomic_add_and_fetch_uint64((uint64_t *)p, (uint64_t)x);
#elif (LG_SIZEOF_PTR == 4)
  return (size_t)atomic_add_and_fetch_uint32((uint32_t *)p, (uint32_t)x);
#endif
}

ATOMIC_INLINE size_t atomic_sub_and_fetch_z(size_t *p, size_t x)
{
#if (LG_SIZEOF_PTR == 8)
  return (size_t)atomic_add_and_fetch_uint64((uint64_t *)p, (uint64_t)-((int64_t)x));
#elif (LG_SIZEOF_PTR == 4)
  return (size_t)atomic_add_and_fetch_uint32((uint32_t *)p, (uint32_t)-((int32_t)x));
#endif
}

ATOMIC_INLINE size_t atomic_fetch_and_add_z(size_t *p, size_t x)
{
#if (LG_SIZEOF_PTR == 8)
  return (size_t)atomic_fetch_and_add_uint64((uint64_t *)p, (uint64_t)x);
#elif (LG_SIZEOF_PTR == 4)
  return (size_t)atomic_fetch_and_add_uint32((uint32_t *)p, (uint32_t)x);
#endif
}

ATOMIC_INLINE size_t atomic_fetch_and_sub_z(size_t *p, size_t x)
{
#if (LG_SIZEOF_PTR == 8)
  return (size_t)atomic_fetch_and_add_uint64((uint64_t *)p, (uint64_t)-((int64_t)x));
#elif (LG_SIZEOF_PTR == 4)
  return (size_t)atomic_fetch_and_add_uint32((uint32_t *)p, (uint32_t)-((int32_t)x));
#endif
}

ATOMIC_INLINE size_t atomic_cas_z(size_t *v, size_t old, size_t _new)
{
#if (LG_SIZEOF_PTR == 8)
  return (size_t)atomic_cas_uint64((uint64_t *)v, (uint64_t)old, (uint64_t)_new);
#elif (LG_SIZEOF_PTR == 4)
  return (size_t)atomic_cas_uint32((uint32_t *)v, (uint32_t)old, (uint32_t)_new);
#endif
}

ATOMIC_INLINE size_t atomic_load_z(const size_t *v)
{
#if (LG_SIZEOF_PTR == 8)
  return (size_t)atomic_load_uint64((const uint64_t *)v);
#elif (LG_SIZEOF_PTR == 4)
  return (size_t)atomic_load_uint32((const uint32_t *)v);
#endif
}

ATOMIC_INLINE void atomic_store_z(size_t *p, size_t v)
{
#if (LG_SIZEOF_PTR == 8)
  atomic_store_uint64((uint64_t *)p, v);
#elif (LG_SIZEOF_PTR == 4)
  atomic_store_uint32((uint32_t *)p, v);
#endif
}

ATOMIC_INLINE size_t atomic_fetch_and_update_max_z(size_t *p, size_t x)
{
  size_t prev_value;
  while ((prev_value = *p) < x) {
    if (atomic_cas_z(p, prev_value, x) == prev_value) {
      break;
    }
  }
  return prev_value;
}

/******************************************************************************/
/* unsigned operations. */
ATOMIC_STATIC_ASSERT(sizeof(unsigned int) == LG_SIZEOF_INT,
                     "sizeof(unsigned int) != LG_SIZEOF_INT");

ATOMIC_INLINE unsigned int atomic_add_and_fetch_u(unsigned int *p, unsigned int x)
{
#if (LG_SIZEOF_INT == 8)
  return (unsigned int)atomic_add_and_fetch_uint64((uint64_t *)p, (uint64_t)x);
#elif (LG_SIZEOF_INT == 4)
  return (unsigned int)atomic_add_and_fetch_uint32((uint32_t *)p, (uint32_t)x);
#endif
}

ATOMIC_INLINE unsigned int atomic_sub_and_fetch_u(unsigned int *p, unsigned int x)
{
#if (LG_SIZEOF_INT == 8)
  return (unsigned int)atomic_add_and_fetch_uint64((uint64_t *)p, (uint64_t)-((int64_t)x));
#elif (LG_SIZEOF_INT == 4)
  return (unsigned int)atomic_add_and_fetch_uint32((uint32_t *)p, (uint32_t)-((int32_t)x));
#endif
}

ATOMIC_INLINE unsigned int atomic_fetch_and_add_u(unsigned int *p, unsigned int x)
{
#if (LG_SIZEOF_INT == 8)
  return (unsigned int)atomic_fetch_and_add_uint64((uint64_t *)p, (uint64_t)x);
#elif (LG_SIZEOF_INT == 4)
  return (unsigned int)atomic_fetch_and_add_uint32((uint32_t *)p, (uint32_t)x);
#endif
}

ATOMIC_INLINE unsigned int atomic_fetch_and_sub_u(unsigned int *p, unsigned int x)
{
#if (LG_SIZEOF_INT == 8)
  return (unsigned int)atomic_fetch_and_add_uint64((uint64_t *)p, (uint64_t)-((int64_t)x));
#elif (LG_SIZEOF_INT == 4)
  return (unsigned int)atomic_fetch_and_add_uint32((uint32_t *)p, (uint32_t)-((int32_t)x));
#endif
}

ATOMIC_INLINE unsigned int atomic_cas_u(unsigned int *v, unsigned int old, unsigned int _new)
{
#if (LG_SIZEOF_INT == 8)
  return (unsigned int)atomic_cas_uint64((uint64_t *)v, (uint64_t)old, (uint64_t)_new);
#elif (LG_SIZEOF_INT == 4)
  return (unsigned int)atomic_cas_uint32((uint32_t *)v, (uint32_t)old, (uint32_t)_new);
#endif
}

/******************************************************************************/
/* Char operations. */
ATOMIC_INLINE char atomic_fetch_and_or_char(char *p, char b)
{
  return (char)atomic_fetch_and_or_uint8((uint8_t *)p, (uint8_t)b);
}

ATOMIC_INLINE char atomic_fetch_and_and_char(char *p, char b)
{
  return (char)atomic_fetch_and_and_uint8((uint8_t *)p, (uint8_t)b);
}

/******************************************************************************/
/* Pointer operations. */

ATOMIC_INLINE void *atomic_cas_ptr(void **v, void *old, void *_new)
{
#if (LG_SIZEOF_PTR == 8)
  return (void *)atomic_cas_uint64((uint64_t *)v, *(uint64_t *)&old, *(uint64_t *)&_new);
#elif (LG_SIZEOF_PTR == 4)
  return (void *)atomic_cas_uint32((uint32_t *)v, *(uint32_t *)&old, *(uint32_t *)&_new);
#endif
}

ATOMIC_INLINE void *atomic_load_ptr(void *const *v)
{
#if (LG_SIZEOF_PTR == 8)
  return (void *)atomic_load_uint64((const uint64_t *)v);
#elif (LG_SIZEOF_PTR == 4)
  return (void *)atomic_load_uint32((const uint32_t *)v);
#endif
}

ATOMIC_INLINE void atomic_store_ptr(void **p, void *v)
{
#if (LG_SIZEOF_PTR == 8)
  atomic_store_uint64((uint64_t *)p, (uint64_t)v);
#elif (LG_SIZEOF_PTR == 4)
  atomic_store_uint32((uint32_t *)p, (uint32_t)v);
#endif
}

/******************************************************************************/
/* float operations. */
ATOMIC_STATIC_ASSERT(sizeof(float) == sizeof(uint32_t), "sizeof(float) != sizeof(uint32_t)");

ATOMIC_INLINE float atomic_cas_float(float *v, float old, float _new)
{
  uint32_t ret = atomic_cas_uint32((uint32_t *)v, *(uint32_t *)&old, *(uint32_t *)&_new);
  return *(float *)&ret;
}

ATOMIC_INLINE float atomic_add_and_fetch_fl(float *p, const float x)
{
  float oldval, newval;
  uint32_t prevval;

  do { /* Note that since collisions are unlikely, loop will nearly always run once. */
    oldval = *p;
    newval = oldval + x;
    prevval = atomic_cas_uint32((uint32_t *)p, *(uint32_t *)(&oldval), *(uint32_t *)(&newval));
  } while (_ATOMIC_UNLIKELY(prevval != *(uint32_t *)(&oldval)));

  return newval;
}

#endif /* __ATOMIC_OPS_EXT_H__ */
