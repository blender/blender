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

/* Optimized CPU kernel entry points. This file is compiled with SSE3/SSSE3
 * optimization flags and nearly all functions inlined, while kernel.cpp
 * is compiled without for other CPU's. */

#define __SPLIT_KERNEL__

#include "util/util_optimization.h"

#ifndef WITH_CYCLES_OPTIMIZED_KERNEL_SSE3
#  define KERNEL_STUB
#else
/* SSE optimization disabled for now on 32 bit, see bug #36316 */
#  if !(defined(__GNUC__) && (defined(i386) || defined(_M_IX86)))
#    define __KERNEL_SSE2__
#    define __KERNEL_SSE3__
#    define __KERNEL_SSSE3__
#  endif
#endif  /* WITH_CYCLES_OPTIMIZED_KERNEL_SSE3 */

#include "kernel/kernel.h"
#define KERNEL_ARCH cpu_sse3
#include "kernel/kernels/cpu/kernel_cpu_impl.h"
