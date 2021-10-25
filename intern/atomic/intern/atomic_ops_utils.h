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
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __ATOMIC_OPS_UTILS_H__
#define __ATOMIC_OPS_UTILS_H__

/* needed for int types */
#include "../../../source/blender/blenlib/BLI_sys_types.h"
#include <stdlib.h>
#include <limits.h>

#include <assert.h>

/* little macro so inline keyword works */
#if defined(_MSC_VER)
#  define ATOMIC_INLINE static __forceinline
#else
#  define ATOMIC_INLINE static inline __attribute__((always_inline))
#endif

#ifndef LIKELY
#  ifdef __GNUC__
#    define LIKELY(x)       __builtin_expect(!!(x), 1)
#    define UNLIKELY(x)     __builtin_expect(!!(x), 0)
#  else
#    define LIKELY(x)       (x)
#    define UNLIKELY(x)     (x)
#  endif
#endif

#if defined(__SIZEOF_POINTER__)
#  define LG_SIZEOF_PTR __SIZEOF_POINTER__
#elif defined(UINTPTR_MAX)
#  if (UINTPTR_MAX == 0xFFFFFFFF)
#    define LG_SIZEOF_PTR 4
#  elif (UINTPTR_MAX == 0xFFFFFFFFFFFFFFFF)
#    define LG_SIZEOF_PTR 8
#  endif
#elif defined(__WORDSIZE)  /* Fallback for older glibc and cpp */
#  if (__WORDSIZE == 32)
#    define LG_SIZEOF_PTR 4
#  elif (__WORDSIZE == 64)
#    define LG_SIZEOF_PTR 8
#  endif
#endif

#ifndef LG_SIZEOF_PTR
#  error "Cannot find pointer size"
#endif

#if (UINT_MAX == 0xFFFFFFFF)
#  define LG_SIZEOF_INT 4
#elif (UINT_MAX == 0xFFFFFFFFFFFFFFFF)
#  define LG_SIZEOF_INT 8
#else
#  error "Cannot find int size"
#endif

#endif /* __ATOMIC_OPS_UTILS_H__ */
