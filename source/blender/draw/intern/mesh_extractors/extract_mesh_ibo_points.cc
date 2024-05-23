/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BLI_array_utils.hh"

#include "GPU_index_buffer.hh"

#include "draw_subdivision.hh"
#include "extract_mesh.hh"

namespace blender::draw {

static IndexMask calc_vert_visibility_mesh(const MeshRenderData &mr,
                                           const IndexMask &mask,
                                           IndexMaskMemory &memory)
{
  IndexMask visible = mask;
  if (!mr.hide_vert.is_empty()) {
    visible = IndexMask::from_bools_inverse(visible, mr.hide_vert, memory);
  }
  if (mr.orig_index_vert != nullptr) {
    const int *orig_index = mr.orig_index_vert;
    visible = IndexMask::from_predicate(visible, GrainSize(4096), memory, [&](const int64_t i) {
      return orig_index[i] != ORIGINDEX_NONE;
    });
  }
  return visible;
}

/**
 * Fill the index buffer in a parallel non-deterministic fashion. This is okay because any of the
 * possible face corner indices are correct, since they all correspond to the same #Mesh vertex.
 * The separate arrays exist as a performance optimization to avoid writing to the VBO.
 */
template<typename Fn>
static void process_ibo_verts_mesh(const MeshRenderData &mr, const Fn &process_vert_fn)
{
  const Span<int> corner_verts = mr.corner_verts;
  threading::parallel_for(corner_verts.index_range(), 2048, [&](const IndexRange range) {
    for (const int corner : range) {
      process_vert_fn(corner, corner_verts[corner]);
    }
  });

  const int loose_edges_start = mr.corners_num;
  const Span<int2> edges = mr.edges;
  const Span<int> loose_edges = mr.loose_edges;
  threading::parallel_for(loose_edges.index_range(), 2048, [&](const IndexRange range) {
    for (const int i : range) {
      const int2 edge = edges[loose_edges[i]];
      process_vert_fn(loose_edges_start + i * 2 + 0, edge[0]);
      process_vert_fn(loose_edges_start + i * 2 + 1, edge[1]);
    }
  });

  const int loose_verts_start = mr.corners_num + loose_edges.size() * 2;
  const Span<int> loose_verts = mr.loose_verts;
  threading::parallel_for(loose_verts.index_range(), 2048, [&](const IndexRange range) {
    for (const int i : range) {
      process_vert_fn(loose_verts_start + i, loose_verts[i]);
    }
  });
}

static void extract_points_mesh(const MeshRenderData &mr, gpu::IndexBuf &points)
{
  IndexMaskMemory memory;
  const IndexMask visible_verts = calc_vert_visibility_mesh(mr, IndexMask(mr.verts_num), memory);

  const int max_index = mr.corners_num + mr.loose_edges.size() * 2 + mr.loose_verts.size();
  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder, GPU_PRIM_POINTS, visible_verts.size(), max_index);
  MutableSpan<uint> data = GPU_indexbuf_get_data(&builder);

  threading::memory_bandwidth_bound_task(mr.corner_verts.size_in_bytes(), [&]() {
    if (visible_verts.size() == mr.verts_num) {
      Array<bool> used(mr.verts_num, false);
      process_ibo_verts_mesh(mr, [&](const int ibo_index, const int vert) {
        if (!used[vert]) {
          data[vert] = ibo_index;
          used[vert] = true;
        }
      });
    }
    else {
      /* Compress the vertex indices into the smaller range of visible vertices in the IBO. */
      Array<int> map(mr.verts_num, -1);
      index_mask::build_reverse_map(visible_verts, map.as_mutable_span());
      process_ibo_verts_mesh(mr, [&](const int ibo_index, const int vert) {
        if (map[vert] != -1) {
          data[map[vert]] = ibo_index;
          map[vert] = -1;
        }
      });
    }
  });

  GPU_indexbuf_build_in_place_ex(&builder, 0, max_index, false, &points);
}

template<typename Fn>
static void process_ibo_verts_bm(const MeshRenderData &mr, const Fn &process_vert_fn)
{
  BMesh &bm = *mr.bm;

  threading::parallel_for(IndexRange(mr.verts_num), 4096, [&](const IndexRange range) {
    for (const int i : range) {
      BMVert &vert = *BM_vert_at_index(&bm, i);
      if (const BMLoop *loop = BM_vert_find_first_loop(&vert)) {
        process_vert_fn(BM_elem_index_get(loop), i);
      }
    }
  });

  const int loose_edges_start = mr.corners_num;
  const Span<int> loose_edges = mr.loose_edges;
  threading::parallel_for(loose_edges.index_range(), 4096, [&](const IndexRange range) {
    for (const int i : range) {
      const BMEdge &edge = *BM_edge_at_index(&bm, loose_edges[i]);
      process_vert_fn(loose_edges_start + i * 2 + 0, BM_elem_index_get(edge.v1));
      process_vert_fn(loose_edges_start + i * 2 + 1, BM_elem_index_get(edge.v2));
    }
  });

  const int loose_verts_start = mr.corners_num + loose_edges.size() * 2;
  const Span<int> loose_verts = mr.loose_verts;
  threading::parallel_for(loose_verts.index_range(), 4096, [&](const IndexRange range) {
    for (const int i : range) {
      process_vert_fn(loose_verts_start + i, loose_verts[i]);
    }
  });
}

static void extract_points_bm(const MeshRenderData &mr, gpu::IndexBuf &points)
{
  BMesh &bm = *mr.bm;

  IndexMaskMemory memory;
  const IndexMask visible_verts = IndexMask::from_predicate(
      IndexRange(bm.totvert), GrainSize(4096), memory, [&](const int i) {
        return !BM_elem_flag_test_bool(BM_vert_at_index(&const_cast<BMesh &>(bm), i),
                                       BM_ELEM_HIDDEN);
      });

  const int max_index = mr.corners_num + mr.loose_edges.size() * 2 + mr.loose_verts.size();
  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder, GPU_PRIM_POINTS, visible_verts.size(), max_index);
  MutableSpan<uint> data = GPU_indexbuf_get_data(&builder);

  if (mr.loose_verts.is_empty() && mr.loose_edges.is_empty()) {
    /* Make use of BMesh's vertex to loop topology knowledge to iterate over verts instead of
     * iterating over faces and defining points implicitly as done in the #Mesh extraction. */
    visible_verts.foreach_index(GrainSize(4096), [&](const int i, const int pos) {
      BMVert &vert = *BM_vert_at_index(&bm, i);
      data[pos] = BM_elem_index_get(BM_vert_find_first_loop(&vert));
    });
  }
  else if (visible_verts.size() == bm.totvert) {
    Array<bool> used(mr.verts_num, false);
    process_ibo_verts_bm(mr, [&](const int ibo_index, const int vert) {
      if (!used[vert]) {
        data[vert] = ibo_index;
        used[vert] = true;
      }
    });
  }
  else {
    /* Compress the vertex indices into the smaller range of visible vertices in the IBO. */
    Array<int> map(mr.verts_num, -1);
    index_mask::build_reverse_map(visible_verts, map.as_mutable_span());
    process_ibo_verts_bm(mr, [&](const int ibo_index, const int vert) {
      if (map[vert] != -1) {
        data[map[vert]] = ibo_index;
        map[vert] = -1;
      }
    });
  }

  GPU_indexbuf_build_in_place_ex(&builder, 0, max_index, false, &points);
}

void extract_points(const MeshRenderData &mr, gpu::IndexBuf &points)
{
  if (mr.extract_type == MR_EXTRACT_MESH) {
    extract_points_mesh(mr, points);
  }
  else {
    extract_points_bm(mr, points);
  }
}

static IndexMask calc_vert_visibility_mapped_mesh(const MeshRenderData &mr,
                                                  const IndexMask &mask,
                                                  const Span<int> map,
                                                  IndexMaskMemory &memory)
{
  IndexMask visible = mask;
  if (!mr.hide_vert.is_empty()) {
    const Span<bool> hide_vert = mr.hide_vert;
    visible = IndexMask::from_predicate(
        visible, GrainSize(4096), memory, [&](const int i) { return !hide_vert[map[i]]; });
  }
  if (mr.orig_index_vert != nullptr) {
    const int *orig_index = mr.orig_index_vert;
    visible = IndexMask::from_predicate(visible, GrainSize(4096), memory, [&](const int i) {
      return orig_index[map[i]] != ORIGINDEX_NONE;
    });
  }
  return visible;
}

static void extract_points_subdiv_mesh(const MeshRenderData &mr,
                                       const DRWSubdivCache &subdiv_cache,
                                       gpu::IndexBuf &points)
{
  const Span<int2> coarse_edges = mr.edges;
  const Span<int> loose_verts = mr.loose_verts;
  const Span<int> loose_edges = mr.loose_edges;
  const int verts_per_edge = subdiv_verts_per_coarse_edge(subdiv_cache);
  const int loose_edge_verts_num = verts_per_edge * loose_edges.size();

  const Span<bool> hide_vert = mr.hide_vert;
  const Span<int> corner_orig_verts{
      static_cast<int *>(GPU_vertbuf_get_data(subdiv_cache.verts_orig_index)),
      subdiv_cache.num_subdiv_loops};

  IndexMaskMemory memory;
  IndexMask visible_corners = IndexMask::from_predicate(
      corner_orig_verts.index_range(), GrainSize(4096), memory, [&](const int i) {
        return corner_orig_verts[i] != -1;
      });
  visible_corners = calc_vert_visibility_mapped_mesh(
      mr, visible_corners, corner_orig_verts, memory);

  const IndexMask visible_loose = calc_vert_visibility_mapped_mesh(
      mr, IndexMask(loose_verts.size()), loose_verts, memory);

  const int max_index = subdiv_cache.num_subdiv_loops + loose_edge_verts_num + loose_verts.size();
  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder,
                    GPU_PRIM_POINTS,
                    visible_corners.size() + loose_edges.size() * 2 + visible_loose.size(),
                    max_index);
  MutableSpan<uint> data = GPU_indexbuf_get_data(&builder);

  visible_corners.to_indices<int32_t>(data.take_front(visible_corners.size()).cast<int32_t>());

  const auto show_vert = [&](const int vert) {
    if (!hide_vert.is_empty() && hide_vert[vert]) {
      return false;
    }
    if (mr.orig_index_vert && mr.orig_index_vert[vert] == ORIGINDEX_NONE) {
      return false;
    }
    return true;
  };

  MutableSpan loose_edge_data = data.slice(visible_corners.size(), loose_edges.size() * 2);
  const int loose_geom_start = subdiv_cache.num_subdiv_loops;
  for (const int i : loose_edges.index_range()) {
    const int2 edge = coarse_edges[loose_edges[i]];
    const IndexRange edge_range(loose_geom_start + i * verts_per_edge, verts_per_edge);
    loose_edge_data[i * 2 + 0] = show_vert(edge[0]) ? edge_range.first() : gpu::RESTART_INDEX;
    loose_edge_data[i * 2 + 1] = show_vert(edge[1]) ? edge_range.last() : gpu::RESTART_INDEX;
  }

  MutableSpan loose_vert_data = data.take_back(visible_loose.size()).cast<int32_t>();
  const int loose_verts_start = loose_geom_start + loose_edge_verts_num;
  visible_loose.shift(loose_verts_start, memory).to_indices<int32_t>(loose_vert_data);

  GPU_indexbuf_build_in_place_ex(&builder, 0, max_index, true, &points);
}

static void extract_points_subdiv_bm(const MeshRenderData &mr,
                                     const DRWSubdivCache &subdiv_cache,
                                     gpu::IndexBuf &points)
{
  const Span<int2> coarse_edges = mr.edges;
  const Span<int> loose_verts = mr.loose_verts;
  const Span<int> loose_edges = mr.loose_edges;
  const int verts_per_edge = subdiv_verts_per_coarse_edge(subdiv_cache);
  const int loose_edge_verts_num = verts_per_edge * loose_edges.size();

  const Span<int> corner_orig_verts{
      static_cast<int *>(GPU_vertbuf_get_data(subdiv_cache.verts_orig_index)),
      subdiv_cache.num_subdiv_loops};

  const auto show_vert_bm = [&](const int vert_index) {
    const BMVert *vert = mr.orig_index_vert ? bm_original_vert_get(mr, vert_index) :
                                              BM_vert_at_index(mr.bm, vert_index);
    return !BM_elem_flag_test_bool(vert, BM_ELEM_HIDDEN);
  };

  IndexMaskMemory memory;
  const IndexMask visible_corners = IndexMask::from_predicate(
      corner_orig_verts.index_range(), GrainSize(4096), memory, [&](const int i) {
        return corner_orig_verts[i] != -1 && show_vert_bm(corner_orig_verts[i]);
      });

  const IndexMask visible_loose = IndexMask::from_predicate(
      loose_verts.index_range(), GrainSize(4096), memory, [&](const int vert) {
        return show_vert_bm(vert);
      });

  const int max_index = subdiv_cache.num_subdiv_loops + loose_edge_verts_num + loose_verts.size();
  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder,
                    GPU_PRIM_POINTS,
                    visible_corners.size() + loose_edges.size() * 2 + visible_loose.size(),
                    max_index);
  MutableSpan<uint> data = GPU_indexbuf_get_data(&builder);

  visible_corners.to_indices<int32_t>(data.take_front(visible_corners.size()).cast<int32_t>());

  MutableSpan loose_edge_data = data.slice(visible_corners.size(), loose_edges.size() * 2);
  const int loose_geom_start = subdiv_cache.num_subdiv_loops;
  for (const int i : loose_edges.index_range()) {
    const int2 edge = coarse_edges[loose_edges[i]];
    const IndexRange edge_range(loose_geom_start + i * verts_per_edge, verts_per_edge);
    loose_edge_data[i * 2 + 0] = show_vert_bm(edge[0]) ? edge_range.first() : gpu::RESTART_INDEX;
    loose_edge_data[i * 2 + 1] = show_vert_bm(edge[1]) ? edge_range.last() : gpu::RESTART_INDEX;
  }

  MutableSpan loose_vert_data = data.take_back(visible_loose.size()).cast<int32_t>();
  const int loose_verts_start = loose_geom_start + loose_edge_verts_num;
  visible_loose.shift(loose_verts_start, memory).to_indices<int32_t>(loose_vert_data);

  GPU_indexbuf_build_in_place_ex(&builder, 0, max_index, true, &points);
}

void extract_points_subdiv(const MeshRenderData &mr,
                           const DRWSubdivCache &subdiv_cache,
                           gpu::IndexBuf &points)
{
  if (mr.extract_type == MR_EXTRACT_MESH) {
    extract_points_subdiv_mesh(mr, subdiv_cache, points);
  }
  else {
    extract_points_subdiv_bm(mr, subdiv_cache, points);
  }
}

}  // namespace blender::draw
