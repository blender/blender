/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup GHOST
 * Macro's used in GHOST debug target.
 */

#pragma once

#ifdef _MSC_VER
#  ifdef DEBUG
/* Suppress STL-MSVC debug info warning. */
#    pragma warning(disable : 4786)
#  endif
#endif

#if defined(WITH_GHOST_DEBUG) || (!defined(NDEBUG))
#  include <iostream>
#  include <stdio.h>  //for printf()
#endif                // WITH_GHOST_DEBUG

#if defined(WITH_GHOST_DEBUG)
#  define GHOST_PRINT(x) \
    { \
      std::cout << x; \
    } \
    (void)0
#  define GHOST_PRINTF(x, ...) \
    { \
      printf(x, __VA_ARGS__); \
    } \
    (void)0
#else
#  define GHOST_PRINT(x)
#  define GHOST_PRINTF(x, ...)
#endif /* `!defined(WITH_GHOST_DEBUG)` */

#ifdef WITH_ASSERT_ABORT
#  include <stdio.h>   //for fprintf()
#  include <stdlib.h>  //for abort()
#  define GHOST_ASSERT(x, info) \
    { \
      if (!(x)) { \
        fprintf(stderr, "GHOST_ASSERT failed: "); \
        fprintf(stderr, info); \
        fprintf(stderr, "\n"); \
        abort(); \
      } \
    } \
    (void)0
/* Assert in non-release builds too. */
#elif defined(WITH_GHOST_DEBUG) || (!defined(NDEBUG))
#  define GHOST_ASSERT(x, info) \
    { \
      if (!(x)) { \
        GHOST_PRINT("GHOST_ASSERT failed: "); \
        GHOST_PRINT(info); \
        GHOST_PRINT("\n"); \
      } \
    } \
    (void)0
#else /* `defined(WITH_GHOST_DEBUG) || (!defined(NDEBUG))` */
#  define GHOST_ASSERT(x, info) ((void)0)
#endif /* `defined(WITH_GHOST_DEBUG) || (!defined(NDEBUG))` */
