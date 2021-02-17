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
