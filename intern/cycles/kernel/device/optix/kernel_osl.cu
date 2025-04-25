/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#define WITH_OSL

/* Copy of the regular OptiX kernels with additional OSL support. */

#include "kernel/device/optix/kernel_shader_raytrace.cu"

#include "kernel/bake/bake.h"
#include "kernel/integrator/init_from_camera.h"
#include "kernel/integrator/shade_background.h"
#include "kernel/integrator/shade_dedicated_light.h"
#include "kernel/integrator/shade_light.h"
#include "kernel/integrator/shade_shadow.h"
#include "kernel/integrator/shade_volume.h"

#include "kernel/device/gpu/work_stealing.h"

extern "C" __global__ void __raygen__kernel_optix_integrator_shade_background()
{
  const int global_index = optixGetLaunchIndex().x;
  const int path_index = (kernel_params.path_index_array) ?
                             kernel_params.path_index_array[global_index] :
                             global_index;
  integrator_shade_background(nullptr, path_index, kernel_params.render_buffer);
}

extern "C" __global__ void __raygen__kernel_optix_integrator_shade_light()
{
  const int global_index = optixGetLaunchIndex().x;
  const int path_index = (kernel_params.path_index_array) ?
                             kernel_params.path_index_array[global_index] :
                             global_index;
  integrator_shade_light(nullptr, path_index, kernel_params.render_buffer);
}

extern "C" __global__ void __raygen__kernel_optix_integrator_shade_surface()
{
  const int global_index = optixGetLaunchIndex().x;
  const int path_index = (kernel_params.path_index_array) ?
                             kernel_params.path_index_array[global_index] :
                             global_index;
  integrator_shade_surface(nullptr, path_index, kernel_params.render_buffer);
}

extern "C" __global__ void __raygen__kernel_optix_integrator_shade_volume()
{
  const int global_index = optixGetLaunchIndex().x;
  const int path_index = (kernel_params.path_index_array) ?
                             kernel_params.path_index_array[global_index] :
                             global_index;
  integrator_shade_volume(nullptr, path_index, kernel_params.render_buffer);
}

extern "C" __global__ void __raygen__kernel_optix_integrator_shade_shadow()
{
  const int global_index = optixGetLaunchIndex().x;
  const int path_index = (kernel_params.path_index_array) ?
                             kernel_params.path_index_array[global_index] :
                             global_index;
  integrator_shade_shadow(nullptr, path_index, kernel_params.render_buffer);
}

extern "C" __global__ void __raygen__kernel_optix_integrator_shade_dedicated_light()
{
  const int global_index = optixGetLaunchIndex().x;
  const int path_index = (kernel_params.path_index_array) ?
                             kernel_params.path_index_array[global_index] :
                             global_index;
  integrator_shade_dedicated_light(nullptr, path_index, kernel_params.render_buffer);
}

extern "C" __global__ void __raygen__kernel_optix_shader_eval_displace()
{
  KernelShaderEvalInput *const input = (KernelShaderEvalInput *)kernel_params.path_index_array;
  float *const output = kernel_params.render_buffer;
  const int global_index = kernel_params.offset + optixGetLaunchIndex().x;
  kernel_displace_evaluate(nullptr, input, output, global_index);
}

extern "C" __global__ void __raygen__kernel_optix_shader_eval_background()
{
  KernelShaderEvalInput *const input = (KernelShaderEvalInput *)kernel_params.path_index_array;
  float *const output = kernel_params.render_buffer;
  const int global_index = kernel_params.offset + optixGetLaunchIndex().x;
  kernel_background_evaluate(nullptr, input, output, global_index);
}

extern "C" __global__ void __raygen__kernel_optix_shader_eval_curve_shadow_transparency()
{
  KernelShaderEvalInput *const input = (KernelShaderEvalInput *)kernel_params.path_index_array;
  float *const output = kernel_params.render_buffer;
  const int global_index = kernel_params.offset + optixGetLaunchIndex().x;
  kernel_curve_shadow_transparency_evaluate(nullptr, input, output, global_index);
}

extern "C" __global__ void __raygen__kernel_optix_integrator_init_from_camera()
{
  const int global_index = optixGetLaunchIndex().x;

  const KernelWorkTile *tiles = (const KernelWorkTile *)kernel_params.path_index_array;

  const int tile_index = global_index / kernel_params.max_tile_work_size;
  const int tile_work_index = global_index - tile_index * kernel_params.max_tile_work_size;

  const KernelWorkTile *tile = &tiles[tile_index];

  if (tile_work_index >= tile->work_size) {
    return;
  }

  const int path_index = tile->path_index_offset + tile_work_index;

  uint x, y, sample;
  get_work_pixel(tile, tile_work_index, &x, &y, &sample);

  integrator_init_from_camera(nullptr, path_index, tile, kernel_params.render_buffer, x, y, sample);
}
