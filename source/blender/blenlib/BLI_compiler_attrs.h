/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2013 Blender Foundation. All rights reserved. */

#pragma once

/** \file
 * \ingroup bli
 */

/* hint to make sure function result is actually used */
#ifdef __GNUC__
#  define ATTR_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#  define ATTR_WARN_UNUSED_RESULT
#endif

/* hint to mark function arguments expected to be non-null
 * if no arguments are given to the macro, all of pointer
 * arguments would be expected to be non-null
 */
#ifdef __GNUC__
#  define ATTR_NONNULL(args...) __attribute__((nonnull(args)))
#else
#  define ATTR_NONNULL(...)
#endif

/* never returns NULL */
#if (__GNUC__ * 100 + __GNUC_MINOR__) >= 409 /* gcc4.9+ only */
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
#  define ATTR_ALLOC_SIZE(args...) __attribute__((alloc_size(args)))
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
#  define ATTR_PRINTF_FORMAT(format_param, dots_param) \
    __attribute__((format(printf, format_param, dots_param)))
#else
#  define ATTR_PRINTF_FORMAT(format_param, dots_param)
#endif

/* Use to suppress '-Wimplicit-fallthrough' (in place of 'break'). */
#ifndef ATTR_FALLTHROUGH
#  if defined(__GNUC__) && (__GNUC__ >= 7) /* gcc7.0+ only */
#    define ATTR_FALLTHROUGH __attribute__((fallthrough))
#  else
#    define ATTR_FALLTHROUGH ((void)0)
#  endif
#endif

/* Declare the memory alignment in Bytes. */
#if defined(_WIN32) && !defined(FREE_WINDOWS)
#  define ATTR_ALIGN(x) __declspec(align(x))
#else
#  define ATTR_ALIGN(x) __attribute__((aligned(x)))
#endif

/* Alignment directive */
#ifdef _WIN64
#  define ALIGN_STRUCT __declspec(align(64))
#else
#  define ALIGN_STRUCT
#endif
