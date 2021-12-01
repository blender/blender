/*
 * Copyright 2018, Blender Foundation.
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

#ifndef __BVH_EMBREE_H__
#define __BVH_EMBREE_H__

#ifdef WITH_EMBREE

#  include <embree3/rtcore.h>
#  include <embree3/rtcore_scene.h>

#  include "bvh/bvh.h"
#  include "bvh/params.h"

#  include "util/thread.h"
#  include "util/types.h"
#  include "util/vector.h"

CCL_NAMESPACE_BEGIN

class Hair;
class Mesh;
class PointCloud;

class BVHEmbree : public BVH {
 public:
  void build(Progress &progress, Stats *stats, RTCDevice rtc_device);
  void refit(Progress &progress);

  RTCScene scene;

 protected:
  friend class BVH;
  BVHEmbree(const BVHParams &params,
            const vector<Geometry *> &geometry,
            const vector<Object *> &objects);
  virtual ~BVHEmbree();

  void add_object(Object *ob, int i);
  void add_instance(Object *ob, int i);
  void add_curves(const Object *ob, const Hair *hair, int i);
  void add_points(const Object *ob, const PointCloud *pointcloud, int i);
  void add_triangles(const Object *ob, const Mesh *mesh, int i);

 private:
  void set_tri_vertex_buffer(RTCGeometry geom_id, const Mesh *mesh, const bool update);
  void set_curve_vertex_buffer(RTCGeometry geom_id, const Hair *hair, const bool update);
  void set_point_vertex_buffer(RTCGeometry geom_id,
                               const PointCloud *pointcloud,
                               const bool update);

  RTCDevice rtc_device;
  enum RTCBuildQuality build_quality;
};

CCL_NAMESPACE_END

#endif /* WITH_EMBREE */

#endif /* __BVH_EMBREE_H__ */
