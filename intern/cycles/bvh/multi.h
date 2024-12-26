/* SPDX-FileCopyrightText: 2020-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "bvh/bvh.h"
#include "bvh/params.h"

CCL_NAMESPACE_BEGIN

class BVHMulti : public BVH {
 public:
  vector<BVH *> sub_bvhs;

 protected:
  friend class BVH;
  BVHMulti(const BVHParams &params,
           const vector<Geometry *> &geometry,
           const vector<Object *> &objects);
  virtual ~BVHMulti();

  virtual void replace_geometry(const vector<Geometry *> &geometry,
                                const vector<Object *> &objects);
};

CCL_NAMESPACE_END
