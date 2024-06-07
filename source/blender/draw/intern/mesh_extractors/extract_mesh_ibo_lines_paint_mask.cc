/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BLI_bitmap.h"
#include "atomic_ops.h"

#include "MEM_guardedalloc.h"

#include "GPU_index_buffer.hh"

#include "draw_subdivision.hh"
#include "extract_mesh.hh"

namespace blender::draw {

void extract_lines_paint_mask(const MeshRenderData &mr, gpu::IndexBuf &lines)
{
  const OffsetIndices faces = mr.faces;
  const Span<int> corner_edges = mr.corner_edges;
  const Span<bool> hide_edge = mr.hide_edge;
  const Span<bool> select_poly = mr.select_poly;
  const Span<int> orig_index_edge = mr.orig_index_edge ?
                                        Span<int>(mr.orig_index_edge, mr.edges_num) :
                                        Span<int>();

  GPUIndexBufBuilder builder;
  const int max_index = mr.corners_num;
  GPU_indexbuf_init(&builder, GPU_PRIM_LINES, mr.edges_num, max_index);
  MutableSpan<uint2> data = GPU_indexbuf_get_data(&builder).cast<uint2>();

  BLI_bitmap *select_map = BLI_BITMAP_NEW(mr.edges_num, __func__);
  threading::parallel_for(faces.index_range(), 1024, [&](const IndexRange range) {
    for (const int face_index : range) {
      const IndexRange face = faces[face_index];
      for (const int corner : face) {
        const int edge = corner_edges[corner];
        if ((!hide_edge.is_empty() && hide_edge[edge]) ||
            (!orig_index_edge.is_empty() && (orig_index_edge[edge] == ORIGINDEX_NONE)))
        {
          data[edge] = uint2(gpu::RESTART_INDEX);
          continue;
        }

        const int corner_next = bke::mesh::face_corner_next(face, corner);
        if (!select_poly.is_empty() && select_poly[face_index]) {
          if (BLI_BITMAP_TEST_AND_SET_ATOMIC(select_map, edge)) {
            /* Hide edge as it has more than 2 selected loop. */
            data[edge] = uint2(gpu::RESTART_INDEX);
          }
          else {
            /* First selected loop. Set edge visible, overwriting any unselected loop. */
            data[edge] = uint2(corner, corner_next);
          }
        }
        else {
          /* Set these unselected loop only if this edge has no other selected loop. */
          if (!BLI_BITMAP_TEST(select_map, edge)) {
            data[edge] = uint2(corner, corner_next);
          }
        }
      }
    }
  });

  GPU_indexbuf_build_in_place_ex(&builder, 0, max_index, true, &lines);

  MEM_freeN(select_map);
}

void extract_lines_paint_mask_subdiv(const MeshRenderData &mr,
                                     const DRWSubdivCache &subdiv_cache,
                                     gpu::IndexBuf &lines)
{
  const Span<bool> hide_edge = mr.hide_edge;
  const Span<bool> select_poly = mr.select_poly;
  const Span<int> orig_index_edge = mr.orig_index_edge ?
                                        Span<int>(mr.orig_index_edge, mr.edges_num) :
                                        Span<int>();
  const Span<int> subdiv_loop_face_index(subdiv_cache.subdiv_loop_face_index,
                                         subdiv_cache.num_subdiv_loops);
  const Span<int> subdiv_loop_subdiv_edge_index(subdiv_cache.subdiv_loop_subdiv_edge_index,
                                                subdiv_cache.num_subdiv_loops);
  const Span<int> subdiv_loop_edge_index(
      static_cast<const int *>(GPU_vertbuf_get_data(*subdiv_cache.edges_orig_index)),
      subdiv_cache.num_subdiv_edges);

  GPUIndexBufBuilder builder;
  const int max_index = subdiv_cache.num_subdiv_loops;
  GPU_indexbuf_init(&builder, GPU_PRIM_LINES, subdiv_cache.num_subdiv_edges, max_index);
  MutableSpan<uint2> data = GPU_indexbuf_get_data(&builder).cast<uint2>();

  BLI_bitmap *select_map = BLI_BITMAP_NEW(mr.edges_num, __func__);
  const int quads_num = subdiv_cache.num_subdiv_quads;
  threading::parallel_for(IndexRange(quads_num), 4096, [&](const IndexRange range) {
    for (const int subdiv_quad_index : range) {
      const int coarse_quad_index = subdiv_loop_face_index[subdiv_quad_index * 4];
      const IndexRange subdiv_face(subdiv_quad_index * 4, 4);
      for (const int corner : subdiv_face) {
        const uint coarse_edge_index = uint(subdiv_loop_edge_index[corner]);
        const uint subdiv_edge_index = uint(subdiv_loop_subdiv_edge_index[corner]);
        if (coarse_edge_index == -1u) {
          data[subdiv_edge_index] = uint2(gpu::RESTART_INDEX);
          continue;
        }
        if ((!hide_edge.is_empty() && hide_edge[coarse_edge_index]) ||
            (!orig_index_edge.is_empty() &&
             (orig_index_edge[coarse_edge_index] == ORIGINDEX_NONE)))
        {
          data[subdiv_edge_index] = uint2(gpu::RESTART_INDEX);
          continue;
        }

        const int corner_next = bke::mesh::face_corner_next(subdiv_face, corner);
        if (!select_poly.is_empty() && select_poly[coarse_quad_index]) {
          if (BLI_BITMAP_TEST_AND_SET_ATOMIC(select_map, coarse_edge_index)) {
            /* Hide edge as it has more than 2 selected loop. */
            data[subdiv_edge_index] = uint2(gpu::RESTART_INDEX);
          }
          else {
            /* First selected loop. Set edge visible, overwriting any unselected loop. */
            data[subdiv_edge_index] = uint2(corner, corner_next);
          }
        }
        else {
          /* Set these unselected loop only if this edge has no other selected loop. */
          if (!BLI_BITMAP_TEST(select_map, coarse_edge_index)) {
            data[subdiv_edge_index] = uint2(corner, corner_next);
          }
        }
      }
    }
  });

  GPU_indexbuf_build_in_place_ex(&builder, 0, max_index, true, &lines);

  MEM_freeN(select_map);
}

}  // namespace blender::draw
