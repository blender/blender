/*
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
 */

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

#ifdef WITH_GHOST_DEBUG
#  include <iostream>
#  include <stdio.h>  //for printf()
#endif                // WITH_GHOST_DEBUG

#ifdef WITH_GHOST_DEBUG
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
#else  // WITH_GHOST_DEBUG
#  define GHOST_PRINT(x)
#  define GHOST_PRINTF(x, ...)
#endif  // WITH_GHOST_DEBUG

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
#elif defined(WITH_GHOST_DEBUG)
#  define GHOST_ASSERT(x, info) \
    { \
      if (!(x)) { \
        GHOST_PRINT("GHOST_ASSERT failed: "); \
        GHOST_PRINT(info); \
        GHOST_PRINT("\n"); \
      } \
    } \
    (void)0
#else  // WITH_GHOST_DEBUG
#  define GHOST_ASSERT(x, info) ((void)0)
#endif  // WITH_GHOST_DEBUG
