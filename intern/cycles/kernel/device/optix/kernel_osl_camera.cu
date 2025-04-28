/* SPDX-FileCopyrightText: 2011-2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#define WITH_OSL

#include "kernel/device/optix/compat.h"
#include "kernel/device/optix/globals.h"

#include "kernel/integrator/init_from_camera.h"

#include "kernel/device/gpu/work_stealing.h"

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
