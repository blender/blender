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

#endif

#endif /* __UTIL_OPTIMIZATION_H__ */
