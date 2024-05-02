/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * SIMD instruction support.
 */

/* sse2neon.h uses a newer pre-processor which is no available for C language when using MSVC.
 * For the consistency require C++ for all build configurations.  */
#if !defined(__cplusplus)
#  error Including BLI_simd.hh requires C++
#endif

// TODO: Re-enable this once blenlib is converted to C++
#if (defined(__ARM_NEON) /* || (defined(_M_ARM64) && defined(_MSC_VER))*/) && \
    defined(WITH_SSE2NEON)
/* SSE/SSE2 emulation on ARM Neon. Match SSE precision. */
#  if !defined(SSE2NEON_PRECISE_MINMAX)
#    define SSE2NEON_PRECISE_MINMAX 1
#  endif
#  if !defined(SSE2NEON_PRECISE_DIV)
#    define SSE2NEON_PRECISE_DIV 1
#  endif
#  if !defined(SSE2NEON_PRECISE_SQRT)
#    define SSE2NEON_PRECISE_SQRT 1
#  endif
#  include <sse2neon.h>
#  define BLI_HAVE_SSE2 1
#elif defined(__SSE2__)
/* Native SSE2 on Intel/AMD. */
#  include <emmintrin.h>
#  define BLI_HAVE_SSE2 1
#else
#  define BLI_HAVE_SSE2 0
#endif

#if defined(__ARM_NEON) && defined(WITH_SSE2NEON)
/* SSE4 is emulated via sse2neon. */
#  define BLI_HAVE_SSE4 1
#elif defined(__SSE4_2__)
/* Native SSE4.2. */
#  include <nmmintrin.h>
#  define BLI_HAVE_SSE4 1
#else
#  define BLI_HAVE_SSE4 0
#endif
