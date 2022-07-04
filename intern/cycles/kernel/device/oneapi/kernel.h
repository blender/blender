/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2021-2022 Intel Corporation */

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
#      define CYCLES_KERNEL_ONEAPI_EXPORT
#    endif
#  endif

class SyclQueue;

typedef void (*OneAPIDeviceIteratorCallback)(const char *id,
                                             const char *name,
                                             int num,
                                             void *user_ptr);

typedef void (*OneAPIErrorCallback)(const char *error, void *user_ptr);

struct KernelContext {
  /* Queue, associated with selected device */
  SyclQueue *queue;
  /* Pointer to USM device memory with all global/constant allocation on this device */
  void *kernel_globals;
};

/* Use extern C linking so that the symbols can be easily load from the dynamic library at runtime.
 */
#  ifdef __cplusplus
extern "C" {
#  endif

#  define DLL_INTERFACE_CALL(function, return_type, ...) \
    CYCLES_KERNEL_ONEAPI_EXPORT return_type function(__VA_ARGS__);
#  include "kernel/device/oneapi/dll_interface_template.h"
#  undef DLL_INTERFACE_CALL

#  ifdef __cplusplus
}
#  endif

#endif /* WITH_ONEAPI */
