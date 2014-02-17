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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Campbell Barton
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BLI_COMPILER_ATTRS_H__
#define __BLI_COMPILER_ATTRS_H__

/** \file BLI_compiler_attrs.h
 *  \ingroup bli
 */

/* hint to make sure function result is actually used */
#ifdef __GNUC__
#  define ATTR_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#  define ATTR_WARN_UNUSED_RESULT
#endif

/* hint to mark function arguments expected to be non-null
 * if no arguments are given to the macro, all of pointer
 * arguments owuld be expected to be non-null
 */
#ifdef __GNUC__
#  define ATTR_NONNULL(args ...) __attribute__((nonnull(args)))
#else
#  define ATTR_NONNULL(...)
#endif

/* never returns NULL */
#  if (__GNUC__ * 100 + __GNUC_MINOR__) >= 409  /* gcc4.9+ only */
#  define ATTR_RETURNS_NONNULL __attribute__((returns_nonnull))
#else
#  define ATTR_RETURNS_NONNULL
#endif

/* hint to mark function as it wouldn't return */
#if defined(__GNUC__) || defined(__clang__)
#  define ATTR_NORETURN __attribute__((noreturn))
#else
#  define ATTR_NORETURN
#endif

/* hint to treat any non-null function return value cannot alias any other pointer */
#if (defined(__GNUC__) && ((__GNUC__ * 100 + __GNUC_MINOR__) >= 403))
#  define ATTR_MALLOC __attribute__((malloc))
#else
#  define ATTR_MALLOC
#endif

/* the function return value points to memory (2 args for 'size * tot') */
#if (defined(__GNUC__) && ((__GNUC__ * 100 + __GNUC_MINOR__) >= 403))
#  define ATTR_ALLOC_SIZE(args ...) __attribute__((alloc_size(args)))
#else
#  define ATTR_ALLOC_SIZE(...)
#endif

/* ensures a NULL terminating argument as the n'th last argument of a variadic function */
#ifdef __GNUC__
#  define ATTR_SENTINEL(arg_pos) __attribute__((sentinel(arg_pos)))
#else
#  define ATTR_SENTINEL(arg_pos)
#endif

/* hint to compiler that function uses printf-style format string */
#ifdef __GNUC__
#  define ATTR_PRINTF_FORMAT(format_param, dots_param) __attribute__((format(printf, format_param, dots_param)))
#else
#  define ATTR_PRINTF_FORMAT(format_param, dots_param)
#endif

#endif  /* __BLI_COMPILER_ATTRS_H__ */
