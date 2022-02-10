/* SPDX-License-Identifier: GPL-2.0-or-later */

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
