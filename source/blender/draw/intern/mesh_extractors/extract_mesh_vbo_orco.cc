/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "extract_mesh.hh"

namespace blender::draw {

gpu::VertBufPtr extract_orco(const MeshRenderData &mr, MeshBufferCache &cache)
{
  const Span<float3> orco_data(
      static_cast<const float3 *>(CustomData_get_layer(&mr.mesh->vert_data, CD_ORCO)),
      mr.corners_num);

  /* FIXME(fclem): We use the last component as a way to differentiate from generic vertex
   * attributes. This is a substantial waste of video-ram and should be done another way.
   * Unfortunately, at the time of writing, I did not found any other "non disruptive"
   * alternative. */
  static const GPUVertFormat format = GPU_vertformat_from_attribute(
      "orco", gpu::VertAttrType::SFLOAT_32_32_32_32);

  gpu::VertBufPtr vert_vbo = gpu::VertBufPtr(GPU_vertbuf_create_with_format(format));
  GPU_vertbuf_data_alloc(*vert_vbo, mr.verts_num);
  MutableSpan vbo_data = vert_vbo->data<float4>();
  const int64_t bytes = orco_data.size_in_bytes() + vbo_data.size_in_bytes();
  threading::memory_bandwidth_bound_task(bytes, [&]() {
    threading::parallel_for(IndexRange(mr.verts_num), 2048, [&](const IndexRange range) {
      for (const int vert : range) {
        vbo_data[vert] = float4(orco_data[vert], 0.0f);
      }
    });
  });

  gpu::VertBufPtr vbo = gpu::VertBufPtr(GPU_vertbuf_create_on_device(format, mr.corners_num));
  gather_vert_to_corner_gpu(mr, cache, *vert_vbo, *vbo);
  return vbo;
}

}  // namespace blender::draw
