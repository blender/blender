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
#include "render/scene.h"

#include "util/util_array.h"
#include "util/util_boundbox.h"
#include "util/util_param.h"
#include "util/util_transform.h"
#include "util/util_thread.h"
#include "util/util_types.h"
#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Mesh;
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

  Mesh *mesh;
  Transform tfm;
  BoundBox bounds;
  uint random_id;
  int pass_id;
  float3 color;
  ustring asset_name;
  vector<ParamValue> attributes;
  uint visibility;
  array<Transform> motion;
  bool hide_on_missing_motion;
  bool use_holdout;
  bool is_shadow_catcher;

  float3 dupli_generated;
  float2 dupli_uv;

  ParticleSystem *particle_system;
  int particle_index;

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

 protected:
  /* Specifies the position of the object in scene->objects and
   * in the device vectors. Gets set in device_update. */
  int index;

  friend class ObjectManager;
};

/* Object Manager */

class ObjectManager {
 public:
  bool need_update;
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

  void device_free(Device *device, DeviceScene *dscene);

  void tag_update(Scene *scene);

  void apply_static_transforms(DeviceScene *dscene, Scene *scene, Progress &progress);

  string get_cryptomatte_objects(Scene *scene);
  string get_cryptomatte_assets(Scene *scene);

 protected:
  void device_update_object_transform(UpdateObjectTransformState *state, Object *ob);
  void device_update_object_transform_task(UpdateObjectTransformState *state);
  bool device_update_object_transform_pop_work(UpdateObjectTransformState *state,
                                               int *start_index,
                                               int *num_objects);
};

CCL_NAMESPACE_END

#endif /* __OBJECT_H__ */
