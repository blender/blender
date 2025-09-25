/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "blender/light_linking.h"
#include "blender/object_cull.h"
#include "blender/sync.h"
#include "blender/util.h"

#include "scene/camera.h"
#include "scene/integrator.h"
#include "scene/light.h"
#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/particles.h"
#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"

#include "util/hash.h"
#include "util/log.h"
#include "util/task.h"

#include "BKE_duplilist.hh"

CCL_NAMESPACE_BEGIN

/* Utilities */

bool BlenderSync::BKE_object_is_modified(BL::Object &b_ob)
{
  /* test if we can instance or if the object is modified */
  if (b_ob.type() == BL::Object::type_META) {
    /* Multi-user and dupli meta-balls are fused, can't instance. */
    return true;
  }
  if (ccl::BKE_object_is_modified(b_ob, b_scene, preview)) {
    /* modifiers */
    return true;
  }

  /* object level material links */
  for (BL::MaterialSlot &b_slot : b_ob.material_slots) {
    if (b_slot.link() == BL::MaterialSlot::link_OBJECT) {
      return true;
    }
  }

  return false;
}

bool BlenderSync::object_is_geometry(BObjectInfo &b_ob_info)
{
  BL::ID b_ob_data = b_ob_info.object_data;

  if (!b_ob_data) {
    return false;
  }

  const BL::Object::type_enum type = b_ob_info.iter_object.type();

  if (type == BL::Object::type_VOLUME || type == BL::Object::type_CURVES ||
      type == BL::Object::type_POINTCLOUD || type == BL::Object::type_LIGHT)
  {
    /* Will be exported as geometry. */
    return true;
  }

  return b_ob_data.is_a(&RNA_Mesh);
}

bool BlenderSync::object_can_have_geometry(BL::Object &b_ob)
{
  const BL::Object::type_enum type = b_ob.type();
  switch (type) {
    case BL::Object::type_MESH:
    case BL::Object::type_CURVE:
    case BL::Object::type_SURFACE:
    case BL::Object::type_META:
    case BL::Object::type_FONT:
    case BL::Object::type_CURVES:
    case BL::Object::type_POINTCLOUD:
    case BL::Object::type_VOLUME:
      return true;
    default:
      return false;
  }
}

bool BlenderSync::object_is_light(BL::Object &b_ob)
{
  BL::ID b_ob_data = object_get_data(b_ob, true);

  return (b_ob_data && b_ob_data.is_a(&RNA_Light));
}

bool BlenderSync::object_is_camera(BL::Object &b_ob)
{
  BL::ID b_ob_data = object_get_data(b_ob, true);

  return (b_ob_data && b_ob_data.is_a(&RNA_Camera));
}

void BlenderSync::sync_object_motion_init(BL::Object &b_parent, BL::Object &b_ob, Object *object)
{
  /* Initialize motion blur for object, detecting if it's enabled and creating motion
   * steps array if so. */
  array<Transform> motion;
  object->set_motion(motion);

  Geometry *geom = object->get_geometry();
  if (!geom) {
    return;
  }

  int motion_steps = 0;
  bool use_motion_blur = false;

  const Scene::MotionType need_motion = scene->need_motion();
  if (need_motion == Scene::MOTION_BLUR) {
    motion_steps = object_motion_steps(b_parent, b_ob, Object::MAX_MOTION_STEPS);
    if (motion_steps && object_use_deform_motion(b_parent, b_ob)) {
      use_motion_blur = true;
    }
  }
  else if (need_motion != Scene::MOTION_NONE) {
    motion_steps = 3;
  }

  geom->set_use_motion_blur(use_motion_blur);

  motion.resize(motion_steps, transform_empty());

  if (motion_steps) {
    motion[motion_steps / 2] = object->get_tfm();

    /* update motion socket before trying to access object->motion_time */
    object->set_motion(motion);

    for (size_t step = 0; step < motion_steps; step++) {
      motion_times.insert(object->motion_time(step));
    }
  }
}

Object *BlenderSync::sync_object(BL::ViewLayer &b_view_layer,
                                 BL::DepsgraphObjectInstance &b_instance,
                                 const float motion_time,
                                 bool use_particle_hair,
                                 bool show_lights,
                                 BlenderObjectCulling &culling,
                                 TaskPool *geom_task_pool)
{
  const bool is_instance = b_instance.is_instance();
  BL::Object b_ob = b_instance.object();
  BL::Object b_parent = is_instance ? b_instance.parent() : b_instance.object();
  BL::Object b_real_object = is_instance ? b_instance.instance_object() : b_ob;
  const bool use_adaptive_subdiv = object_subdivision_type(
                                       b_real_object, preview, use_adaptive_subdivision) !=
                                   Mesh::SUBDIVISION_NONE;
  BObjectInfo b_ob_info{
      b_ob, b_real_object, object_get_data(b_ob, use_adaptive_subdiv), use_adaptive_subdiv};
  const bool motion = motion_time != 0.0f;
  /*const*/ Transform tfm = get_transform(b_ob.matrix_world());
  int *persistent_id = nullptr;
  BL::Array<int, OBJECT_PERSISTENT_ID_SIZE> persistent_id_array;
  if (is_instance) {
    persistent_id_array = b_instance.persistent_id();
    persistent_id = persistent_id_array.data;
    if (!motion && !b_ob_info.is_real_object_data()) {
      /* Remember which object data the geometry is coming from, so that we can sync it when the
       * object has changed. */
      instance_geometries_by_object[b_ob_info.real_object.ptr.data].insert(b_ob_info.object_data);
    }
  }

  /* only interested in object that we can create geometry from */
  if (!object_is_geometry(b_ob_info)) {
    return nullptr;
  }

  /* Perform object culling. */
  if (object_is_light(b_ob)) {
    if (!show_lights) {
      return nullptr;
    }
  }
  else if (culling.test(scene, b_ob, tfm)) {
    return nullptr;
  }

  /* Visibility flags for both parent and child. */
  PointerRNA cobject = RNA_pointer_get(&b_ob.ptr, "cycles");
  const bool use_holdout = b_parent.holdout_get(PointerRNA_NULL, b_view_layer);
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
  const bool use_indirect_only = !use_holdout &&
                                 b_parent.indirect_only_get(PointerRNA_NULL, b_view_layer);
  if (use_indirect_only) {
    visibility &= ~PATH_RAY_CAMERA;
  }

  /* Don't export completely invisible objects. */
  if (visibility == 0) {
    return nullptr;
  }

  /* Use task pool only for non-instances, since sync_dupli_particle accesses
   * geometry. This restriction should be removed for better performance. */
  TaskPool *object_geom_task_pool = (is_instance) ? nullptr : geom_task_pool;

  /* key to lookup object */
  const ObjectKey key(b_parent, persistent_id, b_ob_info.real_object, use_particle_hair);
  Object *object;

  /* motion vector case */
  if (motion) {
    object = object_map.find(key);

    if (object && object->use_motion()) {
      /* Set transform at matching motion time step. */
      const int time_index = object->motion_step(motion_time);
      if (time_index >= 0) {
        array<Transform> motion = object->get_motion();
        motion[time_index] = tfm;
        object->set_motion(motion);
      }

      /* mesh deformation */
      if (object->get_geometry()) {
        sync_geometry_motion(
            b_ob_info, object, motion_time, use_particle_hair, object_geom_task_pool);
      }
    }

    return object;
  }

  /* test if we need to sync */
  bool object_updated = object_map.add_or_update(&object, b_ob, b_parent, key) ||
                        (tfm != object->get_tfm());

  /* mesh sync */
  Geometry *geometry = sync_geometry(
      b_ob_info, object_updated, use_particle_hair, object_geom_task_pool);
  object->set_geometry(geometry);

  /* special case not tracked by object update flags */

  if (sync_object_attributes(b_instance, object)) {
    object_updated = true;
  }

  /* holdout */
  object->set_use_holdout(use_holdout);

  object->set_visibility(visibility);

  object->set_is_shadow_catcher(b_ob.is_shadow_catcher() || b_parent.is_shadow_catcher());

  object->set_shadow_terminator_shading_offset(b_ob.shadow_terminator_shading_offset());

  object->set_shadow_terminator_geometry_offset(b_ob.shadow_terminator_geometry_offset());

  float ao_distance = get_float(cobject, "ao_distance");
  if (ao_distance == 0.0f && b_parent.ptr.data != b_ob.ptr.data) {
    PointerRNA cparent = RNA_pointer_get(&b_parent.ptr, "cycles");
    ao_distance = get_float(cparent, "ao_distance");
  }
  object->set_ao_distance(ao_distance);

  const bool is_caustics_caster = get_boolean(cobject, "is_caustics_caster");
  object->set_is_caustics_caster(is_caustics_caster);

  const bool is_caustics_receiver = get_boolean(cobject, "is_caustics_receiver");
  object->set_is_caustics_receiver(is_caustics_receiver);

  object->set_is_bake_target(b_ob_info.real_object == b_bake_target);

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
  object->set_asset_name(parent_name);

  /* object sync
   * transform comparison should not be needed, but duplis don't work perfect
   * in the depsgraph and may not signal changes, so this is a workaround */
  const bool do_sync = object->is_modified() || object_updated ||
                       (object->get_geometry() && object->get_geometry()->is_modified());
  if (do_sync) {
    object->name = b_ob.name().c_str();
    object->set_pass_id(b_ob.pass_index());
    const BL::Array<float, 4> object_color = b_ob.color();
    object->set_color(get_float3(object_color));
    object->set_alpha(object_color[3]);
    object->set_tfm(tfm);

    /* dupli texture coordinates and random_id */
    if (is_instance) {
      object->set_dupli_generated(0.5f * get_float3(b_instance.orco()) -
                                  make_float3(0.5f, 0.5f, 0.5f));
      object->set_dupli_uv(get_float2(b_instance.uv()));
      object->set_random_id(b_instance.random_id());
    }
    else {
      object->set_dupli_generated(zero_float3());
      object->set_dupli_uv(zero_float2());
      object->set_random_id(hash_uint2(hash_string(object->name.c_str()), 0));
    }

    /* Light group and linking. */
    string lightgroup = b_ob.lightgroup();
    if (lightgroup.empty()) {
      lightgroup = b_parent.lightgroup();
    }
    object->set_lightgroup(ustring(lightgroup));

    object->set_light_set_membership(BlenderLightLink::get_light_set_membership(b_parent, b_ob));
    object->set_receiver_light_set(BlenderLightLink::get_receiver_light_set(b_parent, b_ob));
    object->set_shadow_set_membership(BlenderLightLink::get_shadow_set_membership(b_parent, b_ob));
    object->set_blocker_shadow_set(BlenderLightLink::get_blocker_shadow_set(b_parent, b_ob));
  }

  sync_object_motion_init(b_parent, b_ob, object);

  if (do_sync || object->motion_is_modified()) {
    object->tag_update(scene);
  }

  if (is_instance) {
    /* Sync possible particle data. */
    sync_dupli_particle(b_parent, b_instance, object);
  }

  return object;
}

extern "C" DupliObject *rna_hack_DepsgraphObjectInstance_dupli_object_get(PointerRNA *ptr);

static float4 lookup_instance_property(BL::DepsgraphObjectInstance &b_instance,
                                       const string &name,
                                       bool use_instancer)
{
  ::Object *ob = (::Object *)b_instance.object().ptr.data;
  ::DupliObject *dupli = nullptr;
  ::Object *dupli_parent = nullptr;

  /* If requesting instance data, check the parent particle system and object. */
  if (use_instancer && b_instance.is_instance()) {
    dupli = rna_hack_DepsgraphObjectInstance_dupli_object_get(&b_instance.ptr);
    dupli_parent = (::Object *)b_instance.parent().ptr.data;
  }

  float4 value;
  BKE_object_dupli_find_rgba_attribute(ob, dupli, dupli_parent, name.c_str(), &value.x);

  return value;
}

bool BlenderSync::sync_object_attributes(BL::DepsgraphObjectInstance &b_instance, Object *object)
{
  /* Find which attributes are needed. */
  AttributeRequestSet requests = object->get_geometry()->needed_attributes();

  /* Delete attributes that became unnecessary. */
  vector<ParamValue> &attributes = object->attributes;
  bool changed = false;

  for (int i = attributes.size() - 1; i >= 0; i--) {
    if (!requests.find(attributes[i].name())) {
      attributes.erase(attributes.begin() + i);
      changed = true;
    }
  }

  /* Update attribute values. */
  for (const AttributeRequest &req : requests.requests) {
    const ustring name = req.name;

    std::string real_name;
    const BlenderAttributeType type = blender_attribute_name_split_type(name, &real_name);

    if (type == BL::ShaderNodeAttribute::attribute_type_OBJECT ||
        type == BL::ShaderNodeAttribute::attribute_type_INSTANCER)
    {
      const bool use_instancer = (type == BL::ShaderNodeAttribute::attribute_type_INSTANCER);
      float4 value = lookup_instance_property(b_instance, real_name, use_instancer);

      /* Try finding the existing attribute value. */
      ParamValue *param = nullptr;

      for (size_t i = 0; i < attributes.size(); i++) {
        if (attributes[i].name() == name) {
          param = &attributes[i];
          break;
        }
      }

      /* Replace or add the value. */
      const ParamValue new_param(name, TypeFloat4, 1, &value);
      assert(new_param.datasize() == sizeof(value));

      if (!param) {
        changed = true;
        attributes.push_back(new_param);
      }
      else if (!(param->get<float4>() == value)) {
        changed = true;
        *param = new_param;
      }
    }
  }

  return changed;
}

/* Object Loop */

void BlenderSync::sync_objects(BL::Depsgraph &b_depsgraph,
                               BL::SpaceView3D &b_v3d,
                               const float motion_time)
{
  /* Task pool for multithreaded geometry sync. */
  TaskPool geom_task_pool;

  /* layer data */
  const bool motion = motion_time != 0.0f;

  if (!motion) {
    /* prepare for sync */
    geometry_map.pre_sync();
    object_map.pre_sync();
    procedural_map.pre_sync();
    particle_system_map.pre_sync();
    motion_times.clear();
  }
  else {
    geometry_motion_synced.clear();
  }

  if (!motion) {
    /* Object to geometry instance mapping is built for the reference time, as other
     * times just look up the corresponding geometry. */
    instance_geometries_by_object.clear();
  }

  /* initialize culling */
  BlenderObjectCulling culling(scene, b_scene);

  /* object loop */
  bool cancel = false;
  const bool show_lights = BlenderViewportParameters(b_v3d, use_developer_ui).use_scene_lights;

  BL::ViewLayer b_view_layer = b_depsgraph.view_layer_eval();
  BL::Depsgraph::object_instances_iterator b_instance_iter;

  for (b_depsgraph.object_instances.begin(b_instance_iter);
       b_instance_iter != b_depsgraph.object_instances.end() && !cancel;
       ++b_instance_iter)
  {
    BL::DepsgraphObjectInstance b_instance = *b_instance_iter;
    BL::Object b_ob = b_instance.object();

    /* Viewport visibility. */
    const bool show_in_viewport = !b_v3d || b_ob.visible_in_viewport_get(b_v3d);
    if (show_in_viewport == false) {
      continue;
    }

    /* Load per-object culling data. */
    culling.init_object(scene, b_ob);

    /* Ensure the object geom supporting the hair is processed before adding
     * the hair processing task to the task pool, calling .to_mesh() on the
     * same object in parallel does not work. */
    const bool sync_hair = b_instance.show_particles() && object_has_particle_hair(b_ob);

    /* Object itself. */
    if (b_instance.show_self()) {
      sync_object(b_view_layer,
                  b_instance,
                  motion_time,
                  false,
                  show_lights,
                  culling,
                  sync_hair ? nullptr : &geom_task_pool);
    }

    /* Particle hair as separate object. */
    if (sync_hair) {
      sync_object(
          b_view_layer, b_instance, motion_time, true, show_lights, culling, &geom_task_pool);
    }

    cancel = progress.get_cancel();
  }

  geom_task_pool.wait_work();

  progress.set_sync_status("");

  if (!cancel && !motion) {
    /* After object for world_use_portal. */
    sync_background_light(b_v3d);

    /* Handle removed data and modified pointers, as this may free memory, delete Nodes in the
     * right order to ensure that dependent data is freed after their users. Objects should be
     * freed before particle systems and geometries. */
    object_map.post_sync();
    geometry_map.post_sync();
    particle_system_map.post_sync();
    procedural_map.post_sync();
  }

  if (motion) {
    geometry_motion_synced.clear();
  }
}

void BlenderSync::sync_motion(BL::RenderSettings &b_render,
                              BL::Depsgraph &b_depsgraph,
                              BL::SpaceView3D &b_v3d,
                              BL::RegionView3D &b_rv3d,
                              const int width,
                              const int height,
                              void **python_thread_state)
{
  if (scene->need_motion() == Scene::MOTION_NONE) {
    return;
  }

  /* get camera object here to deal with camera switch */
  BL::Object b_cam = get_camera_object(b_v3d, b_rv3d);

  const int frame_center = b_scene.frame_current();
  const float subframe_center = b_scene.frame_subframe();
  float frame_center_delta = 0.0f;

  if (scene->need_motion() != Scene::MOTION_PASS &&
      scene->camera->get_motion_position() != MOTION_POSITION_CENTER)
  {
    const float shuttertime = scene->camera->get_shuttertime();
    if (scene->camera->get_motion_position() == MOTION_POSITION_END) {
      frame_center_delta = -shuttertime * 0.5f;
    }
    else {
      assert(scene->camera->get_motion_position() == MOTION_POSITION_START);
      frame_center_delta = shuttertime * 0.5f;
    }

    const float time = frame_center + subframe_center + frame_center_delta;
    const int frame = (int)floorf(time);
    const float subframe = time - frame;
    python_thread_state_restore(python_thread_state);
    b_engine.frame_set(frame, subframe);
    python_thread_state_save(python_thread_state);
    if (b_cam) {
      sync_camera_motion(b_render, b_cam, width, height, 0.0f);
    }
    sync_objects(b_depsgraph, b_v3d);
  }

  /* Insert motion times from camera. Motion times from other objects
   * have already been added in a sync_objects call. */
  if (b_cam) {
    const uint camera_motion_steps = object_motion_steps(b_cam, b_cam);
    for (size_t step = 0; step < camera_motion_steps; step++) {
      motion_times.insert(scene->camera->motion_time(step));
    }
  }

  /* Check which geometry already has motion blur so it can be skipped. */
  geometry_motion_attribute_synced.clear();
  for (Geometry *geom : scene->geometry) {
    if (geom->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION)) {
      geometry_motion_attribute_synced.insert(geom);
    }
  }

  /* note iteration over motion_times set happens in sorted order */
  for (const float relative_time : motion_times) {
    /* center time is already handled. */
    if (relative_time == 0.0f) {
      continue;
    }

    LOG_DEBUG << "Synchronizing motion for the relative time " << relative_time << ".";

    /* fixed shutter time to get previous and next frame for motion pass */
    const float shuttertime = scene->motion_shutter_time();

    /* compute frame and subframe time */
    const float time = frame_center + subframe_center + frame_center_delta +
                       relative_time * shuttertime * 0.5f;
    const int frame = (int)floorf(time);
    const float subframe = time - frame;

    /* change frame */
    python_thread_state_restore(python_thread_state);
    b_engine.frame_set(frame, subframe);
    python_thread_state_save(python_thread_state);

    /* Syncs camera motion if relative_time is one of the camera's motion times. */
    sync_camera_motion(b_render, b_cam, width, height, relative_time);

    /* sync object */
    sync_objects(b_depsgraph, b_v3d, relative_time);
  }

  geometry_motion_attribute_synced.clear();

  /* we need to set the python thread state again because this
   * function assumes it is being executed from python and will
   * try to save the thread state */
  python_thread_state_restore(python_thread_state);
  b_engine.frame_set(frame_center, subframe_center);
  python_thread_state_save(python_thread_state);
}

CCL_NAMESPACE_END
