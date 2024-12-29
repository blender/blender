/* SPDX-FileCopyrightText: 2020-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "bvh/multi.h"

CCL_NAMESPACE_BEGIN

BVHMulti::BVHMulti(const BVHParams &params_,
                   const vector<Geometry *> &geometry_,
                   const vector<Object *> &objects_)
    : BVH(params_, geometry_, objects_)
{
}

void BVHMulti::replace_geometry(const vector<Geometry *> &geometry,
                                const vector<Object *> &objects)
{
  for (unique_ptr<BVH> &bvh : sub_bvhs) {
    bvh->replace_geometry(geometry, objects);
  }
}

CCL_NAMESPACE_END
