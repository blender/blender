/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * SIMD instruction support.
 */

#if defined(__ARM_NEON) && defined(WITH_SSE2NEON)
/* SSE/SSE2 emulation on ARM Neon. Match SSE precision. */
#  define SSE2NEON_PRECISE_MINMAX 1
#  define SSE2NEON_PRECISE_DIV 1
#  define SSE2NEON_PRECISE_SQRT 1
#  include <sse2neon.h>
#  define BLI_HAVE_SSE2
#elif defined(__SSE2__)
/* Native SSE2 on Intel/AMD. */
#  include <emmintrin.h>
#  define BLI_HAVE_SSE2
#endif
