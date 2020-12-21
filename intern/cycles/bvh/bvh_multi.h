/*
 * Copyright 2020, Blender Foundation.
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

#ifndef __BVH_MULTI_H__
#define __BVH_MULTI_H__

#include "bvh/bvh.h"
#include "bvh/bvh_params.h"

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
};

CCL_NAMESPACE_END

#endif /* __BVH_MULTI_H__ */
