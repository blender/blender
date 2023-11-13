/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Copy of the regular kernels with additional shader ray-tracing kernel that takes
 * much longer to compiler. This is only loaded when needed by the scene. */

#include "kernel/device/optix/kernel.cu"

#include "kernel/integrator/shade_surface.h"

extern "C" __global__ void __raygen__kernel_optix_integrator_shade_surface_raytrace()
{
  const int global_index = optixGetLaunchIndex().x;
  const int path_index = (kernel_params.path_index_array) ? kernel_params.path_index_array[global_index] :
                                                       global_index;
  integrator_shade_surface_raytrace(nullptr, path_index, kernel_params.render_buffer);
}

extern "C" __global__ void __raygen__kernel_optix_integrator_shade_surface_mnee()
{
  const int global_index = optixGetLaunchIndex().x;
  const int path_index = (kernel_params.path_index_array) ? kernel_params.path_index_array[global_index] :
                                                       global_index;
  integrator_shade_surface_mnee(nullptr, path_index, kernel_params.render_buffer);
}
