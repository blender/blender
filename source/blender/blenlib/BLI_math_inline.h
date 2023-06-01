/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

/* add platform/compiler checks here if it is not supported */
/* all platforms support forcing inline so this is always enabled */
#define BLI_MATH_DO_INLINE 1

#if BLI_MATH_DO_INLINE
#  ifdef _MSC_VER
#    define MINLINE static __forceinline
#    define MALWAYS_INLINE MINLINE
#  else
#    define MINLINE static inline
#    define MALWAYS_INLINE static inline __attribute__((always_inline)) __attribute__((unused))
#  endif
#else
#  define MINLINE
#  define MALWAYS_INLINE
#endif

/* Check for GCC push/pop pragma support. */
#ifdef __GNUC__
#  define BLI_MATH_GCC_WARN_PRAGMA 1
#endif

#ifdef __cplusplus
}
#endif
