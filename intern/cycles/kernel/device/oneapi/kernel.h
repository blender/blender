/* SPDX-FileCopyrightText: 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef WITH_ONEAPI

#  include <stddef.h>

/* NOTE(@nsirgien): Should match underlying type in the declaration inside "kernel/types.h"
 * TODO: use kernel/types.h directly. */
enum DeviceKernel : int;

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

typedef void (*OneAPIErrorCallback)(const char *error, void *user_ptr);

struct KernelContext {
  /* Queue, associated with selected device */
  SyclQueue *queue;
  /* Pointer to USM device memory with all global/constant allocation on this device */
  void *kernel_globals;
  /* We needs this additional data for some kernels. */
  int scene_max_shaders;
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
CYCLES_KERNEL_ONEAPI_EXPORT size_t oneapi_suggested_gpu_kernel_size(const DeviceKernel kernel);
CYCLES_KERNEL_ONEAPI_EXPORT bool oneapi_enqueue_kernel(KernelContext *context,
                                                       int kernel,
                                                       size_t global_size,
                                                       size_t local_size,
                                                       const unsigned int kernel_features,
                                                       bool use_hardware_raytracing,
                                                       void **args);
CYCLES_KERNEL_ONEAPI_EXPORT bool oneapi_load_kernels(SyclQueue *queue,
                                                     const unsigned int kernel_features,
                                                     bool use_hardware_raytracing);
#  ifdef __cplusplus
}

#  endif
#endif /* WITH_ONEAPI */
