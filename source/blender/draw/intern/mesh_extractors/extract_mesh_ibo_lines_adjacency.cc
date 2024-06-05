/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BLI_map.hh"
#include "BLI_ordered_edge.hh"
#include "BLI_vector.hh"

#include "BKE_editmesh.hh"

#include "GPU_index_buffer.hh"

#include "draw_subdivision.hh"
#include "extract_mesh.hh"

namespace blender::draw {

#define NO_EDGE INT_MAX

static void create_lines_for_remaining_edges(MutableSpan<int> vert_to_corner,
                                             Map<OrderedEdge, int> &edge_hash,
                                             GPUIndexBufBuilder &elb,
                                             bool &is_manifold)
{
  for (const auto item : edge_hash.items()) {
    int v_data = item.value;
    if (v_data == NO_EDGE) {
      continue;
    }

    int v2 = item.key.v_low;
    int v3 = item.key.v_high;

    int l1 = uint(abs(v_data)) - 1;
    if (v_data < 0) { /* `inv_opposite`. */
      std::swap(v2, v3);
    }
    int l2 = vert_to_corner[v2];
    int l3 = vert_to_corner[v3];
    GPU_indexbuf_add_line_adj_verts(&elb, l1, l2, l3, l1);
    is_manifold = false;
  }
}

inline void rotate_vector(uint3 &value)
{
  const uint tmp = value[0];
  value[0] = value[2];
  value[2] = value[1];
  value[1] = tmp;
}

/** \param vert_to_corner: Array to convert vert index to any corner of this vert. */
inline void lines_adjacency_triangle(uint3 vert_tri,
                                     uint3 corner_tri,
                                     MutableSpan<int> vert_to_corner,
                                     Map<OrderedEdge, int> &edge_hash,
                                     GPUIndexBufBuilder &elb,
                                     bool &is_manifold)
{
  /* Iterate around the triangle's edges. */
  for (int e = 0; e < 3; e++) {
    rotate_vector(vert_tri);
    rotate_vector(corner_tri);

    bool inv_indices = (vert_tri[1] > vert_tri[2]);
    edge_hash.add_or_modify(
        {vert_tri[1], vert_tri[2]},
        [&](int *value) {
          int new_value = int(corner_tri[0]) + 1; /* 0 cannot be signed so add one. */
          *value = inv_indices ? -new_value : new_value;
          /* Store loop indices for remaining non-manifold edges. */
          vert_to_corner[vert_tri[1]] = corner_tri[1];
          vert_to_corner[vert_tri[2]] = corner_tri[2];
        },
        [&](int *value) {
          int v_data = *value;
          if (v_data == NO_EDGE) {
            int new_value = int(corner_tri[0]) + 1; /* 0 cannot be signed so add one. */
            *value = inv_indices ? -new_value : new_value;
            /* Store loop indices for remaining non-manifold edges. */
            vert_to_corner[vert_tri[1]] = corner_tri[1];
            vert_to_corner[vert_tri[2]] = corner_tri[2];
          }
          else {
            /* HACK Tag as not used. Prevent overhead of BLI_edgehash_remove. */
            *value = NO_EDGE;
            bool inv_opposite = (v_data < 0);
            const int corner_opposite = abs(v_data) - 1;
            /* TODO: Make this part thread-safe. */
            if (inv_opposite == inv_indices) {
              /* Don't share edge if triangles have non matching winding. */
              GPU_indexbuf_add_line_adj_verts(
                  &elb, corner_tri[0], corner_tri[1], corner_tri[2], corner_tri[0]);
              GPU_indexbuf_add_line_adj_verts(
                  &elb, corner_opposite, corner_tri[1], corner_tri[2], corner_opposite);
              is_manifold = false;
            }
            else {
              GPU_indexbuf_add_line_adj_verts(
                  &elb, corner_tri[0], corner_tri[1], corner_tri[2], corner_opposite);
            }
          }
        });
  }
}

static void calc_adjacency_bm(const MeshRenderData &mr,
                              MutableSpan<int> vert_to_corner,
                              Map<OrderedEdge, int> &edge_hash,
                              GPUIndexBufBuilder &elb,
                              bool &is_manifold)
{
  const Span<std::array<BMLoop *, 3>> looptris = mr.edit_bmesh->looptris;
  for (const int i : looptris.index_range()) {
    const std::array<BMLoop *, 3> &tri = looptris[i];
    if (BM_elem_flag_test(tri[0]->f, BM_ELEM_HIDDEN)) {
      continue;
    }
    lines_adjacency_triangle(
        uint3(BM_elem_index_get(tri[0]->v),
              BM_elem_index_get(tri[1]->v),
              BM_elem_index_get(tri[2]->v)),
        uint3(BM_elem_index_get(tri[0]), BM_elem_index_get(tri[1]), BM_elem_index_get(tri[2])),
        vert_to_corner,
        edge_hash,
        elb,
        is_manifold);
  }
}

static void calc_adjacency_mesh(const MeshRenderData &mr,
                                MutableSpan<int> vert_to_corner,
                                Map<OrderedEdge, int> &edge_hash,
                                GPUIndexBufBuilder &elb,
                                bool &is_manifold)
{
  const OffsetIndices faces = mr.faces;
  const Span<int> corner_verts = mr.corner_verts;
  const Span<bool> hide_poly = mr.hide_poly;
  const Span<int3> corner_tris = mr.mesh->corner_tris();
  for (const int face : faces.index_range()) {
    if (!hide_poly.is_empty() && hide_poly[face]) {
      continue;
    }
    const IndexRange tris = bke::mesh::face_triangles_range(faces, face);
    for (const int3 &tri : corner_tris.slice(tris)) {
      lines_adjacency_triangle(
          uint3(corner_verts[tri[0]], corner_verts[tri[1]], corner_verts[tri[2]]),
          uint3(tri),
          vert_to_corner,
          edge_hash,
          elb,
          is_manifold);
    }
  }
}

void extract_lines_adjacency(const MeshRenderData &mr, gpu::IndexBuf &ibo, bool &r_is_manifold)
{
  /* Similar to poly_to_tri_count().
   * There is always (loop + triangle - 1) edges inside a face.
   * Accumulate for all faces and you get : */
  const int tess_edge_len = mr.corners_num + mr.corner_tris_num - mr.faces_num;
  Map<OrderedEdge, int> edge_hash;
  edge_hash.reserve(tess_edge_len);
  Array<int> vert_to_corner(mr.verts_num, 0);
  bool is_manifold = true;

  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder, GPU_PRIM_LINES_ADJ, tess_edge_len, mr.corners_num);

  if (mr.extract_type == MR_EXTRACT_MESH) {
    calc_adjacency_mesh(mr, vert_to_corner, edge_hash, builder, is_manifold);
  }
  else {
    calc_adjacency_bm(mr, vert_to_corner, edge_hash, builder, is_manifold);
  }

  create_lines_for_remaining_edges(vert_to_corner, edge_hash, builder, is_manifold);

  r_is_manifold = is_manifold;

  GPU_indexbuf_build_in_place(&builder, &ibo);
}

void extract_lines_adjacency_subdiv(const DRWSubdivCache &subdiv_cache,
                                    gpu::IndexBuf &ibo,
                                    bool &r_is_manifold)
{
  /* For each face there is (loop + triangle - 1) edges. Since we only have quads, and a quad
   * is split into 2 triangles, we have (loop + 2 - 1) = (loop + 1) edges for each quad, or in
   * total: (number_of_loops + number_of_quads). */
  const uint tess_edge_len = subdiv_cache.num_subdiv_loops + subdiv_cache.num_subdiv_quads;

  Array<int> vert_to_corner(subdiv_cache.num_subdiv_verts, 0);
  Map<OrderedEdge, int> edge_hash;
  edge_hash.reserve(tess_edge_len);

  bool is_manifold = true;

  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder, GPU_PRIM_LINES_ADJ, tess_edge_len, subdiv_cache.num_subdiv_loops);

  for (const int subdiv_quad_index : IndexRange(subdiv_cache.num_subdiv_quads)) {
    const uint loop_index = subdiv_quad_index * 4;
    const uint corner_0 = loop_index + 0;
    const uint corner_1 = loop_index + 1;
    const uint corner_2 = loop_index + 2;
    const uint corner_3 = loop_index + 3;

    const uint vert_0 = subdiv_cache.subdiv_loop_subdiv_vert_index[corner_0];
    const uint vert_1 = subdiv_cache.subdiv_loop_subdiv_vert_index[corner_1];
    const uint vert_2 = subdiv_cache.subdiv_loop_subdiv_vert_index[corner_2];
    const uint vert_3 = subdiv_cache.subdiv_loop_subdiv_vert_index[corner_3];

    lines_adjacency_triangle({vert_0, vert_1, vert_2},
                             {corner_0, corner_1, corner_2},
                             vert_to_corner,
                             edge_hash,
                             builder,
                             is_manifold);
    lines_adjacency_triangle({vert_0, vert_2, vert_3},
                             {corner_0, corner_2, corner_3},
                             vert_to_corner,
                             edge_hash,
                             builder,
                             is_manifold);
  }

  create_lines_for_remaining_edges(vert_to_corner, edge_hash, builder, is_manifold);

  r_is_manifold = is_manifold;

  GPU_indexbuf_build_in_place(&builder, &ibo);
}

#undef NO_EDGE

}  // namespace blender::draw
