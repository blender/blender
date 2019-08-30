/*
 * Copyright 2011-2018 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __SUBD_SUBPATCH_H__
#define __SUBD_SUBPATCH_H__

#include "util/util_map.h"
#include "util/util_types.h"

CCL_NAMESPACE_BEGIN

/* Subpatch */

class Subpatch {
 public:
  class Patch *patch; /* Patch this is a subpatch of. */
  int inner_grid_vert_offset;

  struct edge_t {
    int T;
    int offset; /* Offset along main edge, interpretation depends on the two flags below. */

    bool indices_decrease_along_edge;
    bool sub_edges_created_in_reverse_order;

    struct Edge *edge;

    int get_vert_along_edge(int n) const;
  };

  /*
   *            eu1
   *     c01 --------- c11
   *     |               |
   * ev0 |               | ev1
   *     |               |
   *     c00 --------- c10
   *            eu0
   */

  union {
    float2 corners[4]; /* UV within patch, clockwise starting from uv (0, 0) towards (0, 1) etc. */
    struct {
      float2 c00, c01, c11, c10;
    };
  };

  union {
    edge_t
        edges[4]; /* Edges of this subpatch, each edge starts at the corner of the same index. */
    struct {
      edge_t edge_v0, edge_u1, edge_v1, edge_u0;
    };
  };

  explicit Subpatch(Patch *patch = nullptr)
      : patch(patch),
        c00(make_float2(0.0f, 0.0f)),
        c01(make_float2(0.0f, 1.0f)),
        c11(make_float2(1.0f, 1.0f)),
        c10(make_float2(1.0f, 0.0f))
  {
  }

  Subpatch(Patch *patch, float2 c00, float2 c01, float2 c11, float2 c10)
      : patch(patch), c00(c00), c01(c01), c11(c11), c10(c10)
  {
  }

  int calc_num_inner_verts() const
  {
    int Mu = max(edge_u0.T, edge_u1.T);
    int Mv = max(edge_v0.T, edge_v1.T);
    Mu = max(Mu, 2);
    Mv = max(Mv, 2);
    return (Mu - 1) * (Mv - 1);
  }

  int calc_num_triangles() const
  {
    int Mu = max(edge_u0.T, edge_u1.T);
    int Mv = max(edge_v0.T, edge_v1.T);
    Mu = max(Mu, 2);
    Mv = max(Mv, 2);

    int inner_triangles = (Mu - 2) * (Mv - 2) * 2;
    int edge_triangles = edge_u0.T + edge_u1.T + edge_v0.T + edge_v1.T + (Mu - 2) * 2 +
                         (Mv - 2) * 2;

    return inner_triangles + edge_triangles;
  }

  int get_vert_along_edge(int e, int n) const;

  int get_vert_along_grid_edge(int edge, int n) const
  {
    int Mu = max(edge_u0.T, edge_u1.T);
    int Mv = max(edge_v0.T, edge_v1.T);
    Mu = max(Mu, 2);
    Mv = max(Mv, 2);

    switch (edge) {
      case 0:
        return inner_grid_vert_offset + n * (Mu - 1);
      case 1:
        return inner_grid_vert_offset + (Mu - 1) * (Mv - 2) + n;
      case 2:
        return inner_grid_vert_offset + ((Mu - 1) * (Mv - 1) - 1) - n * (Mu - 1);
      case 3:
        return inner_grid_vert_offset + (Mu - 2) - n;
    }

    return -1;
  }
};

struct Edge {
  /* Number of segments the edge will be diced into, see DiagSplit paper. */
  int T;

  /* top is edge adjacent to start, bottom is adjacent to end. */
  Edge *top, *bottom;

  int top_offset, bottom_offset;
  bool top_indices_decrease, bottom_indices_decrease;

  int start_vert_index;
  int end_vert_index;

  /* Index of the second vert from this edges corner along the edge towards the next corner. */
  int second_vert_index;

  /* Vertices on edge are to be stitched. */
  bool is_stitch_edge;

  /* Key to match this edge with others to be stitched with.
   * The ints in the pair are ordered stitching indices */
  pair<int, int> stitch_edge_key;

  /* Full T along edge (may be larger than T for edges split from ngon edges) */
  int stitch_edge_T;
  int stitch_offset;
  int stitch_top_offset;
  int stitch_start_vert_index;
  int stitch_end_vert_index;

  Edge()
      : T(0),
        top(nullptr),
        bottom(nullptr),
        top_offset(-1),
        bottom_offset(-1),
        top_indices_decrease(false),
        bottom_indices_decrease(false),
        start_vert_index(-1),
        end_vert_index(-1),
        second_vert_index(-1),
        is_stitch_edge(false),
        stitch_edge_T(0),
        stitch_offset(0)
  {
  }

  int get_vert_along_edge(int n) const
  {
    assert(n >= 0 && n <= T);

    if (n == 0) {
      return start_vert_index;
    }
    else if (n == T) {
      return end_vert_index;
    }

    return second_vert_index + n - 1;
  }
};

inline int Subpatch::edge_t::get_vert_along_edge(int n) const
{
  assert(n >= 0 && n <= T);

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

inline int Subpatch::get_vert_along_edge(int edge, int n) const
{
  return edges[edge].get_vert_along_edge(n);
}

CCL_NAMESPACE_END

#endif /* __SUBD_SUBPATCH_H__ */
