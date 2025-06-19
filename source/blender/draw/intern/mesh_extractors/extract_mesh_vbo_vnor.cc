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
                                      MutableSpan<gpu::PackedNormal> vbo_data)
{
  MutableSpan corners_data = vbo_data.take_front(mr.corners_num);
  MutableSpan loose_edge_data = vbo_data.slice(mr.corners_num, mr.loose_edges.size() * 2);
  MutableSpan loose_vert_data = vbo_data.take_back(mr.loose_verts.size());

  const Span<float3> vert_normals = mr.mesh->vert_normals();

  Array<gpu::PackedNormal> converted(vert_normals.size());
  convert_normals(vert_normals, converted.as_mutable_span());
  array_utils::gather(converted.as_span(), mr.corner_verts, corners_data);
  extract_mesh_loose_edge_data(converted.as_span(), mr.edges, mr.loose_edges, loose_edge_data);
  array_utils::gather(converted.as_span(), mr.loose_verts, loose_vert_data);
}

static void extract_vert_normals_bm(const MeshRenderData &mr,
                                    MutableSpan<gpu::PackedNormal> vbo_data)
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
        corners_data[index] = gpu::convert_normal<gpu::PackedNormal>(bm_vert_no_get(mr, loop->v));
        loop = loop->next;
      }
    }
  });

  const Span<int> loose_edges = mr.loose_edges;
  threading::parallel_for(loose_edges.index_range(), 4096, [&](const IndexRange range) {
    for (const int i : range) {
      const BMEdge &edge = *BM_edge_at_index(&const_cast<BMesh &>(bm), loose_edges[i]);
      loose_edge_data[i * 2 + 0] = gpu::convert_normal<gpu::PackedNormal>(
          bm_vert_no_get(mr, edge.v1));
      loose_edge_data[i * 2 + 1] = gpu::convert_normal<gpu::PackedNormal>(
          bm_vert_no_get(mr, edge.v2));
    }
  });

  const Span<int> loose_verts = mr.loose_verts;
  threading::parallel_for(loose_verts.index_range(), 2048, [&](const IndexRange range) {
    for (const int i : range) {
      const BMVert &vert = *BM_vert_at_index(&const_cast<BMesh &>(bm), loose_verts[i]);
      loose_vert_data[i] = gpu::convert_normal<gpu::PackedNormal>(bm_vert_no_get(mr, &vert));
    }
  });
}

gpu::VertBufPtr extract_vert_normals(const MeshRenderData &mr)
{
  static GPUVertFormat format = GPU_vertformat_from_attribute("vnor",
                                                              gpu::VertAttrType::SNORM_10_10_10_2);

  gpu::VertBufPtr vbo = gpu::VertBufPtr(GPU_vertbuf_create_with_format(format));
  GPU_vertbuf_data_alloc(*vbo, mr.corners_num + mr.loose_indices_num);
  MutableSpan vbo_data = vbo->data<gpu::PackedNormal>();

  if (mr.extract_type == MeshExtractType::Mesh) {
    extract_vert_normals_mesh(mr, vbo_data);
  }
  else {
    extract_vert_normals_bm(mr, vbo_data);
  }
  return vbo;
}

}  // namespace blender::draw
