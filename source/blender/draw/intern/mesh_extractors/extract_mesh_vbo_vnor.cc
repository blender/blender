/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BLI_array_utils.hh"

#include "extract_mesh.hh"

#include "draw_subdivision.hh"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Vertex Normal
 * \{ */

static void extract_vert_normals_mesh(const MeshRenderData &mr,
                                      MutableSpan<GPUPackedNormal> vbo_data)
{
  MutableSpan corners_data = vbo_data.take_front(mr.corners_num);
  MutableSpan loose_edge_data = vbo_data.slice(mr.corners_num, mr.loose_edges.size() * 2);
  MutableSpan loose_vert_data = vbo_data.take_back(mr.loose_verts.size());

  Array<GPUPackedNormal> converted(mr.vert_normals.size());
  convert_normals(mr.vert_normals, converted.as_mutable_span());
  array_utils::gather(converted.as_span(), mr.corner_verts, corners_data);
  extract_mesh_loose_edge_data(converted.as_span(), mr.edges, mr.loose_edges, loose_edge_data);
  array_utils::gather(converted.as_span(), mr.loose_verts, loose_vert_data);
}

static void extract_vert_normals_bm(const MeshRenderData &mr,
                                    MutableSpan<GPUPackedNormal> vbo_data)
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
        corners_data[index] = GPU_normal_convert_i10_v3(bm_vert_no_get(mr, loop->v));
        loop = loop->next;
      }
    }
  });

  const Span<int> loose_edges = mr.loose_edges;
  threading::parallel_for(loose_edges.index_range(), 4096, [&](const IndexRange range) {
    for (const int i : range) {
      const BMEdge &edge = *BM_edge_at_index(&const_cast<BMesh &>(bm), loose_edges[i]);
      loose_edge_data[i * 2 + 0] = GPU_normal_convert_i10_v3(bm_vert_no_get(mr, edge.v1));
      loose_edge_data[i * 2 + 1] = GPU_normal_convert_i10_v3(bm_vert_no_get(mr, edge.v2));
    }
  });

  const Span<int> loose_verts = mr.loose_verts;
  threading::parallel_for(loose_verts.index_range(), 2048, [&](const IndexRange range) {
    for (const int i : range) {
      const BMVert &vert = *BM_vert_at_index(&const_cast<BMesh &>(bm), loose_verts[i]);
      loose_vert_data[i] = GPU_normal_convert_i10_v3(bm_vert_no_get(mr, &vert));
    }
  });
}

static void extract_vnor_init(const MeshRenderData &mr,
                              MeshBatchCache & /*cache*/,
                              void *buf,
                              void * /*tls_data*/)
{
  gpu::VertBuf *vbo = static_cast<gpu::VertBuf *>(buf);
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "vnor", GPU_COMP_I10, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
  }
  const int size = mr.corners_num + mr.loose_indices_num;
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, size);
  MutableSpan vbo_data(static_cast<GPUPackedNormal *>(GPU_vertbuf_get_data(vbo)), size);

  if (mr.extract_type == MR_EXTRACT_MESH) {
    extract_vert_normals_mesh(mr, vbo_data);
  }
  else {
    extract_vert_normals_bm(mr, vbo_data);
  }
}

constexpr MeshExtract create_extractor_vnor()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_vnor_init;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.vnor);
  return extractor;
}

/** \} */

const MeshExtract extract_vnor = create_extractor_vnor();

}  // namespace blender::draw
