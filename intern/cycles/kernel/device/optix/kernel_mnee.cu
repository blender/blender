/* SPDX-FileCopyrightText: 2011-2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "kernel/device/optix/compat.h"
#include "kernel/device/optix/globals.h"

#include "kernel/device/gpu/image.h" /* Texture lookup uses normal CUDA intrinsics. */

#include "kernel/integrator/intersect_mnee.h"

extern "C" __global__ void __raygen__kernel_optix_integrator_intersect_mnee()
{
  const int global_index = optixGetLaunchIndex().x;
  const int path_index = (kernel_params.path_index_array) ?
                             kernel_params.path_index_array[global_index] :
                             global_index;
  integrator_intersect_mnee(nullptr, path_index);
}
