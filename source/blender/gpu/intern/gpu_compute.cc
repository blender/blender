/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup gpu
 */

#include "GPU_compute.h"

#include "gpu_backend.hh"

#ifdef __cplusplus
extern "C" {
#endif

void GPU_compute_dispatch(GPUShader *shader,
                          uint groups_x_len,
                          uint groups_y_len,
                          uint groups_z_len)
{
  blender::gpu::GPUBackend &gpu_backend = *blender::gpu::GPUBackend::get();
  GPU_shader_bind(shader);
  gpu_backend.compute_dispatch(groups_x_len, groups_y_len, groups_z_len);
}

#ifdef __cplusplus
}
#endif
