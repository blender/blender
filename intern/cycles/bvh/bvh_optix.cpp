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

#ifdef WITH_OPTIX

#  include "bvh/bvh_optix.h"

CCL_NAMESPACE_BEGIN

BVHOptiX::BVHOptiX(const BVHParams &params_,
                   const vector<Geometry *> &geometry_,
                   const vector<Object *> &objects_,
                   Device *device)
    : BVH(params_, geometry_, objects_),
      traversable_handle(0),
      as_data(device, params_.top_level ? "optix tlas" : "optix blas", false),
      motion_transform_data(device, "optix motion transform", false)
{
}

BVHOptiX::~BVHOptiX()
{
  // Acceleration structure memory is freed via the 'as_data' destructor
}

CCL_NAMESPACE_END

#endif /* WITH_OPTIX */
