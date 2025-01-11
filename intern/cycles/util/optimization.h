/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifndef __KERNEL_GPU__

/* x86
 *
 * Compile a regular kernel. */

#  if defined(i386) || defined(_M_IX86)

/* x86-64
 *
 * Compile a regular (includes SSE4.2) and AVX2 kernel. */

#  elif defined(__x86_64__) || defined(_M_X64)

/* SSE4.2 is our minimum requirement for x86-64 CPUs, so auto enable */
#    define __KERNEL_SSE42__
/* no SSE4.2 kernel on x86-64, part of regular kernel */
#    ifdef WITH_KERNEL_AVX2
#      define WITH_CYCLES_OPTIMIZED_KERNEL_AVX2
#    endif

/* Arm Neon
 *
 * Compile a SSE4 kernel emulated with Neon. Most code is shared with
 * SSE, some specializations for performance and compatibility are made
 * made testing for __KERNEL_NEON__. */

#  elif (defined(__ARM_NEON) || defined(_M_ARM64)) && defined(WITH_SSE2NEON)

#    define __KERNEL_NEON__
#    define __KERNEL_SSE__
#    define __KERNEL_SSE2__
#    define __KERNEL_SSE3__
#    define __KERNEL_SSE42__

#  endif

#endif
