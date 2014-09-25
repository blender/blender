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

#ifndef __BLI_COMPILER_TYPECHECK_H__
#define __BLI_COMPILER_TYPECHECK_H__

/** \file BLI_compiler_typecheck.h
 *  \ingroup bli
 *
 * Type checking macros (often used to ensure valid use of macro args).
 * These depend on compiler extensions and c11 in some cases.
 */

/* Causes warning:
 * incompatible types when assigning to type 'Foo' from type 'Bar'
 * ... the compiler optimizes away the temp var */
#ifdef __GNUC__
#define CHECK_TYPE(var, type)  {  \
	typeof(var) *__tmp;           \
	__tmp = (type *)NULL;         \
	(void)__tmp;                  \
} (void)0

#define CHECK_TYPE_PAIR(var_a, var_b)  {  \
	typeof(var_a) *__tmp;                 \
	__tmp = (typeof(var_b) *)NULL;        \
	(void)__tmp;                          \
} (void)0

#define CHECK_TYPE_PAIR_INLINE(var_a, var_b)  ((void)({  \
	typeof(var_a) *__tmp;                                \
	__tmp = (typeof(var_b) *)NULL;                       \
	(void)__tmp;                                         \
}))

#else
#  define CHECK_TYPE(var, type)
#  define CHECK_TYPE_PAIR(var_a, var_b)
#  define CHECK_TYPE_PAIR_INLINE(var_a, var_b) (void)0
#endif

/* can be used in simple macros */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#  define CHECK_TYPE_INLINE(val, type) \
	(void)((void)(((type)0) != (0 ? (val) : ((type)0))), \
	       _Generic((val), type: 0, const type: 0))
#else
#  define CHECK_TYPE_INLINE(val, type) \
	((void)(((type)0) != (0 ? (val) : ((type)0))))
#endif

#define CHECK_TYPE_NONCONST(var)  {      \
	void *non_const = 0 ? (var) : NULL;  \
	(void)non_const;                     \
} (void)0

#endif  /* __BLI_COMPILER_TYPECHECK_H__ */
