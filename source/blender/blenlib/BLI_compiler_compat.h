/* SPDX-License-Identifier: GPL-2.0-or-later */

/* clang-format off */

/* #define typeof() triggers a bug in some clang-format versions, disable format
 * for entire file to keep results consistent. */

#pragma once


/** \file
 * \ingroup bli
 *
 * Use to help with cross platform portability.
 */

#if defined(_MSC_VER)
#  define alloca _alloca
#endif

#if (defined(__GNUC__) || defined(__clang__)) && defined(__cplusplus)
extern "C++" {
/** Some magic to be sure we don't have reference in the type. */
template<typename T> static inline T decltype_helper(T x)
{
  return x;
}
#define typeof(x) decltype(decltype_helper(x))
}
#endif

/* little macro so inline keyword works */
#if defined(_MSC_VER)
#  define BLI_INLINE static __forceinline
#else
#  define BLI_INLINE static inline __attribute__((always_inline)) __attribute__((__unused__))
#endif

#if defined(__GNUC__)
#  define BLI_NOINLINE __attribute__((noinline))
#else
#  define BLI_NOINLINE
#endif
