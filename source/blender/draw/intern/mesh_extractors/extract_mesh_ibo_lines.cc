/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BLI_array_utils.hh"

#include "GPU_index_buffer.hh"

#include "extract_mesh.hh"

#include "draw_cache_inline.hh"
#include "draw_subdivision.hh"

namespace blender::draw {

static IndexMask calc_mesh_edge_visibility(const MeshRenderData &mr,
                                           const IndexMask &mask,
                                           IndexMaskMemory &memory)
{
  IndexMask visible = mask;
  if (!mr.mesh->runtime->subsurf_optimal_display_edges.is_empty()) {
    const BoundedBitSpan visible_bits = mr.mesh->runtime->subsurf_optimal_display_edges;
    visible = IndexMask::from_bits(visible, visible_bits, memory);
  }
  if (!mr.hide_edge.is_empty()) {
    visible = IndexMask::from_bools_inverse(visible, mr.hide_edge, memory);
  }
  if (mr.hide_unmapped_edges && mr.e_origindex != nullptr) {
    const int *orig_index = mr.e_origindex;
    visible = IndexMask::from_predicate(visible, GrainSize(4096), memory, [&](const int64_t i) {
      return orig_index[i] != ORIGINDEX_NONE;
    });
  }
  return visible;
}

/* In the GPU vertex buffers, the value for each vertex is duplicated to each of its vertex
 * corners. So the edges on the GPU connect face corners rather than vertices. */
static uint2 edge_from_corners(const IndexRange face, const int corner)
{
  const int corner_next = bke::mesh::face_corner_next(face, corner);
  return uint2(corner, corner_next);
}

static void fill_loose_lines_ibo(const uint corners_num, MutableSpan<uint2> data)
{
  /* Vertices for loose edges are not shared in the GPU vertex buffers, so the indices are simply
   * an increasing contiguous range. Ideally this would be generated on the GPU itself, or just
   * unnecessary, but a large number of loose edges isn't expected to be a common performance
   * bottleneck either. */
  threading::memory_bandwidth_bound_task(data.size_in_bytes(), [&]() {
    array_utils::fill_index_range(data.cast<uint>(), corners_num);
  });
}

static void extract_lines_mesh(const MeshRenderData &mr,
                               gpu::IndexBuf *lines,
                               gpu::IndexBuf *lines_loose,
                               bool &no_loose_wire)
{
  IndexMaskMemory memory;
  const IndexMask all_loose_edges = IndexMask::from_indices(mr.loose_edges, memory);
  const IndexMask visible_loose_edges = calc_mesh_edge_visibility(mr, all_loose_edges, memory);
  const int max_index = mr.corners_num + visible_loose_edges.size() * 2;

  no_loose_wire = visible_loose_edges.is_empty();

  if (DRW_ibo_requested(lines_loose) && !DRW_ibo_requested(lines)) {
    GPUIndexBufBuilder builder;
    GPU_indexbuf_init(&builder, GPU_PRIM_LINES, visible_loose_edges.size(), max_index);
    MutableSpan<uint2> data = GPU_indexbuf_get_data(&builder).cast<uint2>();
    fill_loose_lines_ibo(mr.corners_num, data);
    GPU_indexbuf_build_in_place_ex(&builder, 0, max_index, false, lines_loose);
    return;
  }

  const IndexMask non_loose_edges = all_loose_edges.complement(mr.edges.index_range(), memory);
  const IndexMask visible_non_loose_edges = calc_mesh_edge_visibility(mr, non_loose_edges, memory);

  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder,
                    GPU_PRIM_LINES,
                    visible_non_loose_edges.size() + visible_loose_edges.size(),
                    max_index);
  MutableSpan<uint2> data = GPU_indexbuf_get_data(&builder).cast<uint2>();

  /* This code fills the index buffer in a non-deterministic way, with non-atomic access to `used`.
   * This is okay because any of the possible face corner indices are correct, since they all
   * correspond to the same #Mesh vertex. `used` exists here as a performance optimization to
   * avoid writing to the VBO. */
  const OffsetIndices faces = mr.faces;
  const Span<int> corner_edges = mr.corner_edges;
  if (visible_non_loose_edges.size() == mr.edges_num) {
    /* All edges in the mesh are visible. The edges in the GPU buffer will have the same order as
     * the mesh's edges, so any remapping is unnecessary. Use a boolean array to avoid writing to
     * the same indices again multiple times from different threads. This is slightly beneficial
     * because booleans are 8 times smaller than the `uint2` for each edge. */
    Array<bool> used(mr.edges_num, false);
    threading::memory_bandwidth_bound_task(
        used.as_span().size_in_bytes() + data.size_in_bytes() + corner_edges.size_in_bytes(),
        [&]() {
          threading::parallel_for(faces.index_range(), 2048, [&](const IndexRange range) {
            for (const int face_index : range) {
              const IndexRange face = faces[face_index];
              for (const int corner : face) {
                const int edge = corner_edges[corner];
                if (!used[edge]) {
                  data[edge] = edge_from_corners(face, corner);
                  used[edge] = true;
                }
              }
            }
          });
        });
  }
  else {
    Array<int> map(mr.corners_num, -1);
    threading::memory_bandwidth_bound_task(
        map.as_span().size_in_bytes() + data.size_in_bytes() + corner_edges.size_in_bytes(),
        [&]() {
          index_mask::build_reverse_map(visible_non_loose_edges, map.as_mutable_span());
          threading::parallel_for(faces.index_range(), 2048, [&](const IndexRange range) {
            for (const int face_index : range) {
              const IndexRange face = faces[face_index];
              for (const int corner : face) {
                const int edge = corner_edges[corner];
                if (map[edge] != -1) {
                  data[map[edge]] = edge_from_corners(face, corner);
                  map[edge] = -1;
                }
              }
            }
          });
        });
  }

  fill_loose_lines_ibo(mr.corners_num, data.take_back(visible_loose_edges.size()));

  GPU_indexbuf_build_in_place_ex(&builder, 0, max_index, false, lines);
  if (DRW_ibo_requested(lines_loose)) {
    GPU_indexbuf_create_subrange_in_place(
        lines_loose, lines, visible_non_loose_edges.size() * 2, visible_loose_edges.size() * 2);
  }
}

static IndexMask calc_bm_edge_visibility(const BMesh &bm,
                                         const IndexMask &mask,
                                         IndexMaskMemory &memory)
{
  return IndexMask::from_predicate(mask, GrainSize(2048), memory, [&](const int i) {
    return !BM_elem_flag_test_bool(BM_edge_at_index(&const_cast<BMesh &>(bm), i), BM_ELEM_HIDDEN);
  });
}

static void extract_lines_bm(const MeshRenderData &mr,
                             gpu::IndexBuf *lines,
                             gpu::IndexBuf *lines_loose,
                             bool &no_loose_wire)
{
  const BMesh &bm = *mr.bm;

  IndexMaskMemory memory;
  const IndexMask all_loose_edges = IndexMask::from_indices(mr.loose_edges, memory);
  const IndexMask visible_loose_edges = calc_bm_edge_visibility(bm, all_loose_edges, memory);
  const int max_index = mr.corners_num + visible_loose_edges.size() * 2;

  no_loose_wire = visible_loose_edges.is_empty();

  const IndexMask non_loose_edges = all_loose_edges.complement(IndexRange(bm.totedge), memory);
  const IndexMask visible_non_loose_edges = calc_bm_edge_visibility(bm, non_loose_edges, memory);

  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder,
                    GPU_PRIM_LINES,
                    visible_non_loose_edges.size() + visible_loose_edges.size(),
                    max_index);
  MutableSpan<uint2> data = GPU_indexbuf_get_data(&builder).cast<uint2>();

  /* Make use of BMesh's edge to loop topology knowledge to iterate over edges instead of
   * iterating over faces and defining edges implicitly as done in the #Mesh extraction. */
  visible_non_loose_edges.foreach_index(GrainSize(4096), [&](const int i, const int pos) {
    const BMEdge &edge = *BM_edge_at_index(&const_cast<BMesh &>(bm), i);
    data[pos] = uint2(BM_elem_index_get(edge.l), BM_elem_index_get(edge.l->next));
  });

  fill_loose_lines_ibo(mr.corners_num, data.take_back(visible_loose_edges.size()));

  GPU_indexbuf_build_in_place_ex(&builder, 0, max_index, false, lines);
  if (DRW_ibo_requested(lines_loose)) {
    GPU_indexbuf_create_subrange_in_place(
        lines_loose, lines, visible_non_loose_edges.size() * 2, visible_loose_edges.size() * 2);
  }
}

void extract_lines(const MeshRenderData &mr,
                   gpu::IndexBuf *lines,
                   gpu::IndexBuf *lines_loose,
                   bool &no_loose_wire)
{
  if (mr.extract_type == MR_EXTRACT_MESH) {
    extract_lines_mesh(mr, lines, lines_loose, no_loose_wire);
  }
  else {
    extract_lines_bm(mr, lines, lines_loose, no_loose_wire);
  }
}

static void extract_lines_loose_geom_subdiv(const DRWSubdivCache &subdiv_cache,
                                            const MeshRenderData &mr,
                                            const int edge_loose_offset,
                                            gpu::IndexBuf *ibo)
{
  const DRWSubdivLooseGeom &loose_geom = subdiv_cache.loose_geom;
  if (loose_geom.edge_len == 0) {
    return;
  }

  /* Update flags for loose edges, points are already handled. */
  static GPUVertFormat format;
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "data", GPU_COMP_U32, 1, GPU_FETCH_INT);
  }

  gpu::VertBuf *flags = GPU_vertbuf_calloc();
  GPU_vertbuf_init_with_format(flags, &format);

  Span<DRWSubdivLooseEdge> loose_edges = draw_subdiv_cache_get_loose_edges(subdiv_cache);
  GPU_vertbuf_data_alloc(flags, loose_edges.size());

  uint *flags_data = static_cast<uint *>(GPU_vertbuf_get_data(flags));

  switch (mr.extract_type) {
    case MR_EXTRACT_MESH: {
      const int *e_origindex = (mr.hide_unmapped_edges) ? mr.e_origindex : nullptr;
      if (e_origindex == nullptr) {
        const Span<bool> hide_edge = mr.hide_edge;
        if (!hide_edge.is_empty()) {
          for (DRWSubdivLooseEdge edge : loose_edges) {
            *flags_data++ = hide_edge[edge.coarse_edge_index];
          }
        }
        else {
          MutableSpan<uint>(flags_data, loose_edges.size()).fill(0);
        }
      }
      else {
        if (mr.bm) {
          for (DRWSubdivLooseEdge edge : loose_edges) {
            const BMEdge *bm_edge = bm_original_edge_get(mr, edge.coarse_edge_index);
            *flags_data++ = (bm_edge) ? BM_elem_flag_test_bool(bm_edge, BM_ELEM_HIDDEN) != 0 : 1;
          }
        }
        else {
          const Span<bool> hide_edge = mr.hide_edge;
          if (!hide_edge.is_empty()) {
            for (DRWSubdivLooseEdge edge : loose_edges) {
              int e = edge.coarse_edge_index;

              if (e_origindex && e_origindex[e] != ORIGINDEX_NONE) {
                *flags_data++ = hide_edge[edge.coarse_edge_index];
              }
              else {
                *flags_data++ = false;
              }
            }
          }
          else {
            MutableSpan<uint>(flags_data, loose_edges.size()).fill(0);
          }
        }
      }
      break;
    }
    case MR_EXTRACT_BMESH: {
      BMesh *bm = mr.bm;
      for (DRWSubdivLooseEdge edge : loose_edges) {
        const BMEdge *bm_edge = BM_edge_at_index(bm, edge.coarse_edge_index);
        *flags_data++ = BM_elem_flag_test_bool(bm_edge, BM_ELEM_HIDDEN) != 0;
      }
      break;
    }
  }

  draw_subdiv_build_lines_loose_buffer(
      subdiv_cache, ibo, flags, uint(edge_loose_offset), uint(loose_geom.edge_len));

  GPU_vertbuf_discard(flags);
}

void extract_lines_subdiv(const DRWSubdivCache &subdiv_cache,
                          const MeshRenderData &mr,
                          gpu::IndexBuf *lines,
                          gpu::IndexBuf *lines_loose,
                          bool &no_loose_wire)
{
  const DRWSubdivLooseGeom &loose_geom = subdiv_cache.loose_geom;
  const int loose_num = loose_geom.edge_len * 2;
  no_loose_wire = loose_num == 0;
  if (subdiv_cache.num_subdiv_loops == 0) {
    return;
  }

  if (DRW_ibo_requested(lines_loose) && !DRW_ibo_requested(lines)) {
    GPU_indexbuf_init_build_on_device(lines_loose, loose_num);
    extract_lines_loose_geom_subdiv(subdiv_cache, mr, 0, lines_loose);
    return;
  }

  const int non_loose_num = subdiv_cache.num_subdiv_loops * 2;

  GPU_indexbuf_init_build_on_device(lines, non_loose_num + loose_num);
  draw_subdiv_build_lines_buffer(subdiv_cache, lines);
  extract_lines_loose_geom_subdiv(subdiv_cache, mr, non_loose_num, lines);

  if (DRW_ibo_requested(lines_loose)) {
    /* Multiply by 2 because these are edges indices. */
    GPU_indexbuf_create_subrange_in_place(lines_loose, lines, non_loose_num, loose_num);
  }
}

}  // namespace blender::draw
