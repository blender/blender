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

#include "util/set.h"
#include "util/types.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

class Mesh;
class Patch;

class DiagSplit {
 private:
  SubdParams params;
  vector<SubPatch> subpatches;
  unordered_set<SubEdge, SubEdge::Hash, SubEdge::Equal> edges;
  int num_verts = 0;
  int num_triangles = 0;

  /* Allocate vertices, edges and subpatches. */
  int alloc_verts(const int num);
  SubEdge *alloc_edge(const int v0, const int v1);
  void alloc_edge(SubPatch::Edge *sub_edge, const int v0, const int v1);
  void alloc_subpatch(SubPatch &&sub);

  /* Compute edge factors. */
  float3 to_world(const Patch *patch, const float2 uv);
  int T(const Patch *patch,
        const float2 Pstart,
        const float2 Pend,
        const int depth,
        const bool recursive_resolve = false);
  int limit_edge_factor(const Patch *patch, const float2 Pstart, const float2 Pend, const int T);
  void assign_edge_factor(SubEdge *edge, const int T);
  void resolve_edge_factors(const SubPatch &sub, const int depth);

  /* Split edge, subpatch, quad and n-gon. */
  float2 split_edge(
      const Patch *patch, SubPatch::Edge *edge, float2 Pstart, float2 Pend, const int depth);
  void split(SubPatch &&sub, const int depth = 0);
  void split_quad(const Mesh::SubdFace &face, const Patch *patch);
  void split_ngon(const Mesh::SubdFace &face,
                  const Patch *patches,
                  const size_t patches_byte_stride);

 public:
  explicit DiagSplit(const SubdParams &params);

  void split_patches(const Patch *patches, const size_t patches_byte_stride);
};

CCL_NAMESPACE_END
