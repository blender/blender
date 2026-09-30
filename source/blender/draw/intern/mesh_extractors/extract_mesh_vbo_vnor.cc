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
                                      MutableSpan<int1010102_norm> vert_data,
                                      MutableSpan<int1010102_norm> loose_data)
{
  MutableSpan loose_edge_data = loose_data.take_front(mr.loose_edges.size() * 2);
  MutableSpan loose_vert_data = loose_data.take_back(mr.loose_verts.size());
  gpu::convert_normals(mr.mesh->vert_normals(), vert_data);
  extract_mesh_loose_edge_data(vert_data.as_span(), mr.edges, mr.loose_edges, loose_edge_data);
  static_assert(sizeof(int1010102_norm) == sizeof(int32_t));
  array_utils::gather(
      vert_data.as_span().cast<int32_t>(), mr.loose_verts, loose_vert_data.cast<int32_t>());
}

static void extract_vert_normals_bm(const MeshRenderData &mr,
                                    MutableSpan<int1010102_norm> vert_data,
                                    MutableSpan<int1010102_norm> loose_data)
{
  const BMesh &bm = *mr.bm;
  MutableSpan loose_edge_data = loose_data.take_front(mr.loose_edges.size() * 2);
  MutableSpan loose_vert_data = loose_data.take_back(mr.loose_verts.size());

  threading::parallel_for(IndexRange(bm.totvert), 4096, [&](const IndexRange range) {
    for (const int i : range) {
      const BMVert *vert = BM_vert_at_index(&const_cast<BMesh &>(bm), i);
      vert_data[i] = gpu::convert_normal<int1010102_norm>(bm_vert_no_get(mr, vert));
    }
  });

  mr.loose_edges.foreach_index(
      [&](const int i, const int pos) {
        const BMEdge &edge = *BM_edge_at_index(&const_cast<BMesh &>(bm), i);
        loose_edge_data[pos * 2 + 0] = vert_data[BM_elem_index_get(edge.v1)];
        loose_edge_data[pos * 2 + 1] = vert_data[BM_elem_index_get(edge.v2)];
      },
      exec_mode::grain_size(2048));

  mr.loose_verts.foreach_index(
      [&](const int i, const int pos) { loose_vert_data[pos] = vert_data[i]; },
      exec_mode::grain_size(2048));
}

gpu::VertBufPtr extract_vert_normals(const MeshRenderData &mr, MeshBufferCache &cache)
{
  static GPUVertFormat format = GPU_vertformat_from_attribute("vnor",
                                                              gpu::VertAttrType::SNORM_10_10_10_2);

  gpu::VertBufPtr vert_vbo = gpu::VertBufPtr(GPU_vertbuf_create_with_format(format));
  GPU_vertbuf_data_alloc(*vert_vbo, mr.verts_num);
  Array<int1010102_norm> loose_data(mr.loose_indices_num);
  if (mr.extract_type == MeshExtractType::Mesh) {
    extract_vert_normals_mesh(mr, vert_vbo->data<int1010102_norm>(), loose_data);
  }
  else {
    extract_vert_normals_bm(mr, vert_vbo->data<int1010102_norm>(), loose_data);
  }

  gpu::VertBufPtr vbo = gpu::VertBufPtr(
      GPU_vertbuf_create_on_device(format, mr.corners_num + mr.loose_indices_num));
  gather_vert_to_corner_gpu(mr, cache, *vert_vbo, *vbo);
  loose_data_upload(mr, loose_data.as_span(), *vbo);
  return vbo;
}

}  // namespace blender::draw
