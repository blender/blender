/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "GPU_compute.hh"

#include "gpu_backend.hh"
#include "gpu_debug_private.hh"

namespace blender {

void GPU_compute_dispatch(gpu::Shader *shader,
                          uint groups_x_len,
                          uint groups_y_len,
                          uint groups_z_len,
                          const gpu::shader::SpecializationConstants *constants_state)
{
  gpu::GPUBackend &gpu_backend = *gpu::GPUBackend::get();
  GPU_shader_bind(shader, constants_state);
#ifndef NDEBUG
  gpu::debug_validate_binding_image_format();
#endif
  gpu_backend.compute_dispatch(groups_x_len, groups_y_len, groups_z_len);
}

void GPU_compute_dispatch_indirect(gpu::Shader *shader,
                                   gpu::StorageBuf *indirect_buf_,
                                   const gpu::shader::SpecializationConstants *constants_state)
{
  gpu::GPUBackend &gpu_backend = *gpu::GPUBackend::get();
  gpu::StorageBuf *indirect_buf = reinterpret_cast<gpu::StorageBuf *>(indirect_buf_);

  GPU_shader_bind(shader, constants_state);
#ifndef NDEBUG
  gpu::debug_validate_binding_image_format();
#endif
  gpu_backend.compute_dispatch_indirect(indirect_buf);
}

}  // namespace blender
