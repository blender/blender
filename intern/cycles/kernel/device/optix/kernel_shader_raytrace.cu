/*
 * Copyright 2021, Blender Foundation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Copy of the regular kernels with additional shader ray-tracing kernel that takes
 * much longer to compiler. This is only loaded when needed by the scene. */

#include "kernel/device/optix/kernel.cu"

#include "kernel/integrator/shade_surface.h"

extern "C" __global__ void __raygen__kernel_optix_integrator_shade_surface_raytrace()
{
  const int global_index = optixGetLaunchIndex().x;
  const int path_index = (__params.path_index_array) ? __params.path_index_array[global_index] :
                                                       global_index;
  integrator_shade_surface_raytrace(nullptr, path_index, __params.render_buffer);
}
