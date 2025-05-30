/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/hash.h"
#include "util/math_float2.h"

CCL_NAMESPACE_BEGIN

class Patch;

enum {
  DSPLIT_NON_UNIFORM = -1,
  DSPLIT_MAX_DEPTH = 16,
  DSPLIT_MAX_SEGMENTS = 8,
};

/* SubEdge */

struct SubEdge {
  SubEdge(const int start_vert_index, const int end_vert_index, const int depth)
      : start_vert_index(start_vert_index), end_vert_index(end_vert_index), depth(depth)
  {
  }

  /* Vertex indices. */
  int start_vert_index;
  int end_vert_index;

  /* If edge was split, vertex index in the middle. */
  int mid_vert_index = -1;

  /* Number of segments the edge will be diced into, see DiagSplit paper. */
  int T = 0;

  /* Estimated length of edge, for determining preferred split direction. */
  float length = 0.0f;

  /* Index of the second vert from this edges corner along the edge towards the next corner. */
  int second_vert_index = -1;

  /* How many times an edge was subdivided to get this edge. */
  int depth = 0;

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

  bool must_split() const
  {
    return T == DSPLIT_NON_UNIFORM;
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
  /* Face and corner. */
  int face_index = 0;
  int corner = 0;
  /* Is a triangular patch instead of a quad patch? */
  enum { TRIANGLE, QUAD } shape = QUAD;
  /* Vertex indices for inner grid start at this index. */
  int inner_grid_vert_offset = 0;
  /* Triangle indices. */
  int triangles_offset = 0;

  /* Edge of patch. */
  struct Edge {
    SubEdge *edge;

    /* Is the direction of this edge reverse compared to SubEdge? */
    bool reversed;

    /* Is this subpatch responsible for owning attributes for the start vertex? */
    bool own_vertex;
    /* Is this subpatch responsible for owning attributes for edge vertices? */
    bool own_edge;

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
   *               edge2
   *        uv3 ←------------ uv2
   *        |                   ↑
   *  edge3 |                   | edge1
   *        ↓                   |
   *        uv0 ------------→ uv1
   *               edge0
   *
   *         uv2
   *         | \
   *         |  \
   *  edge2  |   \  edge1
   *         |    \
   *         ↓     \
   *         uv0 --→ uv1
   *            edge0
   */

  /* UV within patch, counter-clockwise starting from uv (0, 0) towards (1, 0) etc. */
  float2 uvs[4] = {zero_float2(), make_float2(1.0f, 0.0f), one_float2(), make_float2(0.0f, 1.0f)};

  /* Edges of this subpatch. */
  Edge edges[4] = {};

  explicit SubPatch(const Patch *patch, const int face_index, const int corner = 0)
      : patch(patch), face_index(face_index), corner(corner)
  {
  }

  int calc_num_inner_verts() const
  {
    if (shape == TRIANGLE) {
      const int M = max(max(edges[0].edge->T, edges[1].edge->T), edges[2].edge->T);
      if (M <= 2) {
        /* No inner grid. */
        return 0;
      }
      /* 1 + 2 + .. + M-1 */
      return M * (M - 1) / 2;
    }

    const int Mu = max(edges[0].edge->T, edges[2].edge->T);
    const int Mv = max(edges[3].edge->T, edges[1].edge->T);
    return (Mu - 1) * (Mv - 1);
  }

  int calc_num_triangles() const
  {
    if (shape == TRIANGLE) {
      const int M = max(max(edges[0].edge->T, edges[1].edge->T), edges[2].edge->T);
      if (M == 1) {
        return 1;
      }
      if (M == 2) {
        return edges[0].edge->T + edges[1].edge->T + edges[2].edge->T - 2;
      }

      const int inner_M = M - 2;
      const int inner_triangles = inner_M * inner_M;
      const int edge_triangles = edges[0].edge->T + edges[1].edge->T + edges[2].edge->T +
                                 inner_M * 3;
      return inner_triangles + edge_triangles;
    }

    const int Mu = max(edges[0].edge->T, edges[2].edge->T);
    const int Mv = max(edges[3].edge->T, edges[1].edge->T);

    if (Mu == 1) {
      return edges[3].edge->T + edges[1].edge->T;
    }
    if (Mv == 1) {
      return edges[0].edge->T + edges[2].edge->T;
    }

    const int inner_triangles = (Mu - 2) * (Mv - 2) * 2;
    const int edge_triangles = edges[0].edge->T + edges[2].edge->T + edges[3].edge->T +
                               edges[1].edge->T + ((Mu - 2) * 2) + ((Mv - 2) * 2);

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

  int get_inner_grid_vert_triangle(int i, int j) const
  {
    /* Rows `(1 + 2 + .. + j)`, and column `i`. */
    const int offset = j * (j + 1) / 2 + i;
    assert(offset < calc_num_inner_verts());
    return inner_grid_vert_offset + offset;
  }

  int get_vert_along_grid_edge(const int edge, const int n) const
  {
    if (shape == TRIANGLE) {
      const int M = max(max(edges[0].edge->T, edges[1].edge->T), edges[2].edge->T);
      const int inner_M = M - 2;
      assert(M >= 2);

      switch (edge) {
        case 0: {
          return get_inner_grid_vert_triangle(n, n);
        }
        case 1: {
          return get_inner_grid_vert_triangle(inner_M - n, inner_M);
        }
        case 2: {
          return get_inner_grid_vert_triangle(0, inner_M - n);
        }
        default:
          assert(0);
          break;
      }

      return -1;
    }

    const int Mu = max(edges[0].edge->T, edges[2].edge->T);
    const int Mv = max(edges[3].edge->T, edges[1].edge->T);

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

  float2 map_uv(float2 uv) const
  {
    /* Map UV from subpatch to patch parametric coordinates. */
    if (shape == TRIANGLE) {
      return clamp((1.0f - uv.x - uv.y) * uvs[0] + uv.x * uvs[1] + uv.y * uvs[2],
                   zero_float2(),
                   one_float2());
    }

    const float2 d0 = interp(uvs[0], uvs[3], uv.y);
    const float2 d1 = interp(uvs[1], uvs[2], uv.y);
    return clamp(interp(d0, d1, uv.x), zero_float2(), one_float2());
  }
};

CCL_NAMESPACE_END
