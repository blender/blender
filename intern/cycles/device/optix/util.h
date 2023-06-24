/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef WITH_OPTIX

#  include "device/cuda/util.h"

#  ifdef WITH_CUDA_DYNLOAD
#    include <cuew.h>
// Do not use CUDA SDK headers when using CUEW
#    define OPTIX_DONT_INCLUDE_CUDA
#  endif

#  include <optix_stubs.h>

/* Utility for checking return values of OptiX function calls. */
#  define optix_device_assert(optix_device, stmt) \
    { \
      OptixResult result = stmt; \
      if (result != OPTIX_SUCCESS) { \
        const char *name = optixGetErrorName(result); \
        optix_device->set_error( \
            string_printf("%s in %s (%s:%d)", name, #stmt, __FILE__, __LINE__)); \
      } \
    } \
    (void)0

#  define optix_assert(stmt) optix_device_assert(this, stmt)

#endif /* WITH_OPTIX */
