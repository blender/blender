/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Macro's used in GHOST debug target.
 */

#pragma once

#ifdef _MSC_VER
#  ifdef _DEBUG
/* Suppress STL-MSVC debug info warning. */
#    pragma warning(disable : 4786)
#  endif
#endif

#include <cstdio> /* For `printf()`. */
#include <iostream>

#if defined(WITH_GHOST_DEBUG)
#  define GHOST_PRINT(x) \
    { \
      std::cout << x; \
    } \
    ((void)0)
#  define GHOST_PRINTF(x, ...) \
    { \
      printf(x, __VA_ARGS__); \
    } \
    ((void)0)
#else
/* Expand even when `WITH_GHOST_DEBUG` is disabled to prevent expressions
 * becoming invalid even when the option is disable. */
#  define GHOST_PRINT(x) \
    { \
      if (false) { \
        std::cout << x; \
      } \
    } \
    ((void)0)
#  define GHOST_PRINTF(x, ...) \
    { \
      if (false) { \
        printf(x, __VA_ARGS__); \
      } \
    } \
    ((void)0)

#endif /* `!defined(WITH_GHOST_DEBUG)` */

#ifndef NDEBUG
#  ifdef WITH_ASSERT_ABORT
#    include <cstdlib> /* For `abort()`. */
#    define GHOST_ASSERT(x, info) \
      { \
        if (!(x)) { \
          fprintf(stderr, "GHOST_ASSERT failed: "); \
          fprintf(stderr, info); \
          fprintf(stderr, "\n"); \
          abort(); \
        } \
      } \
      ((void)0)
/* Show the failure in non-release builds too. */
#  else
#    define GHOST_ASSERT(x, info) \
      { \
        if (!(x)) { \
          GHOST_PRINT("GHOST_ASSERT failed: "); \
          GHOST_PRINT(info); \
          GHOST_PRINT("\n"); \
        } \
      } \
      ((void)0)
#  endif
#else /* `!defined(NDEBUG)` */
#  define GHOST_ASSERT(x, info) ((void)0)
#endif /* `!defined(NDEBUG)` */
