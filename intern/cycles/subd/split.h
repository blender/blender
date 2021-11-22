/*
 * Copyright 2011-2013 Blender Foundation
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

#ifndef __SUBD_SPLIT_H__
#define __SUBD_SPLIT_H__

/* DiagSplit: Parallel, Crack-free, Adaptive Tessellation for Micropolygon Rendering
 * Splits up patches and determines edge tessellation factors for dicing. Patch
 * evaluation at arbitrary points is required for this to work. See the paper
 * for more details. */

#include "subd/dice.h"
#include "subd/subpatch.h"

#include "util/deque.h"
#include "util/types.h"
#include "util/vector.h"

#include <deque>

CCL_NAMESPACE_BEGIN

class Mesh;
class Patch;

class DiagSplit {
  SubdParams params;

  vector<Subpatch> subpatches;
  /* `deque` is used so that element pointers remain valid when size is changed. */
  deque<Edge> edges;

  float3 to_world(Patch *patch, float2 uv);
  int T(Patch *patch, float2 Pstart, float2 Pend, bool recursive_resolve = false);

  void limit_edge_factor(int &T, Patch *patch, float2 Pstart, float2 Pend);
  void resolve_edge_factors(Subpatch &sub);

  void partition_edge(
      Patch *patch, float2 *P, int *t0, int *t1, float2 Pstart, float2 Pend, int t);

  void split(Subpatch &sub, int depth = 0);

  int num_alloced_verts = 0;
  int alloc_verts(int n); /* Returns start index of new verts. */

 public:
  Edge *alloc_edge();

  explicit DiagSplit(const SubdParams &params);

  void split_patches(Patch *patches, size_t patches_byte_stride);

  void split_quad(const Mesh::SubdFace &face, Patch *patch);
  void split_ngon(const Mesh::SubdFace &face, Patch *patches, size_t patches_byte_stride);

  void post_split();
};

CCL_NAMESPACE_END

#endif /* __SUBD_SPLIT_H__ */
