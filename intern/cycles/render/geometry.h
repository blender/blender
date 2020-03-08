/*
 * Copyright 2011-2020 Blender Foundation
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

#ifndef __GEOMETRY_H__
#define __GEOMETRY_H__

#include "graph/node.h"

#include "bvh/bvh_params.h"

#include "render/attribute.h"

#include "util/util_boundbox.h"
#include "util/util_transform.h"
#include "util/util_set.h"
#include "util/util_types.h"
#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

class BVH;
class Device;
class DeviceScene;
class Mesh;
class Progress;
class RenderStats;
class Scene;
class SceneParams;
class Shader;

/* Geometry
 *
 * Base class for geometric types like Mesh and Hair. */

class Geometry : public Node {
 public:
  NODE_ABSTRACT_DECLARE

  enum Type {
    MESH,
    HAIR,
  };

  Type type;

  /* Attributes */
  AttributeSet attributes;

  /* Shaders */
  vector<Shader *> used_shaders;

  /* Transform */
  BoundBox bounds;
  bool transform_applied;
  bool transform_negative_scaled;
  Transform transform_normal;

  /* Motion Blur */
  uint motion_steps;
  bool use_motion_blur;

  /* Maximum number of motion steps supported (due to Embree). */
  static const uint MAX_MOTION_STEPS = 129;

  /* BVH */
  BVH *bvh;
  size_t attr_map_offset;
  size_t prim_offset;
  size_t optix_prim_offset;

  /* Shader Properties */
  bool has_volume;         /* Set in the device_update_flags(). */
  bool has_surface_bssrdf; /* Set in the device_update_flags(). */

  /* Update Flags */
  bool need_update;
  bool need_update_rebuild;

  /* Constructor/Destructor */
  explicit Geometry(const NodeType *node_type, const Type type);
  virtual ~Geometry();

  /* Geometry */
  virtual void clear();
  virtual void compute_bounds() = 0;
  virtual void apply_transform(const Transform &tfm, const bool apply_to_motion) = 0;

  /* Attribute Requests */
  bool need_attribute(Scene *scene, AttributeStandard std);
  bool need_attribute(Scene *scene, ustring name);

  /* UDIM */
  virtual void get_uv_tiles(ustring map, unordered_set<int> &tiles) = 0;

  /* Convert between normalized -1..1 motion time and index in the
   * VERTEX_MOTION attribute. */
  float motion_time(int step) const;
  int motion_step(float time) const;

  /* BVH */
  void compute_bvh(Device *device,
                   DeviceScene *dscene,
                   SceneParams *params,
                   Progress *progress,
                   int n,
                   int total);

  /* Check whether the geometry should have own BVH built separately. Briefly,
   * own BVH is needed for geometry, if:
   *
   * - It is instanced multiple times, so each instance object should share the
   *   same BVH tree.
   * - Special ray intersection is needed, for example to limit subsurface rays
   *   to only the geometry itself.
   * - The BVH layout requires the top level to only contain instances.
   */
  bool need_build_bvh(BVHLayout layout) const;

  /* Test if the geometry should be treated as instanced. */
  bool is_instanced() const;

  bool has_true_displacement() const;
  bool has_motion_blur() const;
  bool has_voxel_attributes() const;

  /* Updates */
  void tag_update(Scene *scene, bool rebuild);
};

/* Geometry Manager */

class GeometryManager {
 public:
  /* Update Flags */
  bool need_update;
  bool need_flags_update;

  /* Constructor/Destructor */
  GeometryManager();
  ~GeometryManager();

  /* Device Updates */
  void device_update_preprocess(Device *device, Scene *scene, Progress &progress);
  void device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress &progress);
  void device_free(Device *device, DeviceScene *dscene);

  /* Updates */
  void tag_update(Scene *scene);

  /* Statistics */
  void collect_statistics(const Scene *scene, RenderStats *stats);

 protected:
  bool displace(Device *device, DeviceScene *dscene, Scene *scene, Mesh *mesh, Progress &progress);

  void create_volume_mesh(Mesh *mesh, Progress &progress);

  /* Attributes */
  void update_osl_attributes(Device *device,
                             Scene *scene,
                             vector<AttributeRequestSet> &geom_attributes);
  void update_svm_attributes(Device *device,
                             DeviceScene *dscene,
                             Scene *scene,
                             vector<AttributeRequestSet> &geom_attributes);

  /* Compute verts/triangles/curves offsets in global arrays. */
  void mesh_calc_offset(Scene *scene);

  void device_update_object(Device *device, DeviceScene *dscene, Scene *scene, Progress &progress);

  void device_update_mesh(Device *device,
                          DeviceScene *dscene,
                          Scene *scene,
                          bool for_displacement,
                          Progress &progress);

  void device_update_attributes(Device *device,
                                DeviceScene *dscene,
                                Scene *scene,
                                Progress &progress);

  void device_update_bvh(Device *device, DeviceScene *dscene, Scene *scene, Progress &progress);

  void device_update_displacement_images(Device *device, Scene *scene, Progress &progress);

  void device_update_volume_images(Device *device, Scene *scene, Progress &progress);
};

CCL_NAMESPACE_END

#endif /* __GEOMETRY_H__ */
