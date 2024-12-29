/* SPDX-FileCopyrightText: 2019 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2019-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef WITH_OPTIX

#  include "bvh/bvh.h"
#  include "bvh/params.h"

#  include "device/memory.h"

#  include "util/unique_ptr.h"

CCL_NAMESPACE_BEGIN

class BVHOptiX : public BVH {
 public:
  Device *device;
  uint64_t traversable_handle;
  unique_ptr<device_only_memory<char>> as_data;
  unique_ptr<device_only_memory<char>> motion_transform_data;

  BVHOptiX(const BVHParams &params,
           const vector<Geometry *> &geometry,
           const vector<Object *> &objects,
           Device *device);
  ~BVHOptiX() override;
};

CCL_NAMESPACE_END

#endif /* WITH_OPTIX */
