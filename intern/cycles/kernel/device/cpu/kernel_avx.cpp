/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

/* Optimized CPU kernel entry points. This file is compiled with AVX
 * optimization flags and nearly all functions inlined, while kernel.cpp
 * is compiled without for other CPU's. */

#include "util/optimization.h"

#ifndef WITH_CYCLES_OPTIMIZED_KERNEL_AVX
#  define KERNEL_STUB
#else
/* SSE optimization disabled for now on 32 bit, see bug T36316. */
#  if !(defined(__GNUC__) && (defined(i386) || defined(_M_IX86)))
#    define __KERNEL_SSE__
#    define __KERNEL_SSE2__
#    define __KERNEL_SSE3__
#    define __KERNEL_SSSE3__
#    define __KERNEL_SSE41__
#    define __KERNEL_AVX__
#  endif
#endif /* WITH_CYCLES_OPTIMIZED_KERNEL_AVX */

#include "kernel/device/cpu/kernel.h"
#define KERNEL_ARCH cpu_avx
#include "kernel/device/cpu/kernel_arch_impl.h"
