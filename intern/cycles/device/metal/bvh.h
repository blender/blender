/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef WITH_METAL

#  include "bvh/bvh.h"
#  include "bvh/params.h"
#  include "device/memory.h"

#  include <Metal/Metal.h>

CCL_NAMESPACE_BEGIN

class BVHMetal : public BVH {
 public:
  API_AVAILABLE(macos(11.0))
  id<MTLAccelerationStructure> accel_struct = nil;
  bool accel_struct_building = false;

  API_AVAILABLE(macos(11.0))
  vector<id<MTLAccelerationStructure>> blas_array;

  API_AVAILABLE(macos(11.0))
  vector<id<MTLAccelerationStructure>> unique_blas_array;

  bool motion_blur = false;

  Stats &stats;

  bool build(Progress &progress, id<MTLDevice> device, id<MTLCommandQueue> queue, bool refit);

  BVHMetal(const BVHParams &params,
           const vector<Geometry *> &geometry,
           const vector<Object *> &objects,
           Device *device);
  virtual ~BVHMetal();

  bool build_BLAS(Progress &progress, id<MTLDevice> device, id<MTLCommandQueue> queue, bool refit);
  bool build_BLAS_mesh(Progress &progress,
                       id<MTLDevice> device,
                       id<MTLCommandQueue> queue,
                       Geometry *const geom,
                       bool refit);
  bool build_BLAS_hair(Progress &progress,
                       id<MTLDevice> device,
                       id<MTLCommandQueue> queue,
                       Geometry *const geom,
                       bool refit);
  bool build_BLAS_pointcloud(Progress &progress,
                             id<MTLDevice> device,
                             id<MTLCommandQueue> queue,
                             Geometry *const geom,
                             bool refit);
  bool build_TLAS(Progress &progress, id<MTLDevice> device, id<MTLCommandQueue> queue, bool refit);
};

CCL_NAMESPACE_END

#endif /* WITH_METAL */
