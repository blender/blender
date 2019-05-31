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
 */

#ifndef __BLI_ASSERT_H__
#define __BLI_ASSERT_H__

/** \file
 * \ingroup bli
 *
 * Defines:
 * - #BLI_assert
 * - #BLI_STATIC_ASSERT
 * - #BLI_STATIC_ASSERT_ALIGN
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NDEBUG /* for BLI_assert */
#  include <stdio.h>
#endif

#ifdef _MSC_VER
#  include <crtdbg.h> /* for _STATIC_ASSERT */
#endif

/* BLI_assert(), default only to print
 * for aborting need to define WITH_ASSERT_ABORT
 */
/* For 'abort' only. */
#include <stdlib.h>

#ifndef NDEBUG
#  include "BLI_system.h"
/* _BLI_ASSERT_PRINT_POS */
#  if defined(__GNUC__)
#    define _BLI_ASSERT_PRINT_POS(a) \
      fprintf(stderr, \
              "BLI_assert failed: %s:%d, %s(), at \'%s\'\n", \
              __FILE__, \
              __LINE__, \
              __func__, \
              #a)
#  elif defined(_MSC_VER)
#    define _BLI_ASSERT_PRINT_POS(a) \
      fprintf(stderr, \
              "BLI_assert failed: %s:%d, %s(), at \'%s\'\n", \
              __FILE__, \
              __LINE__, \
              __FUNCTION__, \
              #a)
#  else
#    define _BLI_ASSERT_PRINT_POS(a) \
      fprintf(stderr, "BLI_assert failed: %s:%d, at \'%s\'\n", __FILE__, __LINE__, #a)
#  endif
/* _BLI_ASSERT_ABORT */
#  ifdef WITH_ASSERT_ABORT
#    define _BLI_ASSERT_ABORT abort
#  else
#    define _BLI_ASSERT_ABORT() (void)0
#  endif
/* BLI_assert */
#  define BLI_assert(a) \
    (void)((!(a)) ? ((BLI_system_backtrace(stderr), \
                      _BLI_ASSERT_PRINT_POS(a), \
                      _BLI_ASSERT_ABORT(), \
                      NULL)) : \
                    NULL)
#else
#  define BLI_assert(a) ((void)0)
#endif

// A Clang feature extension to determine compiler features.
#ifndef __has_feature
#  define __has_feature(x) 0
#endif

/* C++ can't use _Static_assert, expects static_assert() but c++0x only,
 * Coverity also errors out. */
#if (!defined(__cplusplus)) && (!defined(__COVERITY__)) && \
    ((defined(__GNUC__) && ((__GNUC__ * 100 + __GNUC_MINOR__) >= 406)) || \
     (defined(__clang__) && __has_feature(c_static_assert))) /* GCC 4.6+ and clang */
#  define BLI_STATIC_ASSERT(a, msg) __extension__ _Static_assert(a, msg);
#elif defined(_MSC_VER)
#  define BLI_STATIC_ASSERT(a, msg) _STATIC_ASSERT(a);
#else /* older gcc, clang... */
/* Code adapted from http://www.pixelbeat.org/programming/gcc/static_assert.html */
/* Note we need the two concats below because arguments to ## are not expanded, so we need to
 * expand __LINE__ with one indirection before doing the actual concatenation. */
#  define _BLI_ASSERT_CONCAT_(a, b) a##b
#  define _BLI_ASSERT_CONCAT(a, b) _BLI_ASSERT_CONCAT_(a, b)
/* This can't be used twice on the same line so ensure if using in headers
 * that the headers are not included twice (by wrapping in #ifndef...#endif)
 * Note it doesn't cause an issue when used on same line of separate modules
 * compiled with gcc -combine -fwhole-program. */
#  define BLI_STATIC_ASSERT(a, msg) \
    ; \
    enum { _BLI_ASSERT_CONCAT(assert_line_, __LINE__) = 1 / (int)(!!(a)) };
#endif

#define BLI_STATIC_ASSERT_ALIGN(st, align) \
  BLI_STATIC_ASSERT((sizeof(st) % (align) == 0), "Structure must be strictly aligned")

#ifdef __cplusplus
}
#endif

#endif /* __BLI_ASSERT_H__ */
