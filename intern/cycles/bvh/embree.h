/* SPDX-FileCopyrightText: 2018-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __BVH_EMBREE_H__
#define __BVH_EMBREE_H__

#ifdef WITH_EMBREE

#  if EMBREE_MAJOR_VERSION >= 4
#    include <embree4/rtcore.h>
#    include <embree4/rtcore_scene.h>
#  else
#    include <embree3/rtcore.h>
#    include <embree3/rtcore_scene.h>
#  endif

#  include "bvh/bvh.h"
#  include "bvh/params.h"

#  include "util/string.h"
#  include "util/thread.h"
#  include "util/types.h"
#  include "util/vector.h"

CCL_NAMESPACE_BEGIN

class Hair;
class Mesh;
class PointCloud;

class BVHEmbree : public BVH {
 public:
  void build(Progress &progress,
             Stats *stats,
             RTCDevice rtc_device,
             const bool isSyclEmbreeDevice = false);
  void refit(Progress &progress);

#  if defined(WITH_EMBREE_GPU) && RTC_VERSION >= 40302
  bool offload_scenes_to_gpu(const vector<RTCScene> &scenes);
#  endif

  string get_last_error_message();

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
  bool rtc_device_is_sycl;
  enum RTCBuildQuality build_quality;
};

CCL_NAMESPACE_END

#endif /* WITH_EMBREE */

#endif /* __BVH_EMBREE_H__ */
