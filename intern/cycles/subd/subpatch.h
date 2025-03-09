/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/map.h"
#include "util/math_float2.h"

CCL_NAMESPACE_BEGIN

class Patch;

/* SubEdge */

struct SubEdge {
  /* Number of segments the edge will be diced into, see DiagSplit paper. */
  int T = 0;

  /* top is edge adjacent to start, bottom is adjacent to end. */
  SubEdge *top = nullptr, *bottom = nullptr;

  int top_offset = -1, bottom_offset = -1;
  bool top_indices_decrease = false, bottom_indices_decrease = false;

  int start_vert_index = -1;
  int end_vert_index = -1;

  /* Index of the second vert from this edges corner along the edge towards the next corner. */
  int second_vert_index = -1;

  /* Vertices on edge are to be stitched. */
  bool is_stitch_edge = false;

  /* Key to match this edge with others to be stitched with.
   * The ints in the pair are ordered stitching indices */
  pair<int, int> stitch_edge_key;

  /* Full T along edge (may be larger than T for edges split from ngon edges) */
  int stitch_edge_T = 0;
  int stitch_offset = 0;
  int stitch_top_offset;
  int stitch_start_vert_index;
  int stitch_end_vert_index;

  SubEdge() = default;

  int get_vert_along_edge(const int n) const
  {
    assert(n >= 0 && n <= T);

    if (n == 0) {
      return start_vert_index;
    }
    if (n == T) {
      return end_vert_index;
    }

    return second_vert_index + n - 1;
  }
};

/* SubPatch */

class SubPatch {
 public:
  const Patch *patch; /* Patch this is a subpatch of. */
  int inner_grid_vert_offset;

  struct Edge {
    int T;
    int offset; /* Offset along main edge, interpretation depends on the two flags below. */

    bool indices_decrease_along_edge;
    bool sub_edges_created_in_reverse_order;

    struct SubEdge *edge;

    int get_vert_along_edge(const int n_relative) const
    {
      assert(n_relative >= 0 && n_relative <= T);

      int n = n_relative;

      if (!indices_decrease_along_edge && !sub_edges_created_in_reverse_order) {
        n = offset + n;
      }
      else if (!indices_decrease_along_edge && sub_edges_created_in_reverse_order) {
        n = edge->T - offset - T + n;
      }
      else if (indices_decrease_along_edge && !sub_edges_created_in_reverse_order) {
        n = offset + T - n;
      }
      else if (indices_decrease_along_edge && sub_edges_created_in_reverse_order) {
        n = edge->T - offset - n;
      }

      return edge->get_vert_along_edge(n);
    }
  };

  /*
   *            eu1
   *     uv01 --------- uv11
   *     |                 |
   * ev0 |                 | ev1
   *     |                 |
   *     uv00 --------- uv10
   *            eu0
   */

  /* UV within patch, counter-clockwise starting from uv (0, 0) towards (1, 0) etc. */
  float2 uv00 = zero_float2();
  float2 uv10 = make_float2(1.0f, 0.0f);
  float2 uv11 = one_float2();
  float2 uv01 = make_float2(0.0f, 1.0f);

  union {
    Edge edges[4]; /* Edges of this subpatch, each edge starts at the corner of the same index. */
    struct {
      Edge edge_v0, edge_u1, edge_v1, edge_u0;
    };
  };

  explicit SubPatch(const Patch *patch = nullptr) : patch(patch) {}

  int calc_num_inner_verts() const
  {
    int Mu = fmax(edge_u0.T, edge_u1.T);
    int Mv = fmax(edge_v0.T, edge_v1.T);
    Mu = fmax(Mu, 2);
    Mv = fmax(Mv, 2);
    return (Mu - 1) * (Mv - 1);
  }

  int calc_num_triangles() const
  {
    int Mu = fmax(edge_u0.T, edge_u1.T);
    int Mv = fmax(edge_v0.T, edge_v1.T);
    Mu = fmax(Mu, 2);
    Mv = fmax(Mv, 2);

    const int inner_triangles = (Mu - 2) * (Mv - 2) * 2;
    const int edge_triangles = edge_u0.T + edge_u1.T + edge_v0.T + edge_v1.T + (Mu - 2) * 2 +
                               (Mv - 2) * 2;

    return inner_triangles + edge_triangles;
  }

  int get_vert_along_grid_edge(const int edge, const int n) const
  {
    int Mu = fmax(edge_u0.T, edge_u1.T);
    int Mv = fmax(edge_v0.T, edge_v1.T);
    Mu = fmax(Mu, 2);
    Mv = fmax(Mv, 2);

    switch (edge) {
      case 0:
        return inner_grid_vert_offset + n * (Mu - 1);
      case 1:
        return inner_grid_vert_offset + (Mu - 1) * (Mv - 2) + n;
      case 2:
        return inner_grid_vert_offset + ((Mu - 1) * (Mv - 1) - 1) - n * (Mu - 1);
      case 3:
        return inner_grid_vert_offset + (Mu - 2) - n;
      default:
        assert(0);
        break;
    }

    return -1;
  }

  int get_vert_along_edge(const int edge, const int n) const
  {
    return edges[edge].get_vert_along_edge(n);
  }

  float2 map_uv(float2 uv)
  {
    /* Map UV from subpatch to patch parametric coordinates. */
    const float2 d0 = interp(uv00, uv01, uv.y);
    const float2 d1 = interp(uv10, uv11, uv.y);
    return clamp(interp(d0, d1, uv.x), zero_float2(), one_float2());
  }
};

CCL_NAMESPACE_END
