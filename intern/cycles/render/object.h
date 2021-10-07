/*
 * Copyright 2011-2013 Blender Foundation
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

#ifndef __OBJECT_H__
#define __OBJECT_H__

#include "graph/node.h"

/* included as Object::set_particle_system defined through NODE_SOCKET_API does
 * not select the right Node::set overload as it does not know that ParticleSystem
 * is a Node */
#include "render/particles.h"
#include "render/scene.h"

#include "util/util_array.h"
#include "util/util_boundbox.h"
#include "util/util_param.h"
#include "util/util_thread.h"
#include "util/util_transform.h"
#include "util/util_types.h"
#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Geometry;
class ParticleSystem;
class Progress;
class Scene;
struct Transform;
struct UpdateObjectTransformState;
class ObjectManager;

/* Object */

class Object : public Node {
 public:
  NODE_DECLARE

  NODE_SOCKET_API(Geometry *, geometry)
  NODE_SOCKET_API(Transform, tfm)
  BoundBox bounds;
  NODE_SOCKET_API(uint, random_id)
  NODE_SOCKET_API(int, pass_id)
  NODE_SOCKET_API(float3, color)
  NODE_SOCKET_API(ustring, asset_name)
  vector<ParamValue> attributes;
  NODE_SOCKET_API(uint, visibility)
  NODE_SOCKET_API_ARRAY(array<Transform>, motion)
  NODE_SOCKET_API(bool, hide_on_missing_motion)
  NODE_SOCKET_API(bool, use_holdout)
  NODE_SOCKET_API(bool, is_shadow_catcher)
  NODE_SOCKET_API(float, shadow_terminator_shading_offset)
  NODE_SOCKET_API(float, shadow_terminator_geometry_offset)

  NODE_SOCKET_API(float3, dupli_generated)
  NODE_SOCKET_API(float2, dupli_uv)

  NODE_SOCKET_API(ParticleSystem *, particle_system);
  NODE_SOCKET_API(int, particle_index);

  NODE_SOCKET_API(float, ao_distance)

  Object();
  ~Object();

  void tag_update(Scene *scene);

  void compute_bounds(bool motion_blur);
  void apply_transform(bool apply_to_motion);

  /* Convert between normalized -1..1 motion time and index
   * in the motion array. */
  bool use_motion() const;
  float motion_time(int step) const;
  int motion_step(float time) const;
  void update_motion();

  /* Maximum number of motion steps supported (due to Embree). */
  static const uint MAX_MOTION_STEPS = 129;

  /* Check whether object is traceable and it worth adding it to
   * kernel scene.
   */
  bool is_traceable() const;

  /* Combine object's visibility with all possible internal run-time
   * determined flags which denotes trace-time visibility.
   */
  uint visibility_for_tracing() const;

  /* Returns the index that is used in the kernel for this object. */
  int get_device_index() const;

  /* Compute step size from attributes, shaders, transforms. */
  float compute_volume_step_size() const;

  /* Check whether this object requires volume sampling (and hence might require space in the
   * volume stack).
   *
   * Note that this is a naive iteration over sharders, which allows to access information prior
   * to `scene_update()`. */
  bool check_is_volume() const;

 protected:
  /* Specifies the position of the object in scene->objects and
   * in the device vectors. Gets set in device_update. */
  int index;

  /* Reference to the attribute map with object attributes,
   * or 0 if none. Set in update_svm_attributes. */
  size_t attr_map_offset;

  friend class ObjectManager;
  friend class GeometryManager;
};

/* Object Manager */

class ObjectManager {
  uint32_t update_flags;

 public:
  enum : uint32_t {
    PARTICLE_MODIFIED = (1 << 0),
    GEOMETRY_MANAGER = (1 << 1),
    MOTION_BLUR_MODIFIED = (1 << 2),
    OBJECT_ADDED = (1 << 3),
    OBJECT_REMOVED = (1 << 4),
    OBJECT_MODIFIED = (1 << 5),
    HOLDOUT_MODIFIED = (1 << 6),
    TRANSFORM_MODIFIED = (1 << 7),
    VISIBILITY_MODIFIED = (1 << 8),

    /* tag everything in the manager for an update */
    UPDATE_ALL = ~0u,

    UPDATE_NONE = 0u,
  };

  bool need_flags_update;

  ObjectManager();
  ~ObjectManager();

  void device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress &progress);
  void device_update_transforms(DeviceScene *dscene, Scene *scene, Progress &progress);

  void device_update_flags(Device *device,
                           DeviceScene *dscene,
                           Scene *scene,
                           Progress &progress,
                           bool bounds_valid = true);
  void device_update_mesh_offsets(Device *device, DeviceScene *dscene, Scene *scene);

  void device_free(Device *device, DeviceScene *dscene, bool force_free);

  void tag_update(Scene *scene, uint32_t flag);

  bool need_update() const;

  void apply_static_transforms(DeviceScene *dscene, Scene *scene, Progress &progress);

  string get_cryptomatte_objects(Scene *scene);
  string get_cryptomatte_assets(Scene *scene);

 protected:
  void device_update_object_transform(UpdateObjectTransformState *state,
                                      Object *ob,
                                      bool update_all);
  void device_update_object_transform_task(UpdateObjectTransformState *state);
  bool device_update_object_transform_pop_work(UpdateObjectTransformState *state,
                                               int *start_index,
                                               int *num_objects);
};

CCL_NAMESPACE_END

#endif /* __OBJECT_H__ */
