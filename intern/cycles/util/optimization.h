/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __UTIL_OPTIMIZATION_H__
#define __UTIL_OPTIMIZATION_H__

#ifndef __KERNEL_GPU__

/* x86
 *
 * Compile a regular, SSE2 and SSE3 kernel. */

#  if defined(i386) || defined(_M_IX86)

/* We require minimum SSE2 support on x86, so auto enable. */
#    define __KERNEL_SSE2__
#    ifdef WITH_KERNEL_SSE2
#      define WITH_CYCLES_OPTIMIZED_KERNEL_SSE2
#    endif

/* x86-64
 *
 * Compile a regular (includes SSE2), SSE3, SSE 4.1, AVX and AVX2 kernel. */

#  elif defined(__x86_64__) || defined(_M_X64)

/* SSE2 is always available on x86-64 CPUs, so auto enable */
#    define __KERNEL_SSE2__
/* no SSE2 kernel on x86-64, part of regular kernel */
#    ifdef WITH_KERNEL_SSE41
#      define WITH_CYCLES_OPTIMIZED_KERNEL_SSE41
#    endif
#    ifdef WITH_KERNEL_AVX2
#      define WITH_CYCLES_OPTIMIZED_KERNEL_AVX2
#    endif

/* Arm Neon
 *
 * Compile a SSE4 kernel emulated with Neon. Most code is shared with
 * SSE, some specializations for performance and compatibility are made
 * made testing for __KERNEL_NEON__. */

#  elif defined(__ARM_NEON) && defined(WITH_SSE2NEON)

#    define __KERNEL_NEON__
#    define __KERNEL_SSE__
#    define __KERNEL_SSE2__
#    define __KERNEL_SSE3__
#    define __KERNEL_SSE41__

#  endif

#endif

#endif /* __UTIL_OPTIMIZATION_H__ */
