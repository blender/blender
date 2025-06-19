/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "extract_mesh.hh"

namespace blender::draw {

gpu::VertBufPtr extract_orco(const MeshRenderData &mr)
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

  gpu::VertBufPtr vbo = gpu::VertBufPtr(GPU_vertbuf_create_with_format(format));
  GPU_vertbuf_data_alloc(*vbo, mr.corners_num);
  MutableSpan vbo_data = vbo->data<float4>();

  const int64_t bytes = orco_data.size_in_bytes() + vbo_data.size_in_bytes();
  threading::memory_bandwidth_bound_task(bytes, [&]() {
    if (mr.extract_type == MeshExtractType::BMesh) {
      const BMesh &bm = *mr.bm;
      threading::parallel_for(IndexRange(bm.totface), 2048, [&](const IndexRange range) {
        for (const int face_index : range) {
          const BMFace &face = *BM_face_at_index(&const_cast<BMesh &>(bm), face_index);
          const BMLoop *loop = BM_FACE_FIRST_LOOP(&face);
          for ([[maybe_unused]] const int i : IndexRange(face.len)) {
            const int index = BM_elem_index_get(loop);
            vbo_data[index] = float4(orco_data[BM_elem_index_get(loop->v)], 0.0f);
            loop = loop->next;
          }
        }
      });
    }
    else {
      const Span<int> corner_verts = mr.corner_verts;
      threading::parallel_for(corner_verts.index_range(), 4096, [&](const IndexRange range) {
        for (const int corner : range) {
          vbo_data[corner] = float4(orco_data[corner_verts[corner]], 0.0f);
        }
      });
    }
  });
  return vbo;
}

}  // namespace blender::draw
