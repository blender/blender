/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup stl
 */

#pragma once

#include <cstdint>

#include "BLI_math_vec_types.hh"
#include "BLI_set.hh"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "DNA_mesh_types.h"

namespace blender::io::stl {
class Triangle {
 public:
  int v1, v2, v3;
  /* Based on an old version of Python's frozenset hash
   * https://web.archive.org/web/20220520211017/https://stackoverflow.com/questions/20832279/python-frozenset-hashing-algorithm-implementation
   */
  uint64_t hash() const
  {
    uint64_t res = 1927868237UL;
    res *= 4;
    res ^= (v1 ^ (v1 << 16) ^ 89869747UL) * 3644798167UL;
    res ^= (v2 ^ (v2 << 16) ^ 89869747UL) * 3644798167UL;
    res ^= (v3 ^ (v3 << 16) ^ 89869747UL) * 3644798167UL;
    return res * 69069U + 907133923UL;
  }
  friend bool operator==(const Triangle &a, const Triangle &b)
  {
    bool i = (a.v1 == b.v1) && (a.v2 == b.v2) && (a.v3 == b.v3);
    bool j = (a.v1 == b.v1) && (a.v3 == b.v2) && (a.v2 == b.v3);
    bool k = (a.v2 == b.v1) && (a.v1 == b.v2) && (a.v3 == b.v3);

    bool l = (a.v2 == b.v1) && (a.v3 == b.v2) && (a.v1 == b.v3);
    bool m = (a.v3 == b.v1) && (a.v1 == b.v2) && (a.v2 == b.v3);
    bool n = (a.v3 == b.v1) && (a.v2 == b.v2) && (a.v1 == b.v3);

    return i || j || k || l || m || n;
  }
};

class STLMeshHelper {
 private:
  VectorSet<float3> m_verts;
  VectorSet<Triangle> m_tris;
  Vector<float3> m_loop_normals;
  int m_num_degenerate_tris;
  int m_num_duplicate_tris;
  const bool m_use_custom_normals;

 public:
  STLMeshHelper(int num_tris, bool use_custom_normals);

  /* Creates a new triangle from specified vertex locations,
   * duplicate vertices and triangles are merged.
   */
  bool add_triangle(const float3 &a, const float3 &b, const float3 &c);
  void add_triangle(const float3 &a,
                    const float3 &b,
                    const float3 &c,
                    const float3 &custom_normal);
  Mesh *to_mesh(Main *bmain, char *mesh_name);
};

}  // namespace blender::io::stl
