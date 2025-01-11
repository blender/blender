/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/debug.h"   // IWYU pragma: keep
#include "util/system.h"  // IWYU pragma: keep

CCL_NAMESPACE_BEGIN

/* A wrapper around per-microarchitecture variant of a kernel function.
 *
 * Provides a function-call-like API which gets routed to the most suitable implementation.
 *
 * For example, on a computer which only has AVX2 the kernel_avx2 will be used. */
template<typename FunctionType> class CPUKernelFunction {
 public:
  CPUKernelFunction(FunctionType kernel_default, FunctionType kernel_avx2)
  {
    kernel_info_ = get_best_kernel_info(kernel_default, kernel_avx2);
  }

  template<typename... Args> auto operator()(Args... args) const
  {
    assert(kernel_info_.kernel);

    return kernel_info_.kernel(args...);
  }

  const char *get_uarch_name() const
  {
    return kernel_info_.uarch_name;
  }

 protected:
  /* Helper class which allows to pass human-readable microarchitecture name together with function
   * pointer. */
  class KernelInfo {
   public:
    KernelInfo() : KernelInfo("", nullptr) {}

    /* TODO(sergey): Use string view, to have higher-level functionality (i.e. comparison) without
     * memory allocation. */
    KernelInfo(const char *uarch_name, FunctionType kernel)
        : uarch_name(uarch_name), kernel(kernel)
    {
    }

    const char *uarch_name;
    FunctionType kernel;
  };

  KernelInfo get_best_kernel_info(FunctionType kernel_default, FunctionType kernel_avx2)
  {
    /* Silence warnings about unused variables when compiling without some architectures. */
    (void)kernel_avx2;

#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX2
    if (DebugFlags().cpu.has_avx2() && system_cpu_support_avx2()) {
      return KernelInfo("AVX2", kernel_avx2);
    }
#endif

    return KernelInfo("default", kernel_default);
  }

  KernelInfo kernel_info_;
};

CCL_NAMESPACE_END
