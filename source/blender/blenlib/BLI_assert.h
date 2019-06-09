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

#if defined(__cplusplus)
/* C++11 */
#  define BLI_STATIC_ASSERT(a, msg) static_assert(a, msg);
#elif defined(_MSC_VER)
/* Visual Studio */
#  if (_MSC_VER > 1910) && !defined(__clang__)
#    define BLI_STATIC_ASSERT(a, msg) static_assert(a, msg);
#  else
#    define BLI_STATIC_ASSERT(a, msg) _STATIC_ASSERT(a);
#  endif
#elif defined(__COVERITY__)
/* Workaround error with coverity */
#  define BLI_STATIC_ASSERT(a, msg)
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
/* C11 */
#  define BLI_STATIC_ASSERT(a, msg) _Static_assert(a, msg);
#else
/* Old unsupported compiler */
#  define BLI_STATIC_ASSERT(a, msg)
#endif

#define BLI_STATIC_ASSERT_ALIGN(st, align) \
  BLI_STATIC_ASSERT((sizeof(st) % (align) == 0), "Structure must be strictly aligned")

#ifdef __cplusplus
}
#endif

#endif /* __BLI_ASSERT_H__ */
