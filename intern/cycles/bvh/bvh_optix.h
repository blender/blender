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

class BVHOptiX : public BVH {
 public:
  Device *device;
  uint64_t traversable_handle;
  device_only_memory<char> as_data;
  device_only_memory<char> motion_transform_data;

 protected:
  friend class BVH;
  BVHOptiX(const BVHParams &params,
           const vector<Geometry *> &geometry,
           const vector<Object *> &objects,
           Device *device);
  virtual ~BVHOptiX();
};

CCL_NAMESPACE_END

#endif /* WITH_OPTIX */

#endif /* __BVH_OPTIX_H__ */
