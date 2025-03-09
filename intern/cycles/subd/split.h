/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

/* DiagSplit: Parallel, Crack-free, Adaptive Tessellation for Micro-polygon Rendering
 * Splits up patches and determines edge tessellation factors for dicing. Patch
 * evaluation at arbitrary points is required for this to work. See the paper
 * for more details. */

#include "scene/mesh.h"

#include "subd/dice.h"
#include "subd/subpatch.h"

#include "util/deque.h"
#include "util/types.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

class Mesh;
class Patch;

class DiagSplit {
  SubdParams params;

  vector<SubPatch> subpatches;
  /* `deque` is used so that element pointers remain valid when size is changed. */
  deque<SubEdge> edges;

  float3 to_world(const Patch *patch, const float2 uv);
  int T(const Patch *patch,
        const float2 Pstart,
        const float2 Pend,
        bool recursive_resolve = false);

  void limit_edge_factor(int &T, const Patch *patch, const float2 Pstart, const float2 Pend);
  void resolve_edge_factors(SubPatch &sub);

  void partition_edge(const Patch *patch,
                      float2 *P,
                      int *t0,
                      int *t1,
                      const float2 Pstart,
                      const float2 Pend,
                      const int t);

  void split(SubPatch &sub, const int depth = 0);

  int num_alloced_verts = 0;
  int alloc_verts(const int n); /* Returns start index of new verts. */

 public:
  SubEdge *alloc_edge();

  explicit DiagSplit(const SubdParams &params);

  void split_patches(const Patch *patches, const size_t patches_byte_stride);

  void split_quad(const Mesh::SubdFace &face, const Patch *patch);
  void split_ngon(const Mesh::SubdFace &face,
                  const Patch *patches,
                  const size_t patches_byte_stride);

  void post_split();
};

CCL_NAMESPACE_END
