/* SPDX-FileCopyrightText: 2011-2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_HIPRT

#  include "bvh/hiprt.h"

#  include "scene/mesh.h"
#  include "scene/object.h"

#  include "device/hiprt/device_impl.h"

CCL_NAMESPACE_BEGIN

BVHHIPRT::BVHHIPRT(const BVHParams &params,
                   const vector<Geometry *> &geometry,
                   const vector<Object *> &objects,
                   Device *in_device)
    : BVH(params, geometry, objects),
      hiprt_geom(nullptr),
      custom_primitive_bound(in_device, "Custom Primitive Bound", MEM_READ_ONLY),
      triangle_index(in_device, "HIPRT Triangle Index", MEM_READ_ONLY),
      vertex_data(in_device, "vertex_data", MEM_READ_ONLY),
      device(in_device)
{
  triangle_mesh = {nullptr};
  custom_prim_aabb = {nullptr};
}

BVHHIPRT::~BVHHIPRT()
{
  custom_primitive_bound.free();
  triangle_index.free();
  vertex_data.free();
  device->release_bvh(this);
}

CCL_NAMESPACE_END

#endif
