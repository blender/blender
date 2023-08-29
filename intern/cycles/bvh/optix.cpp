/* SPDX-FileCopyrightText: 2019 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2019-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_OPTIX

#  include "device/device.h"

#  include "bvh/optix.h"

CCL_NAMESPACE_BEGIN

BVHOptiX::BVHOptiX(const BVHParams &params_,
                   const vector<Geometry *> &geometry_,
                   const vector<Object *> &objects_,
                   Device *device)
    : BVH(params_, geometry_, objects_),
      device(device),
      traversable_handle(0),
      as_data(make_unique<device_only_memory<char>>(
          device, params.top_level ? "optix tlas" : "optix blas", false)),
      motion_transform_data(
          make_unique<device_only_memory<char>>(device, "optix motion transform", false))
{
}

BVHOptiX::~BVHOptiX()
{
  /* Acceleration structure memory is delayed freed on device, since deleting the
   * BVH may happen while still being used for rendering. */
  device->release_optix_bvh(this);
}

CCL_NAMESPACE_END

#endif /* WITH_OPTIX */
