/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

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

/* gcc 4.6 (supports push/pop) */
#if (defined(__GNUC__) && ((__GNUC__ * 100 + __GNUC_MINOR__) >= 406))
#  define BLI_MATH_GCC_WARN_PRAGMA 1
#endif

#ifdef __cplusplus
}
#endif
