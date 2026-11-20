/* SPDX-FileCopyrightText: 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef WITH_ONEAPI

#  include <stddef.h>

#  include <cstdint>

#  ifndef CYCLES_KERNEL_ONEAPI_EXPORT
#    ifdef _WIN32
#      if defined(ONEAPI_EXPORT)
#        define CYCLES_KERNEL_ONEAPI_EXPORT extern __declspec(dllexport)
#      else
#        define CYCLES_KERNEL_ONEAPI_EXPORT extern __declspec(dllimport)
#      endif
#    else
#      define CYCLES_KERNEL_ONEAPI_EXPORT extern __attribute__((visibility("default")))
#    endif
#  endif

class SyclQueue;
class SyclDevice;

typedef void (*OneAPIErrorCallback)(const char *error, void *user_ptr);

struct KernelContext {
  /* Queue, associated with selected device */
  SyclQueue *queue = nullptr;
  /* Pointer to USM device memory with the kernel parameters of this device */
  void *kernel_params = nullptr;
  /* We needs this additional data for some kernels. */
  int scene_max_shaders = 0;
};

/* Properties of a compiled kernel, zero when unknown. */
struct OneAPIKernelInfo {
  size_t max_work_group_size = 0;
  /* SIMD width the kernel got compiled with. */
  uint32_t max_sub_group_size = 0;
  /* Maximum number of work-groups of one sub-group per EU of an Xe-core that can run concurrently
   * on the device, which depends on the number of registers the kernel uses. */
  size_t max_num_work_groups = 0;
};

/* Use extern C linking so that the symbols can be easily load from the dynamic library at runtime.
 */
#  ifdef __cplusplus
extern "C" {
#  endif

CYCLES_KERNEL_ONEAPI_EXPORT bool oneapi_run_test_kernel(SyclQueue *queue_);
CYCLES_KERNEL_ONEAPI_EXPORT bool oneapi_zero_memory_on_device(SyclQueue *queue_,
                                                              void *device_pointer,
                                                              size_t num_bytes);
CYCLES_KERNEL_ONEAPI_EXPORT void oneapi_set_error_cb(OneAPIErrorCallback cb, void *user_ptr);
CYCLES_KERNEL_ONEAPI_EXPORT bool oneapi_enqueue_kernel(KernelContext *context,
                                                       const int kernel,
                                                       const size_t global_size,
                                                       const size_t local_size,
                                                       uint64_t kernel_features,
                                                       bool use_hardware_raytracing,
                                                       void **args);
CYCLES_KERNEL_ONEAPI_EXPORT bool oneapi_load_kernels(SyclQueue *queue,
                                                     uint64_t kernel_features,
                                                     bool use_hardware_raytracing,
                                                     OneAPIKernelInfo *kernel_infos);
#  ifdef __cplusplus
}

#  endif
#endif /* WITH_ONEAPI */
