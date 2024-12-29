/* SPDX-FileCopyrightText: 2020-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "bvh/bvh.h"
#include "bvh/params.h"

#include <util/unique_ptr.h>
#include <util/vector.h>

CCL_NAMESPACE_BEGIN

class BVHMulti : public BVH {
 public:
  vector<unique_ptr<BVH>> sub_bvhs;

  BVHMulti(const BVHParams &params,
           const vector<Geometry *> &geometry,
           const vector<Object *> &objects);

 protected:
  void replace_geometry(const vector<Geometry *> &geometry,
                        const vector<Object *> &objects) override;
};

CCL_NAMESPACE_END
