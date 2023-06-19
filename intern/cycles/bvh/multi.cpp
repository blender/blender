/* SPDX-FileCopyrightText: 2020-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

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

void BVHMulti::replace_geometry(const vector<Geometry *> &geometry,
                                const vector<Object *> &objects)
{
  foreach (BVH *bvh, sub_bvhs) {
    bvh->replace_geometry(geometry, objects);
  }
}

CCL_NAMESPACE_END
