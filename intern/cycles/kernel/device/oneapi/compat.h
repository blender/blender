/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2021-2022 Intel Corporation */

#pragma once

#define __KERNEL_GPU__
#define __KERNEL_ONEAPI__

#define CCL_NAMESPACE_BEGIN
#define CCL_NAMESPACE_END

#include <cstdint>
#include <math.h>

#ifndef __NODES_MAX_GROUP__
#  define __NODES_MAX_GROUP__ NODE_GROUP_LEVEL_MAX
#endif
#ifndef __NODES_FEATURES__
#  define __NODES_FEATURES__ NODE_FEATURE_ALL
#endif

/* This one does not have an abstraction.
 * It's used by other devices directly.
 */

#define __device__

/* Qualifier wrappers for different names on different devices */

#define ccl_device
#define ccl_device_extern extern "C"
#define ccl_global
#define ccl_always_inline __attribute__((always_inline))
#define ccl_device_inline inline
#define ccl_noinline __attribute__((noinline))
#define ccl_inline_constant const constexpr
#define ccl_static_constant const
#define ccl_device_forceinline __attribute__((always_inline))
#define ccl_device_noinline ccl_device ccl_noinline
#define ccl_device_noinline_cpu ccl_device
#define ccl_device_inline_method ccl_device
#define ccl_restrict __restrict__
#define ccl_loop_no_unroll
#define ccl_optional_struct_init
#define ccl_private
#define ATTR_FALLTHROUGH __attribute__((fallthrough))
#define ccl_constant const
#define ccl_try_align(...) __attribute__((aligned(__VA_ARGS__)))
#define ccl_align(n) __attribute__((aligned(n)))
#define kernel_assert(cond)
#define ccl_may_alias

/* clang-format off */

/* kernel.h adapters */
#define ccl_gpu_kernel(block_num_threads, thread_num_registers)
#define ccl_gpu_kernel_threads(block_num_threads)

#define ccl_gpu_kernel_signature(name, ...) \
void oneapi_kernel_##name(KernelGlobalsGPU *ccl_restrict kg, \
                          size_t kernel_global_size, \
                          size_t kernel_local_size, \
                          sycl::handler &cgh, \
                          __VA_ARGS__) { \
      (kg); \
      cgh.parallel_for<class kernel_##name>( \
          sycl::nd_range<1>(kernel_global_size, kernel_local_size), \
          [=](sycl::nd_item<1> item) {

#define ccl_gpu_kernel_postfix \
          }); \
    }

#define ccl_gpu_kernel_call(x) ((ONEAPIKernelContext*)kg)->x

#define ccl_gpu_kernel_lambda(func, ...) \
  struct KernelLambda \
  { \
    KernelLambda(const ONEAPIKernelContext *_kg) : kg(_kg) {} \
    ccl_private const ONEAPIKernelContext *kg; \
    __VA_ARGS__; \
    int operator()(const int state) const { return (func); } \
  } ccl_gpu_kernel_lambda_pass((ONEAPIKernelContext *)kg)

/* GPU thread, block, grid size and index */
#define ccl_gpu_thread_idx_x (sycl::ext::oneapi::experimental::this_nd_item<1>().get_local_id(0))
#define ccl_gpu_block_dim_x (sycl::ext::oneapi::experimental::this_nd_item<1>().get_local_range(0))
#define ccl_gpu_block_idx_x (sycl::ext::oneapi::experimental::this_nd_item<1>().get_group(0))
#define ccl_gpu_grid_dim_x (sycl::ext::oneapi::experimental::this_nd_item<1>().get_group_range(0))
#define ccl_gpu_warp_size (sycl::ext::oneapi::experimental::this_sub_group().get_local_range()[0])
#define ccl_gpu_thread_mask(thread_warp) uint(0xFFFFFFFF >> (ccl_gpu_warp_size - thread_warp))

#define ccl_gpu_global_id_x() (sycl::ext::oneapi::experimental::this_nd_item<1>().get_global_id(0))
#define ccl_gpu_global_size_x() (sycl::ext::oneapi::experimental::this_nd_item<1>().get_global_range(0))

/* GPU warp synchronization */
#define ccl_gpu_syncthreads() sycl::ext::oneapi::experimental::this_nd_item<1>().barrier()
#define ccl_gpu_local_syncthreads() sycl::ext::oneapi::experimental::this_nd_item<1>().barrier(sycl::access::fence_space::local_space)
#ifdef __SYCL_DEVICE_ONLY__
  #define ccl_gpu_ballot(predicate) (sycl::ext::oneapi::group_ballot(sycl::ext::oneapi::experimental::this_sub_group(), predicate).count())
#else
  #define ccl_gpu_ballot(predicate) (predicate ? 1 : 0)
#endif

/* Debug defines */
#if defined(__SYCL_DEVICE_ONLY__)
#  define CONSTANT __attribute__((opencl_constant))
#else
#  define CONSTANT
#endif

#define sycl_printf(format, ...) {               \
    static const CONSTANT char fmt[] = format;               \
    sycl::ext::oneapi::experimental::printf(fmt, __VA_ARGS__ );    \
  }

#define sycl_printf_(format) {               \
    static const CONSTANT char fmt[] = format;               \
    sycl::ext::oneapi::experimental::printf(fmt);                  \
  }

/* GPU texture objects */

/* clang-format on */

/* Types */

/* It's not possible to use sycl types like sycl::float3, sycl::int3, etc
 * because these types have different interfaces from blender version. */

using uchar = unsigned char;
using sycl::half;

/* math functions */
#define fabsf(x) sycl::fabs((x))
#define copysignf(x, y) sycl::copysign((x), (y))
#define asinf(x) sycl::asin((x))
#define acosf(x) sycl::acos((x))
#define atanf(x) sycl::atan((x))
#define floorf(x) sycl::floor((x))
#define ceilf(x) sycl::ceil((x))
#define sinhf(x) sycl::sinh((x))
#define coshf(x) sycl::cosh((x))
#define tanhf(x) sycl::tanh((x))
#define hypotf(x, y) sycl::hypot((x), (y))
#define atan2f(x, y) sycl::atan2((x), (y))
#define fmaxf(x, y) sycl::fmax((x), (y))
#define fminf(x, y) sycl::fmin((x), (y))
#define fmodf(x, y) sycl::fmod((x), (y))
#define lgammaf(x) sycl::lgamma((x))

#define cosf(x) sycl::native::cos(((float)(x)))
#define sinf(x) sycl::native::sin(((float)(x)))
#define powf(x, y) sycl::native::powr(((float)(x)), ((float)(y)))
#define tanf(x) sycl::native::tan(((float)(x)))
#define logf(x) sycl::native::log(((float)(x)))
#define expf(x) sycl::native::exp(((float)(x)))

#define __forceinline __attribute__((always_inline))

/* Types */
#include "util/half.h"
#include "util/types.h"
