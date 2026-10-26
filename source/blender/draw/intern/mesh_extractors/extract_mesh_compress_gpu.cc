/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "GPU_capabilities.hh"
#include "GPU_compute.hh"
#include "GPU_shader.hh"
#include "GPU_state.hh"

#include "draw_shader.hh"

#include "extract_mesh.hh"

namespace blender::draw {

static void dispatch_batched(gpu::Shader *shader, const int size)
{
  const int64_t max_batch_size = int64_t(GPU_max_work_group_count(0)) *
                                 DRW_MESH_COMPRESS_GROUP_SIZE;
  int start = 0;
  while (start < size) {
    const int batch_size = std::min(int64_t(size - start), max_batch_size);
    GPU_shader_uniform_1i(shader, "start", start);
    GPU_shader_uniform_1i(shader, "count", batch_size);
    GPU_compute_dispatch(shader, divide_ceil_u(batch_size, DRW_MESH_COMPRESS_GROUP_SIZE), 1, 1);
    start += batch_size;
  }
}

void mesh_compress_normals(gpu::VertBuf &src, gpu::VertBuf &dst)
{
  BLI_assert(GPU_vertbuf_get_vertex_len(&src) == GPU_vertbuf_get_vertex_len(&dst));

  uint elem_count = GPU_vertbuf_get_vertex_len(&src);
  if (elem_count == 0) {
    return;
  }
  bool use_hq = (dst.format.stride == 8);
  gpu::Shader *shader = DRW_shader_mesh_compress_normals_get(use_hq);
  GPU_shader_bind(shader);
  GPU_vertbuf_bind_as_ssbo(&src, 0);
  GPU_vertbuf_bind_as_ssbo(&dst, 1);
  dispatch_batched(shader, elem_count);
  GPU_memory_barrier(GPU_BARRIER_VERTEX_ATTRIB_ARRAY | GPU_BARRIER_SHADER_STORAGE);
  GPU_shader_unbind();
}

}  // namespace blender::draw
