/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/hash.h"
#include "util/math_float2.h"

CCL_NAMESPACE_BEGIN

class Patch;

/* SubEdge */

struct SubEdge {
  SubEdge(const int start_vert_index, const int end_vert_index)
      : start_vert_index(start_vert_index), end_vert_index(end_vert_index)
  {
  }

  /* Vertex indices. */
  int start_vert_index;
  int end_vert_index;

  /* If edge was split, vertex index in the middle. */
  int mid_vert_index = -1;

  /* Number of segments the edge will be diced into, see DiagSplit paper. */
  int T = 0;

  /* Index of the second vert from this edges corner along the edge towards the next corner. */
  int second_vert_index = -1;

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

  struct Hash {
    size_t operator()(const SubEdge &edge) const
    {
      int a = edge.start_vert_index;
      int b = edge.end_vert_index;
      if (b > a) {
        std::swap(a, b);
      }
      return hash_uint2(a, b);
    }
  };

  struct Equal {
    size_t operator()(const SubEdge &a, const SubEdge &b) const
    {
      return (a.start_vert_index == b.start_vert_index && a.end_vert_index == b.end_vert_index) ||
             (a.start_vert_index == b.end_vert_index && a.end_vert_index == b.start_vert_index);
    }
  };
};

/* SubPatch */

class SubPatch {
 public:
  /* Patch this is a subpatch of. */
  const Patch *patch = nullptr;
  /* Vertex indices for inner grid start at this index. */
  int inner_grid_vert_offset = 0;

  /* Edge of patch. */
  struct Edge {
    SubEdge *edge;

    /* Is the direction of this edge reverse compared to SubEdge? */
    bool reversed;

    /* Get vertex indices in the direction of this patch edge, will take into
     * account the reversed flag to flip the indices. */
    int start_vert_index() const
    {
      return (reversed) ? edge->end_vert_index : edge->start_vert_index;
    }
    int mid_vert_index() const
    {
      return edge->mid_vert_index;
    }
    int end_vert_index() const
    {
      return (reversed) ? edge->start_vert_index : edge->end_vert_index;
    }

    int get_vert_along_edge(const int n_relative) const
    {
      assert(n_relative >= 0 && n_relative <= edge->T);

      const int n = (reversed) ? edge->T - n_relative : n_relative;

      return edge->get_vert_along_edge(n);
    }
  };

  /*
   *                edge_u1
   *          uv01 ←-------- uv11
   *          |                 ↑
   *  edge_v0 |                 | edge_v1
   *          ↓                 |
   *          uv00 --------→ uv10
   *                edge_u0
   */

  /* UV within patch, counter-clockwise starting from uv (0, 0) towards (1, 0) etc. */
  float2 uv00 = zero_float2();
  float2 uv10 = make_float2(1.0f, 0.0f);
  float2 uv11 = one_float2();
  float2 uv01 = make_float2(0.0f, 1.0f);

  /* Edges of this subpatch. */
  union {
    Edge edges[4] = {};
    struct {
      Edge edge_u0, edge_v1, edge_u1, edge_v0;
    };
  };

  explicit SubPatch(const Patch *patch = nullptr) : patch(patch) {}

  int calc_num_inner_verts() const
  {
    const int Mu = max(edge_u0.edge->T, edge_u1.edge->T);
    const int Mv = max(edge_v0.edge->T, edge_v1.edge->T);
    return (Mu - 1) * (Mv - 1);
  }

  int calc_num_triangles() const
  {
    const int Mu = max(edge_u0.edge->T, edge_u1.edge->T);
    const int Mv = max(edge_v0.edge->T, edge_v1.edge->T);

    if (Mu == 1) {
      return Mv * 2;
    }
    if (Mv == 1) {
      return Mu * 2;
    }

    const int inner_triangles = (Mu - 2) * (Mv - 2) * 2;
    const int edge_triangles = edge_u0.edge->T + edge_u1.edge->T + edge_v0.edge->T +
                               edge_v1.edge->T + ((Mu - 2) * 2) + ((Mv - 2) * 2);

    return inner_triangles + edge_triangles;
  }

  int get_vert_along_edge(const int edge, const int n) const
  {
    return edges[edge].get_vert_along_edge(n);
  }

  int get_vert_along_edge_reverse(const int edge, const int n) const
  {
    return get_vert_along_edge(edge, edges[edge].edge->T - n);
  }

  int get_vert_along_grid_edge(const int edge, const int n) const
  {
    const int Mu = max(edge_u0.edge->T, edge_u1.edge->T);
    const int Mv = max(edge_v0.edge->T, edge_v1.edge->T);

    assert(Mu >= 2 && Mv >= 2);

    switch (edge) {
      case 0: {
        return inner_grid_vert_offset + n;
      }
      case 1: {
        return inner_grid_vert_offset + (Mu - 2) + n * (Mu - 1);
      }
      case 2: {
        const int reverse_n = (Mu - 2) - n;
        return inner_grid_vert_offset + (Mu - 1) * (Mv - 2) + reverse_n;
      }
      case 3: {
        const int reverse_n = (Mv - 2) - n;
        return inner_grid_vert_offset + reverse_n * (Mu - 1);
      }
      default:
        assert(0);
        break;
    }

    return -1;
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
