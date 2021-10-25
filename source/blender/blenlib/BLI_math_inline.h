/*
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: some of this file.
 *
 * ***** END GPL LICENSE BLOCK *****
 * */

#ifndef __BLI_MATH_INLINE_H__
#define __BLI_MATH_INLINE_H__

/** \file BLI_math_inline.h
 *  \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

/* add platform/compiler checks here if it is not supported */
/* all platforms support forcing inline so this is always enabled */
#define BLI_MATH_DO_INLINE 1

#if BLI_MATH_DO_INLINE
#  ifdef _MSC_VER
#    define MINLINE static __forceinline
#    define MALWAYS_INLINE MINLINE
#  else
#    define MINLINE static inline
#    define MALWAYS_INLINE static inline __attribute__((always_inline)) __attribute__((unused))
#  endif
#else
#  define MINLINE
#  define MALWAYS_INLINE
#endif

/* gcc 4.6 (supports push/pop) */
#if (defined(__GNUC__) && ((__GNUC__ * 100 + __GNUC_MINOR__) >= 406))
#  define BLI_MATH_GCC_WARN_PRAGMA 1
#endif

#ifdef __cplusplus
}
#endif

#endif /* __BLI_MATH_INLINE_H__ */
