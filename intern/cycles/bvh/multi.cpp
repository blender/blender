/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2020-2022 Blender Foundation. */

#include "bvh/multi.h"

#include "util/foreach.h"

CCL_NAMESPACE_BEGIN

BVHMulti::BVHMulti(const BVHParams &params_,
                   const vector<Geometry *> &geometry_,
                   const vector<Object *> &objects_)
    : BVH(params_, geometry_, objects_)
{
}

BVHMulti::~BVHMulti()
{
  foreach (BVH *bvh, sub_bvhs) {
    delete bvh;
  }
}

CCL_NAMESPACE_END
