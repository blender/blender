/* SPDX-FileCopyrightText: 2011-2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#define WITH_OSL

/* Shadow shading raygens for OSL, loaded as a separate optix module so they
 * can be compiled in parallel with the base OSL module. */

#include "kernel/device/optix/compat.h"
#include "kernel/device/optix/globals.h"

#include "kernel/film/data_passes.h"

#include "kernel/integrator/shade_shadow.h"

extern "C" __global__ void __raygen__kernel_optix_integrator_shade_shadow()
{
  const int global_index = optixGetLaunchIndex().x;
  const int path_index = (kernel_params.path_index_array) ?
                             kernel_params.path_index_array[global_index] :
                             global_index;
  integrator_shade_shadow(nullptr, path_index, kernel_params.render_buffer);
}
