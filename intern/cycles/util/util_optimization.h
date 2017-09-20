/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __UTIL_OPTIMIZATION_H__
#define __UTIL_OPTIMIZATION_H__

#ifndef __KERNEL_GPU__

/* quiet unused define warnings */
#if defined(__KERNEL_SSE2__)  || \
	defined(__KERNEL_SSE3__)  || \
	defined(__KERNEL_SSSE3__) || \
	defined(__KERNEL_SSE41__) || \
	defined(__KERNEL_AVX__)   || \
	defined(__KERNEL_AVX2__)
	/* do nothing */
#endif

/* x86
 *
 * Compile a regular, SSE2 and SSE3 kernel. */

#if defined(i386) || defined(_M_IX86)

/* We require minimum SSE2 support on x86, so auto enable. */
#  define __KERNEL_SSE2__

#  ifdef WITH_KERNEL_SSE2
#    define WITH_CYCLES_OPTIMIZED_KERNEL_SSE2
#  endif

#  ifdef WITH_KERNEL_SSE3
#    define WITH_CYCLES_OPTIMIZED_KERNEL_SSE3
#  endif

#endif  /* defined(i386) || defined(_M_IX86) */

/* x86-64
 *
 * Compile a regular (includes SSE2), SSE3, SSE 4.1, AVX and AVX2 kernel. */

#if defined(__x86_64__) || defined(_M_X64)

/* SSE2 is always available on x86-64 CPUs, so auto enable */
#  define __KERNEL_SSE2__

/* no SSE2 kernel on x86-64, part of regular kernel */
#  ifdef WITH_KERNEL_SSE3
#    define WITH_CYCLES_OPTIMIZED_KERNEL_SSE3
#  endif

#  ifdef WITH_KERNEL_SSE41
#    define WITH_CYCLES_OPTIMIZED_KERNEL_SSE41
#  endif

#  ifdef WITH_KERNEL_AVX
#    define WITH_CYCLES_OPTIMIZED_KERNEL_AVX
#  endif

#  ifdef WITH_KERNEL_AVX2
#    define WITH_CYCLES_OPTIMIZED_KERNEL_AVX2
#  endif

#endif  /* defined(__x86_64__) || defined(_M_X64) */

/* SSE Experiment
 *
 * This is disabled code for an experiment to use SSE types globally for types
 * such as float3 and float4. Currently this gives an overall slowdown. */

#if 0
#  define __KERNEL_SSE__
#  ifndef __KERNEL_SSE2__
#    define __KERNEL_SSE2__
#  endif
#  ifndef __KERNEL_SSE3__
#    define __KERNEL_SSE3__
#  endif
#  ifndef __KERNEL_SSSE3__
#    define __KERNEL_SSSE3__
#  endif
#  ifndef __KERNEL_SSE4__
#    define __KERNEL_SSE4__
#  endif
#endif

/* SSE Intrinsics includes
 *
 * We assume __KERNEL_SSEX__ flags to have been defined at this point */

/* SSE intrinsics headers */
#ifndef FREE_WINDOWS64

#ifdef _MSC_VER
#  include <intrin.h>
#elif (defined(__x86_64__) || defined(__i386__))
#  include <x86intrin.h>
#endif

#else

/* MinGW64 has conflicting declarations for these SSE headers in <windows.h>.
 * Since we can't avoid including <windows.h>, better only include that */
#include "util/util_windows.h"

#endif

#endif

#endif /* __UTIL_OPTIMIZATION_H__ */

