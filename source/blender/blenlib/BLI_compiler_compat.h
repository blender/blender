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
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BLI_COMPILER_COMPAT_H__
#define __BLI_COMPILER_COMPAT_H__

/** \file BLI_compiler_compat.h
 *  \ingroup bli
 *
 * Use to help with cross platform portability.
 */

#if defined(_MSC_VER)
#  define __func__ __FUNCTION__
#  define alloca _alloca
#endif

#if (defined(__GNUC__) || defined(__clang__)) && defined(__cplusplus)
extern "C++" {
	/* Some magic to be sure we don't have reference in the type. */
	template<typename T> static inline T decltype_helper(T x) { return x; }
#  define typeof(x) decltype(decltype_helper(x))
}
#endif

/* little macro so inline keyword works */
#if defined(_MSC_VER)
#  define BLI_INLINE static __forceinline
#else
#  define BLI_INLINE static inline __attribute__((always_inline)) __attribute__((__unused__))
#endif

#endif  /* __BLI_COMPILER_COMPAT_H__ */
