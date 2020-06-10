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
#  include "bvh/bvh_params.h"

#  include "util/util_thread.h"
#  include "util/util_types.h"
#  include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

class Geometry;
class Hair;
class Mesh;

class BVHEmbree : public BVH {
 public:
  virtual void build(Progress &progress, Stats *stats) override;
  virtual void copy_to_device(Progress &progress, DeviceScene *dscene) override;
  virtual ~BVHEmbree();
  RTCScene scene;
  static void destroy(RTCScene);

  /* Building process. */
  virtual BVHNode *widen_children_nodes(const BVHNode *root) override;

 protected:
  friend class BVH;
  BVHEmbree(const BVHParams &params,
            const vector<Geometry *> &geometry,
            const vector<Object *> &objects);

  virtual void pack_nodes(const BVHNode *) override;
  virtual void refit_nodes() override;

  void add_object(Object *ob, int i);
  void add_instance(Object *ob, int i);
  void add_curves(const Object *ob, const Hair *hair, int i);
  void add_triangles(const Object *ob, const Mesh *mesh, int i);

  ssize_t mem_used;

  void add_delayed_delete_scene(RTCScene scene)
  {
    delayed_delete_scenes.push_back(scene);
  }
  BVHEmbree *top_level;

 private:
  void delete_rtcScene();
  void update_tri_vertex_buffer(RTCGeometry geom_id, const Mesh *mesh);
  void update_curve_vertex_buffer(RTCGeometry geom_id, const Hair *hair);

  static RTCDevice rtc_shared_device;
  static int rtc_shared_users;
  static thread_mutex rtc_shared_mutex;

  Stats *stats;
  vector<RTCScene> delayed_delete_scenes;
  int curve_subdivisions;
  enum RTCBuildQuality build_quality;
  bool dynamic_scene;
};

CCL_NAMESPACE_END

#endif /* WITH_EMBREE */

#endif /* __BVH_EMBREE_H__ */
