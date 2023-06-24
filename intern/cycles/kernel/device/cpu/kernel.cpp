/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* CPU kernel entry points */

/* On x86-64, we can assume SSE2, so avoid the extra kernel and compile this
 * one with SSE2 intrinsics.
 */
#if defined(__x86_64__) || defined(_M_X64)
#  define __KERNEL_SSE__
#  define __KERNEL_SSE2__
#endif

/* When building kernel for native machine detect kernel features from the flags
 * set by compiler.
 */
#ifdef WITH_KERNEL_NATIVE
#  ifdef __SSE2__
#    ifndef __KERNEL_SSE2__
#      define __KERNEL_SSE2__
#    endif
#  endif
#  ifdef __SSE3__
#    define __KERNEL_SSE3__
#  endif
#  ifdef __SSSE3__
#    define __KERNEL_SSSE3__
#  endif
#  ifdef __SSE4_1__
#    define __KERNEL_SSE41__
#  endif
#  ifdef __AVX__
#    ifndef __KERNEL_SSE__
#      define __KERNEL_SSE__
#    endif
#    define __KERNEL_AVX__
#  endif
#  ifdef __AVX2__
#    ifndef __KERNEL_SSE__
#      define __KERNEL_SSE__
#    endif
#    define __KERNEL_AVX2__
#  endif
#endif

/* quiet unused define warnings */
#if defined(__KERNEL_SSE2__)
/* do nothing */
#endif

#include "kernel/device/cpu/kernel.h"
#define KERNEL_ARCH cpu
#include "kernel/device/cpu/kernel_arch_impl.h"

CCL_NAMESPACE_BEGIN

/* Memory Copy */

void kernel_const_copy(KernelGlobalsCPU *kg, const char *name, void *host, size_t)
{
  if (strcmp(name, "data") == 0) {
    kg->data = *(KernelData *)host;
  }
  else {
    assert(0);
  }
}

void kernel_global_memory_copy(KernelGlobalsCPU *kg, const char *name, void *mem, size_t size)
{
  if (0) {
  }

#define KERNEL_DATA_ARRAY(type, tname) \
  else if (strcmp(name, #tname) == 0) { \
    kg->tname.data = (type *)mem; \
    kg->tname.width = size; \
  }
#include "kernel/data_arrays.h"
  else {
    assert(0);
  }
}

CCL_NAMESPACE_END
