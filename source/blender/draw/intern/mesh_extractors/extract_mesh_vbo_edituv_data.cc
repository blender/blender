/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BKE_mesh.hh"

#include "extract_mesh.hh"

#include "draw_cache_impl.hh"

#include "draw_subdivision.hh"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Edit UV Data / Flags
 * \{ */

static const GPUVertFormat &edituv_data_format()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    /* WARNING: Adjust #EditLoopData struct accordingly. */
    GPU_vertformat_attr_add(&format, "data", GPU_COMP_U8, 4, GPU_FETCH_INT);
    GPU_vertformat_alias_add(&format, "flag");
  }
  return format;
}

static void extract_edituv_data_bm(const MeshRenderData &mr, MutableSpan<EditLoopData> vbo_data)
{
  const BMesh &bm = *mr.bm;
  const BMUVOffsets offsets = BM_uv_map_get_offsets(&bm);
  threading::parallel_for(IndexRange(bm.totface), 2048, [&](const IndexRange range) {
    for (const int face_index : range) {
      const BMFace &face = *BM_face_at_index(&const_cast<BMesh &>(bm), face_index);
      const BMLoop *loop = BM_FACE_FIRST_LOOP(&face);
      for ([[maybe_unused]] const int i : IndexRange(face.len)) {
        const int index = BM_elem_index_get(loop);
        EditLoopData &value = vbo_data[index];
        value = {};
        mesh_render_data_loop_flag(mr, loop, offsets, value);
        mesh_render_data_face_flag(mr, &face, offsets, value);
        mesh_render_data_loop_edge_flag(mr, loop, offsets, value);
        loop = loop->next;
      }
    }
  });
}

static void extract_edituv_data_mesh(const MeshRenderData &mr, MutableSpan<EditLoopData> vbo_data)
{
  const BMesh &bm = *mr.bm;
  const BMUVOffsets offsets = BM_uv_map_get_offsets(&bm);
  const OffsetIndices faces = mr.faces;
  const Span<int> corner_verts = mr.corner_verts;
  const Span<int> corner_edges = mr.corner_edges;
  threading::parallel_for(faces.index_range(), 2048, [&](const IndexRange range) {
    for (const int face_index : range) {
      const IndexRange face = faces[face_index];
      BMFace *face_orig = bm_original_face_get(mr, face_index);
      if (!face_orig) {
        vbo_data.slice(face).fill({});
        continue;
      }
      for (const int corner : face) {
        EditLoopData &value = vbo_data[corner];
        value = {};
        BMVert *vert = bm_original_vert_get(mr, corner_verts[corner]);
        BMEdge *edge = bm_original_edge_get(mr, corner_edges[corner]);
        if (edge && vert) {
          /* Loop on an edge endpoint. */
          BMLoop *l = BM_face_edge_share_loop(face_orig, edge);
          mesh_render_data_loop_flag(mr, l, offsets, value);
          mesh_render_data_loop_edge_flag(mr, l, offsets, value);
        }
        else {
          if (edge == nullptr) {
            /* Find if the loop's vert is not part of an edit edge.
             * For this, we check if the previous loop was on an edge. */
            const int corner_prev = bke::mesh::face_corner_prev(face, corner);
            edge = bm_original_edge_get(mr, corner_edges[corner_prev]);
          }
          if (edge) {
            /* Mapped points on an edge between two edit verts. */
            BMLoop *l = BM_face_edge_share_loop(face_orig, edge);
            mesh_render_data_loop_edge_flag(mr, l, offsets, value);
          }
        }
      }
    }
  });
}

void extract_edituv_data(const MeshRenderData &mr, gpu::VertBuf &vbo)
{
  GPU_vertbuf_init_with_format(vbo, edituv_data_format());
  GPU_vertbuf_data_alloc(vbo, mr.corners_num);
  MutableSpan vbo_data(static_cast<EditLoopData *>(GPU_vertbuf_get_data(vbo)), mr.corners_num);

  if (mr.extract_type == MR_EXTRACT_BMESH) {
    extract_edituv_data_bm(mr, vbo_data);
  }
  else {
    extract_edituv_data_mesh(mr, vbo_data);
  }
}

static void extract_edituv_data_iter_subdiv_bm(const MeshRenderData &mr,
                                               const BMUVOffsets offsets,
                                               const Span<int> subdiv_loop_vert_index,
                                               const Span<int> subdiv_loop_edge_index,
                                               const int subdiv_quad_index,
                                               const BMFace *coarse_quad,
                                               MutableSpan<EditLoopData> vbo_data)
{

  uint start_loop_idx = subdiv_quad_index * 4;
  uint end_loop_idx = (subdiv_quad_index + 1) * 4;
  for (uint i = start_loop_idx; i < end_loop_idx; i++) {
    const int vert_origindex = subdiv_loop_vert_index[i];
    int edge_origindex = subdiv_loop_edge_index[i];

    EditLoopData *edit_loop_data = &vbo_data[i];
    memset(edit_loop_data, 0, sizeof(EditLoopData));

    if (vert_origindex != -1 && edge_origindex != -1) {
      BMEdge *eed = BM_edge_at_index(mr.bm, edge_origindex);
      /* Loop on an edge endpoint. */
      BMLoop *l = BM_face_edge_share_loop(const_cast<BMFace *>(coarse_quad), eed);
      mesh_render_data_loop_flag(mr, l, offsets, *edit_loop_data);
      mesh_render_data_loop_edge_flag(mr, l, offsets, *edit_loop_data);
    }
    else {
      if (edge_origindex == -1) {
        /* Find if the loop's vert is not part of an edit edge.
         * For this, we check if the previous loop was on an edge. */
        const uint loop_index_last = (i == start_loop_idx) ? end_loop_idx - 1 : i - 1;
        edge_origindex = subdiv_loop_edge_index[loop_index_last];
      }
      if (edge_origindex != -1) {
        /* Mapped points on an edge between two edit verts. */
        BMEdge *eed = BM_edge_at_index(mr.bm, edge_origindex);
        BMLoop *l = BM_face_edge_share_loop(const_cast<BMFace *>(coarse_quad), eed);
        mesh_render_data_loop_edge_flag(mr, l, offsets, *edit_loop_data);
      }
    }

    mesh_render_data_face_flag(mr, coarse_quad, offsets, *edit_loop_data);
  }
}

static void extract_edituv_subdiv_data_bm(const MeshRenderData &mr,
                                          const DRWSubdivCache &subdiv_cache,
                                          MutableSpan<EditLoopData> vbo_data)
{
  const int corners_num = subdiv_cache.num_subdiv_loops;
  const Span<int> subdiv_loop_face_index(subdiv_cache.subdiv_loop_face_index, corners_num);
  const Span<int> subdiv_loop_vert_index(
      static_cast<const int *>(GPU_vertbuf_get_data(*subdiv_cache.verts_orig_index)), corners_num);
  /* NOTE: #subdiv_loop_edge_index already has the origindex layer baked in. */
  const Span<int> subdiv_loop_edge_index(
      static_cast<const int *>(GPU_vertbuf_get_data(*subdiv_cache.edges_orig_index)), corners_num);

  const BMUVOffsets offsets = BM_uv_map_get_offsets(mr.bm);
  threading::parallel_for(IndexRange(subdiv_cache.num_subdiv_quads), 2048, [&](IndexRange range) {
    for (const int subdiv_quad : range) {
      const int coarse_face = subdiv_loop_face_index[subdiv_quad * 4];
      extract_edituv_data_iter_subdiv_bm(mr,
                                         offsets,
                                         subdiv_loop_vert_index,
                                         subdiv_loop_edge_index,
                                         subdiv_quad,
                                         BM_face_at_index(mr.bm, coarse_face),
                                         vbo_data);
    }
  });
}

static void extract_edituv_subdiv_data_mesh(const MeshRenderData &mr,
                                            const DRWSubdivCache &subdiv_cache,
                                            MutableSpan<EditLoopData> vbo_data)
{
  const int corners_num = subdiv_cache.num_subdiv_loops;
  const Span<int> subdiv_loop_face_index(subdiv_cache.subdiv_loop_face_index, corners_num);
  const Span<int> subdiv_loop_vert_index(
      static_cast<const int *>(GPU_vertbuf_get_data(*subdiv_cache.verts_orig_index)), corners_num);
  /* NOTE: #subdiv_loop_edge_index already has the origindex layer baked in. */
  const Span<int> subdiv_loop_edge_index(
      static_cast<const int *>(GPU_vertbuf_get_data(*subdiv_cache.edges_orig_index)), corners_num);

  const BMUVOffsets offsets = BM_uv_map_get_offsets(mr.bm);
  threading::parallel_for(IndexRange(subdiv_cache.num_subdiv_quads), 2048, [&](IndexRange range) {
    for (const int subdiv_quad : range) {
      const int coarse_face = subdiv_loop_face_index[subdiv_quad * 4];
      extract_edituv_data_iter_subdiv_bm(mr,
                                         offsets,
                                         subdiv_loop_vert_index,
                                         subdiv_loop_edge_index,
                                         subdiv_quad,
                                         bm_original_face_get(mr, coarse_face),
                                         vbo_data);
    }
  });
}

void extract_edituv_data_subdiv(const MeshRenderData &mr,
                                const DRWSubdivCache &subdiv_cache,
                                gpu::VertBuf &vbo)
{
  GPU_vertbuf_init_with_format(vbo, edituv_data_format());
  const int size = subdiv_cache.num_subdiv_loops;
  GPU_vertbuf_data_alloc(vbo, size);
  MutableSpan vbo_data(static_cast<EditLoopData *>(GPU_vertbuf_get_data(vbo)), size);

  if (mr.extract_type == MR_EXTRACT_BMESH) {
    extract_edituv_subdiv_data_bm(mr, subdiv_cache, vbo_data);
  }
  else {
    extract_edituv_subdiv_data_mesh(mr, subdiv_cache, vbo_data);
  }
}

}  // namespace blender::draw
