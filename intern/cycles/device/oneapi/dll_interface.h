/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

/* Include kernel header to get access to SYCL-specific types, like SyclQueue and
 * OneAPIDeviceIteratorCallback. */
#include "kernel/device/oneapi/kernel.h"

#ifdef WITH_ONEAPI
struct OneAPIDLLInterface {
#  define DLL_INTERFACE_CALL(function, return_type, ...) \
    return_type (*function)(__VA_ARGS__) = nullptr;
#  include "kernel/device/oneapi/dll_interface_template.h"
#  undef DLL_INTERFACE_CALL
};
#endif
