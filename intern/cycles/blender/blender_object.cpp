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

#include "render/camera.h"
#include "render/graph.h"
#include "render/integrator.h"
#include "render/light.h"
#include "render/mesh.h"
#include "render/nodes.h"
#include "render/object.h"
#include "render/particles.h"
#include "render/scene.h"
#include "render/shader.h"

#include "blender/blender_object_cull.h"
#include "blender/blender_sync.h"
#include "blender/blender_util.h"

#include "util/util_foreach.h"
#include "util/util_hash.h"
#include "util/util_logging.h"

CCL_NAMESPACE_BEGIN

/* Utilities */

bool BlenderSync::BKE_object_is_modified(BL::Object &b_ob)
{
  /* test if we can instance or if the object is modified */
  if (b_ob.type() == BL::Object::type_META) {
    /* multi-user and dupli metaballs are fused, can't instance */
    return true;
  }
  else if (ccl::BKE_object_is_modified(b_ob, b_scene, preview)) {
    /* modifiers */
    return true;
  }
  else {
    /* object level material links */
    BL::Object::material_slots_iterator slot;
    for (b_ob.material_slots.begin(slot); slot != b_ob.material_slots.end(); ++slot)
      if (slot->link() == BL::MaterialSlot::link_OBJECT)
        return true;
  }

  return false;
}

bool BlenderSync::object_is_mesh(BL::Object &b_ob)
{
  BL::ID b_ob_data = b_ob.data();

  if (!b_ob_data) {
    return false;
  }

  BL::Object::type_enum type = b_ob.type();

  if (type == BL::Object::type_VOLUME || type == BL::Object::type_HAIR) {
    /* Will be exported attached to mesh. */
    return true;
  }
  else if (type == BL::Object::type_CURVE) {
    /* Skip exporting curves without faces, overhead can be
     * significant if there are many for path animation. */
    BL::Curve b_curve(b_ob_data);

    return (b_curve.bevel_object() || b_curve.extrude() != 0.0f || b_curve.bevel_depth() != 0.0f ||
            b_curve.dimensions() == BL::Curve::dimensions_2D || b_ob.modifiers.length());
  }
  else {
    return (b_ob_data.is_a(&RNA_Mesh) || b_ob_data.is_a(&RNA_Curve) ||
            b_ob_data.is_a(&RNA_MetaBall));
  }
}

bool BlenderSync::object_is_light(BL::Object &b_ob)
{
  BL::ID b_ob_data = b_ob.data();

  return (b_ob_data && b_ob_data.is_a(&RNA_Light));
}

/* Object */

Object *BlenderSync::sync_object(BL::Depsgraph &b_depsgraph,
                                 BL::ViewLayer &b_view_layer,
                                 BL::DepsgraphObjectInstance &b_instance,
                                 float motion_time,
                                 bool use_particle_hair,
                                 bool show_lights,
                                 BlenderObjectCulling &culling,
                                 bool *use_portal)
{
  const bool is_instance = b_instance.is_instance();
  BL::Object b_ob = b_instance.object();
  BL::Object b_parent = is_instance ? b_instance.parent() : b_instance.object();
  BL::Object b_ob_instance = is_instance ? b_instance.instance_object() : b_ob;
  const bool motion = motion_time != 0.0f;
  /*const*/ Transform tfm = get_transform(b_ob.matrix_world());
  int *persistent_id = NULL;
  BL::Array<int, OBJECT_PERSISTENT_ID_SIZE> persistent_id_array;
  if (is_instance) {
    persistent_id_array = b_instance.persistent_id();
    persistent_id = persistent_id_array.data;
  }

  /* light is handled separately */
  if (!motion && object_is_light(b_ob)) {
    if (!show_lights) {
      return NULL;
    }

    /* TODO: don't use lights for excluded layers used as mask layer,
     * when dynamic overrides are back. */
#if 0
    if (!((layer_flag & view_layer.holdout_layer) && (layer_flag & view_layer.exclude_layer)))
#endif
    {
      sync_light(b_parent,
                 persistent_id,
                 b_ob,
                 b_ob_instance,
                 is_instance ? b_instance.random_id() : 0,
                 tfm,
                 use_portal);
    }

    return NULL;
  }

  /* only interested in object that we can create meshes from */
  if (!object_is_mesh(b_ob)) {
    return NULL;
  }

  /* Perform object culling. */
  if (culling.test(scene, b_ob, tfm)) {
    return NULL;
  }

  /* Visibility flags for both parent and child. */
  PointerRNA cobject = RNA_pointer_get(&b_ob.ptr, "cycles");
  bool use_holdout = get_boolean(cobject, "is_holdout") ||
                     b_parent.holdout_get(PointerRNA_NULL, b_view_layer);
  uint visibility = object_ray_visibility(b_ob) & PATH_RAY_ALL_VISIBILITY;

  if (b_parent.ptr.data != b_ob.ptr.data) {
    visibility &= object_ray_visibility(b_parent);
  }

  /* TODO: make holdout objects on excluded layer invisible for non-camera rays. */
#if 0
  if (use_holdout && (layer_flag & view_layer.exclude_layer)) {
    visibility &= ~(PATH_RAY_ALL_VISIBILITY - PATH_RAY_CAMERA);
  }
#endif

  /* Clear camera visibility for indirect only objects. */
  bool use_indirect_only = !use_holdout &&
                           b_parent.indirect_only_get(PointerRNA_NULL, b_view_layer);
  if (use_indirect_only) {
    visibility &= ~PATH_RAY_CAMERA;
  }

  /* Don't export completely invisible objects. */
  if (visibility == 0) {
    return NULL;
  }

  /* key to lookup object */
  ObjectKey key(b_parent, persistent_id, b_ob_instance, use_particle_hair);
  Object *object;

  /* motion vector case */
  if (motion) {
    object = object_map.find(key);

    if (object && object->use_motion()) {
      /* Set transform at matching motion time step. */
      int time_index = object->motion_step(motion_time);
      if (time_index >= 0) {
        object->motion[time_index] = tfm;
      }

      /* mesh deformation */
      if (object->geometry)
        sync_geometry_motion(b_depsgraph, b_ob, object, motion_time, use_particle_hair);
    }

    return object;
  }

  /* test if we need to sync */
  bool object_updated = false;

  if (object_map.add_or_update(&object, b_ob, b_parent, key))
    object_updated = true;

  /* mesh sync */
  object->geometry = sync_geometry(
      b_depsgraph, b_ob, b_ob_instance, object_updated, use_particle_hair);

  /* special case not tracked by object update flags */

  /* holdout */
  if (use_holdout != object->use_holdout) {
    object->use_holdout = use_holdout;
    scene->object_manager->tag_update(scene);
    object_updated = true;
  }

  if (visibility != object->visibility) {
    object->visibility = visibility;
    object_updated = true;
  }

  bool is_shadow_catcher = get_boolean(cobject, "is_shadow_catcher");
  if (is_shadow_catcher != object->is_shadow_catcher) {
    object->is_shadow_catcher = is_shadow_catcher;
    object_updated = true;
  }

  float shadow_terminator_offset = get_float(cobject, "shadow_terminator_offset");
  if (shadow_terminator_offset != object->shadow_terminator_offset) {
    object->shadow_terminator_offset = shadow_terminator_offset;
    object_updated = true;
  }

  /* sync the asset name for Cryptomatte */
  BL::Object parent = b_ob.parent();
  ustring parent_name;
  if (parent) {
    while (parent.parent()) {
      parent = parent.parent();
    }
    parent_name = parent.name();
  }
  else {
    parent_name = b_ob.name();
  }
  if (object->asset_name != parent_name) {
    object->asset_name = parent_name;
    object_updated = true;
  }

  /* object sync
   * transform comparison should not be needed, but duplis don't work perfect
   * in the depsgraph and may not signal changes, so this is a workaround */
  if (object_updated || (object->geometry && object->geometry->need_update) ||
      tfm != object->tfm) {
    object->name = b_ob.name().c_str();
    object->pass_id = b_ob.pass_index();
    object->color = get_float3(b_ob.color());
    object->tfm = tfm;
    object->motion.clear();

    /* motion blur */
    Scene::MotionType need_motion = scene->need_motion();
    if (need_motion != Scene::MOTION_NONE && object->geometry) {
      Geometry *geom = object->geometry;
      geom->use_motion_blur = false;
      geom->motion_steps = 0;

      uint motion_steps;

      if (need_motion == Scene::MOTION_BLUR) {
        motion_steps = object_motion_steps(b_parent, b_ob, Object::MAX_MOTION_STEPS);
        geom->motion_steps = motion_steps;
        if (motion_steps && object_use_deform_motion(b_parent, b_ob)) {
          geom->use_motion_blur = true;
        }
      }
      else {
        motion_steps = 3;
        geom->motion_steps = motion_steps;
      }

      object->motion.clear();
      object->motion.resize(motion_steps, transform_empty());

      if (motion_steps) {
        object->motion[motion_steps / 2] = tfm;

        for (size_t step = 0; step < motion_steps; step++) {
          motion_times.insert(object->motion_time(step));
        }
      }
    }

    /* dupli texture coordinates and random_id */
    if (is_instance) {
      object->dupli_generated = 0.5f * get_float3(b_instance.orco()) -
                                make_float3(0.5f, 0.5f, 0.5f);
      object->dupli_uv = get_float2(b_instance.uv());
      object->random_id = b_instance.random_id();
    }
    else {
      object->dupli_generated = make_float3(0.0f, 0.0f, 0.0f);
      object->dupli_uv = make_float2(0.0f, 0.0f);
      object->random_id = hash_uint2(hash_string(object->name.c_str()), 0);
    }

    object->tag_update(scene);
  }

  if (is_instance) {
    /* Sync possible particle data. */
    sync_dupli_particle(b_parent, b_instance, object);
  }

  return object;
}

/* Object Loop */

void BlenderSync::sync_objects(BL::Depsgraph &b_depsgraph,
                               BL::SpaceView3D &b_v3d,
                               float motion_time)
{
  /* layer data */
  bool motion = motion_time != 0.0f;

  if (!motion) {
    /* prepare for sync */
    light_map.pre_sync();
    geometry_map.pre_sync();
    object_map.pre_sync();
    particle_system_map.pre_sync();
    motion_times.clear();
  }
  else {
    geometry_motion_synced.clear();
  }

  /* initialize culling */
  BlenderObjectCulling culling(scene, b_scene);

  /* object loop */
  bool cancel = false;
  bool use_portal = false;
  const bool show_lights = BlenderViewportParameters(b_v3d).use_scene_lights;

  BL::ViewLayer b_view_layer = b_depsgraph.view_layer_eval();

  BL::Depsgraph::object_instances_iterator b_instance_iter;
  for (b_depsgraph.object_instances.begin(b_instance_iter);
       b_instance_iter != b_depsgraph.object_instances.end() && !cancel;
       ++b_instance_iter) {
    BL::DepsgraphObjectInstance b_instance = *b_instance_iter;
    BL::Object b_ob = b_instance.object();

    /* Viewport visibility. */
    const bool show_in_viewport = !b_v3d || b_ob.visible_in_viewport_get(b_v3d);
    if (show_in_viewport == false) {
      continue;
    }

    /* Load per-object culling data. */
    culling.init_object(scene, b_ob);

    /* Object itself. */
    if (b_instance.show_self()) {
      sync_object(b_depsgraph,
                  b_view_layer,
                  b_instance,
                  motion_time,
                  false,
                  show_lights,
                  culling,
                  &use_portal);
    }

    /* Particle hair as separate object. */
    if (b_instance.show_particles() && object_has_particle_hair(b_ob)) {
      sync_object(b_depsgraph,
                  b_view_layer,
                  b_instance,
                  motion_time,
                  true,
                  show_lights,
                  culling,
                  &use_portal);
    }

    cancel = progress.get_cancel();
  }

  progress.set_sync_status("");

  if (!cancel && !motion) {
    sync_background_light(b_v3d, use_portal);

    /* handle removed data and modified pointers */
    if (light_map.post_sync())
      scene->light_manager->tag_update(scene);
    if (geometry_map.post_sync())
      scene->geometry_manager->tag_update(scene);
    if (object_map.post_sync())
      scene->object_manager->tag_update(scene);
    if (particle_system_map.post_sync())
      scene->particle_system_manager->tag_update(scene);
  }

  if (motion)
    geometry_motion_synced.clear();
}

void BlenderSync::sync_motion(BL::RenderSettings &b_render,
                              BL::Depsgraph &b_depsgraph,
                              BL::SpaceView3D &b_v3d,
                              BL::Object &b_override,
                              int width,
                              int height,
                              void **python_thread_state)
{
  if (scene->need_motion() == Scene::MOTION_NONE)
    return;

  /* get camera object here to deal with camera switch */
  BL::Object b_cam = b_scene.camera();
  if (b_override)
    b_cam = b_override;

  Camera prevcam = *(scene->camera);

  int frame_center = b_scene.frame_current();
  float subframe_center = b_scene.frame_subframe();
  float frame_center_delta = 0.0f;

  if (scene->need_motion() != Scene::MOTION_PASS &&
      scene->camera->motion_position != Camera::MOTION_POSITION_CENTER) {
    float shuttertime = scene->camera->shuttertime;
    if (scene->camera->motion_position == Camera::MOTION_POSITION_END) {
      frame_center_delta = -shuttertime * 0.5f;
    }
    else {
      assert(scene->camera->motion_position == Camera::MOTION_POSITION_START);
      frame_center_delta = shuttertime * 0.5f;
    }

    float time = frame_center + subframe_center + frame_center_delta;
    int frame = (int)floorf(time);
    float subframe = time - frame;
    python_thread_state_restore(python_thread_state);
    b_engine.frame_set(frame, subframe);
    python_thread_state_save(python_thread_state);
    sync_camera_motion(b_render, b_cam, width, height, 0.0f);
    sync_objects(b_depsgraph, b_v3d, 0.0f);
  }

  /* Insert motion times from camera. Motion times from other objects
   * have already been added in a sync_objects call. */
  uint camera_motion_steps = object_motion_steps(b_cam, b_cam);
  for (size_t step = 0; step < camera_motion_steps; step++) {
    motion_times.insert(scene->camera->motion_time(step));
  }

  /* note iteration over motion_times set happens in sorted order */
  foreach (float relative_time, motion_times) {
    /* center time is already handled. */
    if (relative_time == 0.0f) {
      continue;
    }

    VLOG(1) << "Synchronizing motion for the relative time " << relative_time << ".";

    /* fixed shutter time to get previous and next frame for motion pass */
    float shuttertime = scene->motion_shutter_time();

    /* compute frame and subframe time */
    float time = frame_center + subframe_center + frame_center_delta +
                 relative_time * shuttertime * 0.5f;
    int frame = (int)floorf(time);
    float subframe = time - frame;

    /* change frame */
    python_thread_state_restore(python_thread_state);
    b_engine.frame_set(frame, subframe);
    python_thread_state_save(python_thread_state);

    /* Syncs camera motion if relative_time is one of the camera's motion times. */
    sync_camera_motion(b_render, b_cam, width, height, relative_time);

    /* sync object */
    sync_objects(b_depsgraph, b_v3d, relative_time);
  }

  /* we need to set the python thread state again because this
   * function assumes it is being executed from python and will
   * try to save the thread state */
  python_thread_state_restore(python_thread_state);
  b_engine.frame_set(frame_center, subframe_center);
  python_thread_state_save(python_thread_state);

  /* tag camera for motion update */
  if (scene->camera->motion_modified(prevcam))
    scene->camera->tag_update();
}

CCL_NAMESPACE_END
