/*
 * Copyright 2019, NVIDIA Corporation.
 * Copyright 2019, Blender Foundation.
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

#ifndef __BVH_OPTIX_H__
#define __BVH_OPTIX_H__

#ifdef WITH_OPTIX

#  include "bvh/bvh.h"
#  include "bvh/bvh_params.h"
#  include "device/device_memory.h"

CCL_NAMESPACE_BEGIN

class Geometry;
class Optix;

class BVHOptiX : public BVH {
  friend class BVH;

 public:
  uint64_t optix_handle;
  uint64_t optix_data_handle;
  bool do_refit;

  BVHOptiX(const BVHParams &params,
           const vector<Geometry *> &geometry,
           const vector<Object *> &objects);
  virtual ~BVHOptiX();

  virtual void build(Progress &progress, Stats *) override;
  virtual void copy_to_device(Progress &progress, DeviceScene *dscene) override;

 private:
  void pack_blas();
  void pack_tlas();

  virtual void pack_nodes(const BVHNode *) override;
  virtual void refit_nodes() override;

  virtual BVHNode *widen_children_nodes(const BVHNode *) override;
};

CCL_NAMESPACE_END

#endif /* WITH_OPTIX */

#endif /* __BVH_OPTIX_H__ */
