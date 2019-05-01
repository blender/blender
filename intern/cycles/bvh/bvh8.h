/*
 * Original code Copyright 2017, Intel Corporation
 * Modifications Copyright 2018, Blender Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of Intel Corporation nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __BVH8_H__
#define __BVH8_H__

#include "bvh/bvh.h"
#include "bvh/bvh_params.h"

#include "util/util_types.h"
#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

class BVHNode;
struct BVHStackEntry;
class BVHParams;
class BoundBox;
class LeafNode;
class Object;
class Progress;

#define BVH_ONODE_SIZE 16
#define BVH_ONODE_LEAF_SIZE 1
#define BVH_UNALIGNED_ONODE_SIZE 28

/* BVH8
 *
 * Octo BVH, with each node having eight children, to use with SIMD instructions.
 */
class BVH8 : public BVH {
 protected:
  /* constructor */
  friend class BVH;
  BVH8(const BVHParams &params, const vector<Object *> &objects);

  /* Building process. */
  virtual BVHNode *widen_children_nodes(const BVHNode *root) override;

  /* pack */
  void pack_nodes(const BVHNode *root) override;

  void pack_leaf(const BVHStackEntry &e, const LeafNode *leaf);
  void pack_inner(const BVHStackEntry &e, const BVHStackEntry *en, int num);

  void pack_aligned_inner(const BVHStackEntry &e, const BVHStackEntry *en, int num);
  void pack_aligned_node(int idx,
                         const BoundBox *bounds,
                         const int *child,
                         const uint visibility,
                         const float time_from,
                         const float time_to,
                         const int num);

  void pack_unaligned_inner(const BVHStackEntry &e, const BVHStackEntry *en, int num);
  void pack_unaligned_node(int idx,
                           const Transform *aligned_space,
                           const BoundBox *bounds,
                           const int *child,
                           const uint visibility,
                           const float time_from,
                           const float time_to,
                           const int num);

  /* refit */
  void refit_nodes() override;
  void refit_node(int idx, bool leaf, BoundBox &bbox, uint &visibility);
};

CCL_NAMESPACE_END

#endif /* __BVH8_H__ */
