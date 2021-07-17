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

#pragma once

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

/* Utility functions. */
void _BLI_assert_print_pos(const char *file, const int line, const char *function, const char *id);
void _BLI_assert_print_extra(const char *str);
void _BLI_assert_print_backtrace(void);
void _BLI_assert_abort(void);
void _BLI_assert_unreachable_print(const char *file, const int line, const char *function);

#ifdef _MSC_VER
#  include <crtdbg.h> /* for _STATIC_ASSERT */
#endif

#ifndef NDEBUG
/* _BLI_ASSERT_PRINT_POS */
#  if defined(__GNUC__)
#    define _BLI_ASSERT_PRINT_POS(a) _BLI_assert_print_pos(__FILE__, __LINE__, __func__, #    a)
#  elif defined(_MSC_VER)
#    define _BLI_ASSERT_PRINT_POS(a) _BLI_assert_print_pos(__FILE__, __LINE__, __func__, #    a)
#  else
#    define _BLI_ASSERT_PRINT_POS(a) _BLI_assert_print_pos(__FILE__, __LINE__, "<?>", #    a)
#  endif
/* _BLI_ASSERT_ABORT */
#  ifdef WITH_ASSERT_ABORT
#    define _BLI_ASSERT_ABORT _BLI_assert_abort
#  else
#    define _BLI_ASSERT_ABORT() (void)0
#  endif
/* BLI_assert */
#  define BLI_assert(a) \
    (void)((!(a)) ? ((_BLI_assert_print_backtrace(), \
                      _BLI_ASSERT_PRINT_POS(a), \
                      _BLI_ASSERT_ABORT(), \
                      NULL)) : \
                    NULL)
/** A version of #BLI_assert() to pass an additional message to be printed on failure. */
#  define BLI_assert_msg(a, msg) \
    (void)((!(a)) ? ((_BLI_assert_print_backtrace(), \
                      _BLI_ASSERT_PRINT_POS(a), \
                      _BLI_assert_print_extra(msg), \
                      _BLI_ASSERT_ABORT(), \
                      NULL)) : \
                    NULL)
#else
#  define BLI_assert(a) ((void)0)
#  define BLI_assert_msg(a, msg) ((void)0)
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

/**
 * Indicates that this line of code should never be executed. If it is reached, it will abort in
 * debug builds and print an error in release builds.
 */
#define BLI_assert_unreachable() \
  { \
    _BLI_assert_unreachable_print(__FILE__, __LINE__, __func__); \
    BLI_assert_msg(0, "This line of code is marked to be unreachable."); \
  } \
  ((void)0)

#ifdef __cplusplus
}
#endif
