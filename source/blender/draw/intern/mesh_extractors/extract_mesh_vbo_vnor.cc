/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BLI_array_utils.hh"

#include "GPU_attribute_convert.hh"

#include "extract_mesh.hh"

namespace blender::draw {

static void extract_vert_normals_mesh(const MeshRenderData &mr,
                                      MutableSpan<int1010102_norm> vbo_data)
{
  MutableSpan corners_data = vbo_data.take_front(mr.corners_num);
  MutableSpan loose_edge_data = vbo_data.slice(mr.corners_num, mr.loose_edges.size() * 2);
  MutableSpan loose_vert_data = vbo_data.take_back(mr.loose_verts.size());

  const Span<float3> vert_normals = mr.mesh->vert_normals();

  Array<int1010102_norm> converted(vert_normals.size());
  gpu::convert_normals(vert_normals, converted.as_mutable_span());
  static_assert(sizeof(int1010102_norm) == sizeof(int32_t));
  array_utils::gather(
      converted.as_span().cast<int32_t>(), mr.corner_verts, corners_data.cast<int32_t>());
  extract_mesh_loose_edge_data(converted.as_span(), mr.edges, mr.loose_edges, loose_edge_data);
  array_utils::gather(
      converted.as_span().cast<int32_t>(), mr.loose_verts, loose_vert_data.cast<int32_t>());
}

static void extract_vert_normals_bm(const MeshRenderData &mr,
                                    MutableSpan<int1010102_norm> vbo_data)
{
  const BMesh &bm = *mr.bm;
  MutableSpan corners_data = vbo_data.take_front(mr.corners_num);
  MutableSpan loose_edge_data = vbo_data.slice(mr.corners_num, mr.loose_edges.size() * 2);
  MutableSpan loose_vert_data = vbo_data.take_back(mr.loose_verts.size());

  threading::parallel_for(IndexRange(bm.totface), 2048, [&](const IndexRange range) {
    for (const int face_index : range) {
      const BMFace &face = *BM_face_at_index(&const_cast<BMesh &>(bm), face_index);
      const BMLoop *loop = BM_FACE_FIRST_LOOP(&face);
      for ([[maybe_unused]] const int i : IndexRange(face.len)) {
        const int index = BM_elem_index_get(loop);
        corners_data[index] = gpu::convert_normal<int1010102_norm>(bm_vert_no_get(mr, loop->v));
        loop = loop->next;
      }
    }
  });

  mr.loose_edges.foreach_index(
      [&](const int i, const int pos) {
        const BMEdge &edge = *BM_edge_at_index(&const_cast<BMesh &>(bm), i);
        loose_edge_data[pos * 2 + 0] = gpu::convert_normal<int1010102_norm>(
            bm_vert_no_get(mr, edge.v1));
        loose_edge_data[pos * 2 + 1] = gpu::convert_normal<int1010102_norm>(
            bm_vert_no_get(mr, edge.v2));
      },
      exec_mode::grain_size(2048));

  mr.loose_verts.foreach_index(
      [&](const int i, const int pos) {
        const BMVert &vert = *BM_vert_at_index(&const_cast<BMesh &>(bm), i);
        loose_vert_data[pos] = gpu::convert_normal<int1010102_norm>(bm_vert_no_get(mr, &vert));
      },
      exec_mode::grain_size(2048));
}

gpu::VertBufPtr extract_vert_normals(const MeshRenderData &mr)
{
  static GPUVertFormat format = GPU_vertformat_from_attribute("vnor",
                                                              gpu::VertAttrType::SNORM_10_10_10_2);

  gpu::VertBufPtr vbo = gpu::VertBufPtr(GPU_vertbuf_create_with_format(format));
  GPU_vertbuf_data_alloc(*vbo, mr.corners_num + mr.loose_indices_num);
  MutableSpan vbo_data = vbo->data<int1010102_norm>();

  if (mr.extract_type == MeshExtractType::Mesh) {
    extract_vert_normals_mesh(mr, vbo_data);
  }
  else {
    extract_vert_normals_bm(mr, vbo_data);
  }
  return vbo;
}

}  // namespace blender::draw
