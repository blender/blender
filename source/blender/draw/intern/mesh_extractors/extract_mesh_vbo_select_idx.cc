/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BLI_array_utils.hh"

#include "draw_subdivision.hh"
#include "extract_mesh.hh"

namespace blender::draw {

static MutableSpan<int> init_vbo_data(gpu::VertBuf &vbo, const int size)
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "index", GPU_COMP_I32, 1, GPU_FETCH_INT);
  }
  GPU_vertbuf_init_with_format(vbo, format);
  GPU_vertbuf_data_alloc(vbo, size);
  return {static_cast<int *>(GPU_vertbuf_get_data(vbo)), size};
}

/* TODO: Use #glVertexID to get loop index and use the data structure on the CPU to retrieve the
 * select element associated with this loop ID. This would remove the need for this separate
 * index VBO's. We could upload the p/e/orig_index_vert as a buffer texture and sample it inside
 * the shader to output original index. */

static void extract_vert_index_mesh(const MeshRenderData &mr, MutableSpan<int> vbo_data)
{
  MutableSpan corners_data = vbo_data.take_front(mr.corners_num);
  MutableSpan loose_edge_data = vbo_data.slice(mr.corners_num, mr.loose_edges.size() * 2);
  MutableSpan loose_vert_data = vbo_data.take_back(mr.loose_verts.size());

  if (mr.orig_index_vert) {
    const Span<int> orig_index_vert(mr.orig_index_vert, mr.verts_num);
    array_utils::gather(orig_index_vert, mr.corner_verts, corners_data);
    extract_mesh_loose_edge_data(orig_index_vert, mr.edges, mr.loose_edges, loose_edge_data);
    array_utils::gather(orig_index_vert, mr.loose_verts, loose_vert_data);
  }
  else {
    array_utils::copy(mr.corner_verts, corners_data);
    const Span<int2> edges = mr.edges;
    const Span<int> loose_edges = mr.loose_edges;
    threading::parallel_for(loose_edges.index_range(), 4096, [&](const IndexRange range) {
      for (const int i : range) {
        loose_edge_data[i * 2 + 0] = edges[loose_edges[i]][0];
        loose_edge_data[i * 2 + 1] = edges[loose_edges[i]][1];
      }
    });
    array_utils::copy(mr.loose_verts, loose_vert_data);
  }
}

static void extract_vert_index_bm(const MeshRenderData &mr, MutableSpan<int> vbo_data)
{
  MutableSpan corners_data = vbo_data.take_front(mr.corners_num);
  MutableSpan loose_edge_data = vbo_data.slice(mr.corners_num, mr.loose_edges.size() * 2);
  MutableSpan loose_vert_data = vbo_data.take_back(mr.loose_verts.size());

  const BMesh &bm = *mr.bm;
  threading::parallel_for(IndexRange(bm.totface), 2048, [&](const IndexRange range) {
    for (const int face_index : range) {
      const BMFace &face = *BM_face_at_index(&const_cast<BMesh &>(bm), face_index);
      const BMLoop *loop = BM_FACE_FIRST_LOOP(&face);
      for ([[maybe_unused]] const int i : IndexRange(face.len)) {
        const int index = BM_elem_index_get(loop);
        corners_data[index] = BM_elem_index_get(loop->v);
        loop = loop->next;
      }
    }
  });

  const Span<int> loose_edges = mr.loose_edges;
  threading::parallel_for(loose_edges.index_range(), 4096, [&](const IndexRange range) {
    for (const int i : range) {
      const BMEdge &edge = *BM_edge_at_index(&const_cast<BMesh &>(bm), loose_edges[i]);
      loose_edge_data[i * 2 + 0] = BM_elem_index_get(edge.v1);
      loose_edge_data[i * 2 + 1] = BM_elem_index_get(edge.v2);
    }
  });

  array_utils::copy(mr.loose_verts, loose_vert_data);
}

void extract_vert_index(const MeshRenderData &mr, gpu::VertBuf &vbo)
{
  MutableSpan<int> vbo_data = init_vbo_data(vbo, mr.corners_num + mr.loose_indices_num);
  if (mr.extract_type == MR_EXTRACT_MESH) {
    extract_vert_index_mesh(mr, vbo_data);
  }
  else {
    extract_vert_index_bm(mr, vbo_data);
  }
}

static void extract_edge_index_mesh(const MeshRenderData &mr, MutableSpan<int> vbo_data)
{
  MutableSpan corners_data = vbo_data.take_front(mr.corners_num);
  MutableSpan loose_edge_data = vbo_data.slice(mr.corners_num, mr.loose_edges.size() * 2);

  const Span<int> loose_edges = mr.loose_edges;
  if (mr.orig_index_edge) {
    const Span<int> orig_index_edge(mr.orig_index_edge, mr.edges_num);
    array_utils::gather(orig_index_edge, mr.corner_edges, corners_data);
    threading::parallel_for(loose_edges.index_range(), 4096, [&](const IndexRange range) {
      for (const int i : range) {
        loose_edge_data[i * 2 + 0] = orig_index_edge[loose_edges[i]];
        loose_edge_data[i * 2 + 1] = orig_index_edge[loose_edges[i]];
      }
    });
  }
  else {
    array_utils::copy(mr.corner_edges, corners_data);
    threading::parallel_for(loose_edges.index_range(), 4096, [&](const IndexRange range) {
      for (const int i : range) {
        loose_edge_data[i * 2 + 0] = loose_edges[i];
        loose_edge_data[i * 2 + 1] = loose_edges[i];
      }
    });
  }
}

static void extract_edge_index_bm(const MeshRenderData &mr, MutableSpan<int> vbo_data)
{
  MutableSpan corners_data = vbo_data.take_front(mr.corners_num);
  MutableSpan loose_edge_data = vbo_data.slice(mr.corners_num, mr.loose_edges.size() * 2);

  const BMesh &bm = *mr.bm;
  threading::parallel_for(IndexRange(bm.totface), 2048, [&](const IndexRange range) {
    for (const int face_index : range) {
      const BMFace &face = *BM_face_at_index(&const_cast<BMesh &>(bm), face_index);
      const BMLoop *loop = BM_FACE_FIRST_LOOP(&face);
      for ([[maybe_unused]] const int i : IndexRange(face.len)) {
        const int index = BM_elem_index_get(loop);
        corners_data[index] = BM_elem_index_get(loop->e);
        loop = loop->next;
      }
    }
  });

  const Span<int> loose_edges = mr.loose_edges;
  threading::parallel_for(loose_edges.index_range(), 4096, [&](const IndexRange range) {
    for (const int i : range) {
      loose_edge_data[i * 2 + 0] = loose_edges[i];
      loose_edge_data[i * 2 + 1] = loose_edges[i];
    }
  });
}

void extract_edge_index(const MeshRenderData &mr, gpu::VertBuf &vbo)
{
  MutableSpan<int> vbo_data = init_vbo_data(vbo, mr.corners_num + mr.loose_edges.size() * 2);
  if (mr.extract_type == MR_EXTRACT_MESH) {
    extract_edge_index_mesh(mr, vbo_data);
  }
  else {
    extract_edge_index_bm(mr, vbo_data);
  }
}

static void extract_face_index_mesh(const MeshRenderData &mr, MutableSpan<int> vbo_data)
{
  const OffsetIndices faces = mr.faces;
  if (mr.orig_index_face) {
    const Span<int> orig_index_face(mr.orig_index_face, mr.edges_num);
    threading::parallel_for(faces.index_range(), 2048, [&](const IndexRange range) {
      for (const int face : range) {
        vbo_data.slice(faces[face]).fill(orig_index_face[face]);
      }
    });
  }
  else {
    offset_indices::build_reverse_map(faces, vbo_data);
  }
}

static void extract_face_index_bm(const MeshRenderData &mr, MutableSpan<int> vbo_data)
{
  const BMesh &bm = *mr.bm;
  threading::parallel_for(IndexRange(bm.totface), 2048, [&](const IndexRange range) {
    for (const int face_index : range) {
      const BMFace &face = *BM_face_at_index(&const_cast<BMesh &>(bm), face_index);
      const IndexRange face_range(BM_elem_index_get(BM_FACE_FIRST_LOOP(&face)), face.len);
      vbo_data.slice(face_range).fill(face_index);
    }
  });
}

void extract_face_index(const MeshRenderData &mr, gpu::VertBuf &vbo)
{
  MutableSpan<int> vbo_data = init_vbo_data(vbo, mr.corners_num);
  if (mr.extract_type == MR_EXTRACT_MESH) {
    extract_face_index_mesh(mr, vbo_data);
  }
  else {
    extract_face_index_bm(mr, vbo_data);
  }
}

static void extract_vert_idx_loose_geom_subdiv(const DRWSubdivCache &subdiv_cache,
                                               const MeshRenderData &mr,
                                               gpu::VertBuf &vbo)
{
  const Span<int> loose_verts = mr.loose_verts;
  const Span<int> loose_edges = mr.loose_edges;
  if (loose_edges.is_empty() && loose_verts.is_empty()) {
    return;
  }

  MutableSpan<int32_t> vbo_data(static_cast<int32_t *>(GPU_vertbuf_get_data(vbo)),
                                subdiv_full_vbo_size(mr, subdiv_cache));

  const Span<int2> coarse_edges = mr.edges;
  const int verts_per_edge = subdiv_verts_per_coarse_edge(subdiv_cache);
  MutableSpan<int32_t> edge_data = vbo_data.slice(subdiv_cache.num_subdiv_loops,
                                                  loose_edges.size() * verts_per_edge);
  for (const int i : loose_edges.index_range()) {
    const int2 edge = coarse_edges[loose_edges[i]];
    MutableSpan data = edge_data.slice(i * verts_per_edge, verts_per_edge);
    data.first() = mr.orig_index_vert ? mr.orig_index_vert[edge[0]] : edge[0];
    data.last() = mr.orig_index_vert ? mr.orig_index_vert[edge[1]] : edge[1];
  }

  MutableSpan<int32_t> loose_vert_data = vbo_data.take_back(loose_verts.size());
  if (mr.orig_index_vert) {
    array_utils::gather(Span(mr.orig_index_vert, mr.verts_num), loose_verts, loose_vert_data);
  }
  else {
    array_utils::copy(loose_verts, loose_vert_data);
  }
}

void extract_vert_index_subdiv(const DRWSubdivCache &subdiv_cache,
                               const MeshRenderData &mr,
                               gpu::VertBuf &vbo)
{
  /* Each element points to an element in the `ibo.points`. */
  draw_subdiv_init_origindex_buffer(
      vbo,
      (int32_t *)GPU_vertbuf_get_data(*subdiv_cache.verts_orig_index),
      subdiv_cache.num_subdiv_loops,
      subdiv_full_vbo_size(mr, subdiv_cache));
  if (!mr.orig_index_vert) {
    return;
  }

  /* Remap the vertex indices to those pointed by the origin indices layer. At this point, the
   * VBO data is a copy of #verts_orig_index which contains the coarse vertices indices, so
   * the memory can both be accessed for lookup and immediately overwritten. */
  int32_t *vbo_data = static_cast<int32_t *>(GPU_vertbuf_get_data(vbo));
  for (int i = 0; i < subdiv_cache.num_subdiv_loops; i++) {
    if (vbo_data[i] == -1) {
      continue;
    }
    vbo_data[i] = mr.orig_index_vert[vbo_data[i]];
  }
  extract_vert_idx_loose_geom_subdiv(subdiv_cache, mr, vbo);
}

static void extract_edge_idx_loose_geom_subdiv(const DRWSubdivCache &subdiv_cache,
                                               const MeshRenderData &mr,
                                               gpu::VertBuf &vbo)
{
  const Span<int> loose_edges = mr.loose_edges;
  if (loose_edges.is_empty()) {
    return;
  }

  MutableSpan<int32_t> vbo_data(static_cast<int32_t *>(GPU_vertbuf_get_data(vbo)),
                                subdiv_full_vbo_size(mr, subdiv_cache));

  const int verts_per_edge = subdiv_verts_per_coarse_edge(subdiv_cache);
  MutableSpan data = vbo_data.slice(subdiv_cache.num_subdiv_loops,
                                    loose_edges.size() * verts_per_edge);
  for (const int i : loose_edges.index_range()) {
    const int edge = loose_edges[i];
    const int index = mr.orig_index_edge ? mr.orig_index_edge[edge] : edge;
    data.slice(i * verts_per_edge, verts_per_edge).fill(index);
  }
}

void extract_edge_index_subdiv(const DRWSubdivCache &subdiv_cache,
                               const MeshRenderData &mr,
                               gpu::VertBuf &vbo)
{
  draw_subdiv_init_origindex_buffer(
      vbo,
      static_cast<int32_t *>(GPU_vertbuf_get_data(*subdiv_cache.edges_orig_index)),
      subdiv_cache.num_subdiv_loops,
      subdiv_loose_edges_num(mr, subdiv_cache) * 2);
  extract_edge_idx_loose_geom_subdiv(subdiv_cache, mr, vbo);
}

void extract_face_index_subdiv(const DRWSubdivCache &subdiv_cache,
                               const MeshRenderData &mr,
                               gpu::VertBuf &vbo)
{
  draw_subdiv_init_origindex_buffer(
      vbo, subdiv_cache.subdiv_loop_face_index, subdiv_cache.num_subdiv_loops, 0);

  if (!mr.orig_index_face) {
    return;
  }

  /* Remap the face indices to those pointed by the origin indices layer. At this point, the
   * VBO data is a copy of #subdiv_loop_face_index which contains the coarse face indices, so
   * the memory can both be accessed for lookup and immediately overwritten. */
  int32_t *vbo_data = static_cast<int32_t *>(GPU_vertbuf_get_data(vbo));
  for (int i = 0; i < subdiv_cache.num_subdiv_loops; i++) {
    vbo_data[i] = mr.orig_index_face[vbo_data[i]];
  }
}

void extract_face_dot_index(const MeshRenderData &mr, gpu::VertBuf &vbo)
{
  MutableSpan<int> vbo_data = init_vbo_data(vbo, mr.faces_num);
  if (mr.extract_type == MR_EXTRACT_MESH) {
    if (mr.orig_index_face) {
      const Span<int> orig_index_face(mr.orig_index_face, mr.faces_num);
      array_utils::copy(orig_index_face, vbo_data);
    }
    else {
      array_utils::fill_index_range(vbo_data);
    }
  }
  else {
    array_utils::fill_index_range(vbo_data);
  }
}

}  // namespace blender::draw
