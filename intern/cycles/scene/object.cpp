/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "scene/object.h"
#include "device/device.h"
#include "scene/camera.h"
#include "scene/curves.h"
#include "scene/hair.h"
#include "scene/integrator.h"
#include "scene/light.h"
#include "scene/mesh.h"
#include "scene/particles.h"
#include "scene/pointcloud.h"
#include "scene/scene.h"
#include "scene/stats.h"
#include "scene/volume.h"

#include "util/foreach.h"
#include "util/log.h"
#include "util/map.h"
#include "util/murmurhash.h"
#include "util/progress.h"
#include "util/set.h"
#include "util/task.h"
#include "util/vector.h"

#include "subd/patch_table.h"

CCL_NAMESPACE_BEGIN

/* Global state of object transform update. */

struct UpdateObjectTransformState {
  /* Global state used by device_update_object_transform().
   * Common for both threaded and non-threaded update.
   */

  /* Type of the motion required by the scene settings. */
  Scene::MotionType need_motion;

  /* Mapping from particle system to a index in packed particle array.
   * Only used for read.
   */
  map<ParticleSystem *, int> particle_offset;

  /* Motion offsets for each object. */
  array<uint> motion_offset;

  /* Packed object arrays. Those will be filled in. */
  uint *object_flag;
  uint *object_visibility;
  KernelObject *objects;
  Transform *object_motion_pass;
  DecomposedTransform *object_motion;
  float *object_volume_step;

  /* Flags which will be synchronized to Integrator. */
  bool have_motion;
  bool have_curves;
  bool have_points;
  bool have_volumes;

  /* ** Scheduling queue. ** */
  Scene *scene;

  /* First unused object index in the queue. */
  int queue_start_object;
};

/* Object */

NODE_DEFINE(Object)
{
  NodeType *type = NodeType::add("object", create);

  SOCKET_NODE(geometry, "Geometry", Geometry::get_node_base_type());
  SOCKET_TRANSFORM(tfm, "Transform", transform_identity());
  SOCKET_UINT(visibility, "Visibility", ~0);
  SOCKET_COLOR(color, "Color", zero_float3());
  SOCKET_FLOAT(alpha, "Alpha", 0.0f);
  SOCKET_UINT(random_id, "Random ID", 0);
  SOCKET_INT(pass_id, "Pass ID", 0);
  SOCKET_BOOLEAN(use_holdout, "Use Holdout", false);
  SOCKET_BOOLEAN(hide_on_missing_motion, "Hide on Missing Motion", false);
  SOCKET_POINT(dupli_generated, "Dupli Generated", zero_float3());
  SOCKET_POINT2(dupli_uv, "Dupli UV", zero_float2());
  SOCKET_TRANSFORM_ARRAY(motion, "Motion", array<Transform>());
  SOCKET_FLOAT(shadow_terminator_shading_offset, "Shadow Terminator Shading Offset", 0.0f);
  SOCKET_FLOAT(shadow_terminator_geometry_offset, "Shadow Terminator Geometry Offset", 0.1f);
  SOCKET_STRING(asset_name, "Asset Name", ustring());

  SOCKET_BOOLEAN(is_shadow_catcher, "Shadow Catcher", false);

  SOCKET_BOOLEAN(is_caustics_caster, "Cast Shadow Caustics", false);
  SOCKET_BOOLEAN(is_caustics_receiver, "Receive Shadow Caustics", false);

  SOCKET_NODE(particle_system, "Particle System", ParticleSystem::get_node_type());
  SOCKET_INT(particle_index, "Particle Index", 0);

  SOCKET_FLOAT(ao_distance, "AO Distance", 0.0f);

  SOCKET_STRING(lightgroup, "Light Group", ustring());

  return type;
}

Object::Object() : Node(get_node_type())
{
  particle_system = NULL;
  particle_index = 0;
  attr_map_offset = 0;
  bounds = BoundBox::empty;
  intersects_volume = false;
}

Object::~Object()
{
}

void Object::update_motion()
{
  if (!use_motion()) {
    return;
  }

  bool have_motion = false;

  for (size_t i = 0; i < motion.size(); i++) {
    if (motion[i] == transform_empty()) {
      if (hide_on_missing_motion) {
        /* Hide objects that have no valid previous or next
         * transform, for example particle that stop existing. It
         * would be better to handle this in the kernel and make
         * objects invisible outside certain motion steps. */
        tfm = transform_empty();
        motion.clear();
        return;
      }
      else {
        /* Otherwise just copy center motion. */
        motion[i] = tfm;
      }
    }

    /* Test if any of the transforms are actually different. */
    have_motion = have_motion || motion[i] != tfm;
  }

  /* Clear motion array if there is no actual motion. */
  if (!have_motion) {
    motion.clear();
  }
}

void Object::compute_bounds(bool motion_blur)
{
  BoundBox mbounds = geometry->bounds;

  if (motion_blur && use_motion()) {
    array<DecomposedTransform> decomp(motion.size());
    transform_motion_decompose(decomp.data(), motion.data(), motion.size());

    bounds = BoundBox::empty;

    /* TODO: this is really terrible. according to PBRT there is a better
     * way to find this iteratively, but did not find implementation yet
     * or try to implement myself */
    for (float t = 0.0f; t < 1.0f; t += (1.0f / 128.0f)) {
      Transform ttfm;

      transform_motion_array_interpolate(&ttfm, decomp.data(), motion.size(), t);
      bounds.grow(mbounds.transformed(&ttfm));
    }
  }
  else {
    /* No motion blur case. */
    if (geometry->transform_applied) {
      bounds = mbounds;
    }
    else {
      bounds = mbounds.transformed(&tfm);
    }
  }
}

void Object::apply_transform(bool apply_to_motion)
{
  if (!geometry || tfm == transform_identity())
    return;

  geometry->apply_transform(tfm, apply_to_motion);

  /* we keep normals pointing in same direction on negative scale, notify
   * geometry about this in it (re)calculates normals */
  if (transform_negative_scale(tfm))
    geometry->transform_negative_scaled = true;

  if (bounds.valid()) {
    geometry->compute_bounds();
    compute_bounds(false);
  }

  /* tfm is not reset to identity, all code that uses it needs to check the
   * transform_applied boolean */
}

void Object::tag_update(Scene *scene)
{
  uint32_t flag = ObjectManager::UPDATE_NONE;

  if (is_modified()) {
    flag |= ObjectManager::OBJECT_MODIFIED;

    if (use_holdout_is_modified()) {
      flag |= ObjectManager::HOLDOUT_MODIFIED;
    }

    if (is_shadow_catcher_is_modified()) {
      scene->tag_shadow_catcher_modified();
      flag |= ObjectManager::VISIBILITY_MODIFIED;
    }
  }

  if (geometry) {
    if (tfm_is_modified() || motion_is_modified()) {
      flag |= ObjectManager::TRANSFORM_MODIFIED;
    }

    if (visibility_is_modified()) {
      flag |= ObjectManager::VISIBILITY_MODIFIED;
    }

    foreach (Node *node, geometry->get_used_shaders()) {
      Shader *shader = static_cast<Shader *>(node);
      if (shader->emission_sampling != EMISSION_SAMPLING_NONE)
        scene->light_manager->tag_update(scene, LightManager::EMISSIVE_MESH_MODIFIED);
    }
  }

  scene->camera->need_flags_update = true;
  scene->object_manager->tag_update(scene, flag);
}

bool Object::use_motion() const
{
  return (motion.size() > 1);
}

float Object::motion_time(int step) const
{
  return (use_motion()) ? 2.0f * step / (motion.size() - 1) - 1.0f : 0.0f;
}

int Object::motion_step(float time) const
{
  if (use_motion()) {
    for (size_t step = 0; step < motion.size(); step++) {
      if (time == motion_time(step)) {
        return step;
      }
    }
  }

  return -1;
}

bool Object::is_traceable() const
{
  /* Mesh itself can be empty,can skip all such objects. */
  if (!bounds.valid() || bounds.size() == zero_float3()) {
    return false;
  }
  /* TODO(sergey): Check for mesh vertices/curves. visibility flags. */
  return true;
}

uint Object::visibility_for_tracing() const
{
  return SHADOW_CATCHER_OBJECT_VISIBILITY(is_shadow_catcher, visibility & PATH_RAY_ALL_VISIBILITY);
}

float Object::compute_volume_step_size() const
{
  if (geometry->geometry_type != Geometry::MESH && geometry->geometry_type != Geometry::VOLUME) {
    return FLT_MAX;
  }

  Mesh *mesh = static_cast<Mesh *>(geometry);

  if (!mesh->has_volume) {
    return FLT_MAX;
  }

  /* Compute step rate from shaders. */
  float step_rate = FLT_MAX;

  foreach (Node *node, mesh->get_used_shaders()) {
    Shader *shader = static_cast<Shader *>(node);
    if (shader->has_volume) {
      if ((shader->get_heterogeneous_volume() && shader->has_volume_spatial_varying) ||
          (shader->has_volume_attribute_dependency)) {
        step_rate = fminf(shader->get_volume_step_rate(), step_rate);
      }
    }
  }

  if (step_rate == FLT_MAX) {
    return FLT_MAX;
  }

  /* Compute step size from voxel grids. */
  float step_size = FLT_MAX;

  if (geometry->geometry_type == Geometry::VOLUME) {
    Volume *volume = static_cast<Volume *>(geometry);

    foreach (Attribute &attr, volume->attributes.attributes) {
      if (attr.element == ATTR_ELEMENT_VOXEL) {
        ImageHandle &handle = attr.data_voxel();
        const ImageMetaData &metadata = handle.metadata();
        if (metadata.width == 0 || metadata.height == 0 || metadata.depth == 0) {
          continue;
        }

        /* User specified step size. */
        float voxel_step_size = volume->get_step_size();

        if (voxel_step_size == 0.0f) {
          /* Auto detect step size. */
          float3 size = one_float3();
#ifdef WITH_NANOVDB
          /* Dimensions were not applied to image transform with NanoVDB (see image_vdb.cpp) */
          if (metadata.type != IMAGE_DATA_TYPE_NANOVDB_FLOAT &&
              metadata.type != IMAGE_DATA_TYPE_NANOVDB_FLOAT3 &&
              metadata.type != IMAGE_DATA_TYPE_NANOVDB_FPN &&
              metadata.type != IMAGE_DATA_TYPE_NANOVDB_FP16)
#endif
            size /= make_float3(metadata.width, metadata.height, metadata.depth);

          /* Step size is transformed from voxel to world space. */
          Transform voxel_tfm = tfm;
          if (metadata.use_transform_3d) {
            voxel_tfm = tfm * transform_inverse(metadata.transform_3d);
          }
          voxel_step_size = reduce_min(fabs(transform_direction(&voxel_tfm, size)));
        }
        else if (volume->get_object_space()) {
          /* User specified step size in object space. */
          float3 size = make_float3(voxel_step_size, voxel_step_size, voxel_step_size);
          voxel_step_size = reduce_min(fabs(transform_direction(&tfm, size)));
        }

        if (voxel_step_size > 0.0f) {
          step_size = fminf(voxel_step_size, step_size);
        }
      }
    }
  }

  if (step_size == FLT_MAX) {
    /* Fall back to 1/10th of bounds for procedural volumes. */
    step_size = 0.1f * average(bounds.size());
  }

  step_size *= step_rate;

  return step_size;
}

int Object::get_device_index() const
{
  return index;
}

/* Object Manager */

ObjectManager::ObjectManager()
{
  update_flags = UPDATE_ALL;
  need_flags_update = true;
}

ObjectManager::~ObjectManager()
{
}

static float object_volume_density(const Transform &tfm, Geometry *geom)
{
  if (geom->geometry_type == Geometry::VOLUME) {
    /* Volume density automatically adjust to object scale. */
    if (static_cast<Volume *>(geom)->get_object_space()) {
      const float3 unit = normalize(one_float3());
      return 1.0f / len(transform_direction(&tfm, unit));
    }
  }

  return 1.0f;
}

void ObjectManager::device_update_object_transform(UpdateObjectTransformState *state,
                                                   Object *ob,
                                                   bool update_all,
                                                   const Scene *scene)
{
  KernelObject &kobject = state->objects[ob->index];
  Transform *object_motion_pass = state->object_motion_pass;

  Geometry *geom = ob->geometry;
  uint flag = 0;

  /* Compute transformations. */
  Transform tfm = ob->tfm;
  Transform itfm = transform_inverse(tfm);

  float3 color = ob->color;
  float pass_id = ob->pass_id;
  float random_number = (float)ob->random_id * (1.0f / (float)0xFFFFFFFF);
  int particle_index = (ob->particle_system) ?
                           ob->particle_index + state->particle_offset[ob->particle_system] :
                           0;

  kobject.tfm = tfm;
  kobject.itfm = itfm;
  kobject.volume_density = object_volume_density(tfm, geom);
  kobject.color[0] = color.x;
  kobject.color[1] = color.y;
  kobject.color[2] = color.z;
  kobject.alpha = ob->alpha;
  kobject.pass_id = pass_id;
  kobject.random_number = random_number;
  kobject.particle_index = particle_index;
  kobject.motion_offset = 0;
  kobject.ao_distance = ob->ao_distance;

  if (geom->get_use_motion_blur()) {
    state->have_motion = true;
  }

  if (transform_negative_scale(tfm)) {
    flag |= SD_OBJECT_NEGATIVE_SCALE;
  }

  if (geom->geometry_type == Geometry::MESH || geom->geometry_type == Geometry::POINTCLOUD) {
    /* TODO: why only mesh? */
    Mesh *mesh = static_cast<Mesh *>(geom);
    if (mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION)) {
      flag |= SD_OBJECT_HAS_VERTEX_MOTION;
    }
  }
  else if (geom->is_volume()) {
    Volume *volume = static_cast<Volume *>(geom);
    if (volume->attributes.find(ATTR_STD_VOLUME_VELOCITY) &&
        volume->get_velocity_scale() != 0.0f) {
      flag |= SD_OBJECT_HAS_VOLUME_MOTION;
      kobject.velocity_scale = volume->get_velocity_scale();
    }
  }

  if (state->need_motion == Scene::MOTION_PASS) {
    /* Clear motion array if there is no actual motion. */
    ob->update_motion();

    /* Compute motion transforms. */
    Transform tfm_pre, tfm_post;
    if (ob->use_motion()) {
      tfm_pre = ob->motion[0];
      tfm_post = ob->motion[ob->motion.size() - 1];
    }
    else {
      tfm_pre = tfm;
      tfm_post = tfm;
    }

    /* Motion transformations, is world/object space depending if mesh
     * comes with deformed position in object space, or if we transform
     * the shading point in world space. */
    if (!(flag & SD_OBJECT_HAS_VERTEX_MOTION)) {
      tfm_pre = tfm_pre * itfm;
      tfm_post = tfm_post * itfm;
    }

    int motion_pass_offset = ob->index * OBJECT_MOTION_PASS_SIZE;
    object_motion_pass[motion_pass_offset + 0] = tfm_pre;
    object_motion_pass[motion_pass_offset + 1] = tfm_post;
  }
  else if (state->need_motion == Scene::MOTION_BLUR) {
    if (ob->use_motion()) {
      kobject.motion_offset = state->motion_offset[ob->index];

      /* Decompose transforms for interpolation. */
      if (ob->tfm_is_modified() || ob->motion_is_modified() || update_all) {
        DecomposedTransform *decomp = state->object_motion + kobject.motion_offset;
        transform_motion_decompose(decomp, ob->motion.data(), ob->motion.size());
      }

      flag |= SD_OBJECT_MOTION;
      state->have_motion = true;
    }
  }

  /* Dupli object coords and motion info. */
  kobject.dupli_generated[0] = ob->dupli_generated[0];
  kobject.dupli_generated[1] = ob->dupli_generated[1];
  kobject.dupli_generated[2] = ob->dupli_generated[2];
  kobject.numkeys = (geom->geometry_type == Geometry::HAIR) ?
                        static_cast<Hair *>(geom)->get_curve_keys().size() :
                    (geom->geometry_type == Geometry::POINTCLOUD) ?
                        static_cast<PointCloud *>(geom)->num_points() :
                        0;
  kobject.dupli_uv[0] = ob->dupli_uv[0];
  kobject.dupli_uv[1] = ob->dupli_uv[1];
  int totalsteps = geom->get_motion_steps();
  kobject.numsteps = (totalsteps - 1) / 2;
  kobject.numverts = (geom->geometry_type == Geometry::MESH ||
                      geom->geometry_type == Geometry::VOLUME) ?
                         static_cast<Mesh *>(geom)->get_verts().size() :
                         0;
  kobject.patch_map_offset = 0;
  kobject.attribute_map_offset = 0;

  if (ob->asset_name_is_modified() || update_all) {
    uint32_t hash_name = util_murmur_hash3(ob->name.c_str(), ob->name.length(), 0);
    uint32_t hash_asset = util_murmur_hash3(ob->asset_name.c_str(), ob->asset_name.length(), 0);
    kobject.cryptomatte_object = util_hash_to_float(hash_name);
    kobject.cryptomatte_asset = util_hash_to_float(hash_asset);
  }

  kobject.shadow_terminator_shading_offset = 1.0f /
                                             (1.0f - 0.5f * ob->shadow_terminator_shading_offset);
  kobject.shadow_terminator_geometry_offset = ob->shadow_terminator_geometry_offset;

  kobject.visibility = ob->visibility_for_tracing();
  kobject.primitive_type = geom->primitive_type();

  /* Object shadow caustics flag */
  if (ob->is_caustics_caster) {
    flag |= SD_OBJECT_CAUSTICS_CASTER;
  }
  if (ob->is_caustics_receiver) {
    flag |= SD_OBJECT_CAUSTICS_RECEIVER;
  }

  /* Object flag. */
  if (ob->use_holdout) {
    flag |= SD_OBJECT_HOLDOUT_MASK;
  }
  state->object_flag[ob->index] = flag;
  state->object_volume_step[ob->index] = FLT_MAX;

  /* Have curves. */
  if (geom->geometry_type == Geometry::HAIR) {
    state->have_curves = true;
  }
  if (geom->geometry_type == Geometry::POINTCLOUD) {
    state->have_points = true;
  }
  if (geom->geometry_type == Geometry::VOLUME) {
    state->have_volumes = true;
  }

  /* Light group. */
  auto it = scene->lightgroups.find(ob->lightgroup);
  if (it != scene->lightgroups.end()) {
    kobject.lightgroup = it->second;
  }
  else {
    kobject.lightgroup = LIGHTGROUP_NONE;
  }
}

void ObjectManager::device_update_prim_offsets(Device *device, DeviceScene *dscene, Scene *scene)
{
  if (!scene->integrator->get_use_light_tree()) {
    BVHLayoutMask layout_mask = device->get_bvh_layout_mask();
    if (layout_mask != BVH_LAYOUT_METAL && layout_mask != BVH_LAYOUT_MULTI_METAL &&
        layout_mask != BVH_LAYOUT_MULTI_METAL_EMBREE) {
      return;
    }
  }

  /* On MetalRT, primitive / curve segment offsets can't be baked at BVH build time. Intersection
   * handlers need to apply the offset manually. */
  uint *object_prim_offset = dscene->object_prim_offset.alloc(scene->objects.size());
  foreach (Object *ob, scene->objects) {
    uint32_t prim_offset = 0;
    if (Geometry *const geom = ob->geometry) {
      if (geom->geometry_type == Geometry::HAIR) {
        prim_offset = ((Hair *const)geom)->curve_segment_offset;
      }
      else {
        prim_offset = geom->prim_offset;
      }
    }
    uint obj_index = ob->get_device_index();
    object_prim_offset[obj_index] = prim_offset;
  }

  dscene->object_prim_offset.copy_to_device();
  dscene->object_prim_offset.clear_modified();
}

void ObjectManager::device_update_transforms(DeviceScene *dscene, Scene *scene, Progress &progress)
{
  UpdateObjectTransformState state;
  state.need_motion = scene->need_motion();
  state.have_motion = false;
  state.have_curves = false;
  state.have_points = false;
  state.have_volumes = false;
  state.scene = scene;
  state.queue_start_object = 0;

  state.objects = dscene->objects.alloc(scene->objects.size());
  state.object_flag = dscene->object_flag.alloc(scene->objects.size());
  state.object_volume_step = dscene->object_volume_step.alloc(scene->objects.size());
  state.object_motion = NULL;
  state.object_motion_pass = NULL;

  if (state.need_motion == Scene::MOTION_PASS) {
    state.object_motion_pass = dscene->object_motion_pass.alloc(OBJECT_MOTION_PASS_SIZE *
                                                                scene->objects.size());
  }
  else if (state.need_motion == Scene::MOTION_BLUR) {
    /* Set object offsets into global object motion array. */
    uint *motion_offsets = state.motion_offset.resize(scene->objects.size());
    uint motion_offset = 0;

    foreach (Object *ob, scene->objects) {
      *motion_offsets = motion_offset;
      motion_offsets++;

      /* Clear motion array if there is no actual motion. */
      ob->update_motion();
      motion_offset += ob->motion.size();
    }

    state.object_motion = dscene->object_motion.alloc(motion_offset);
  }

  /* Particle system device offsets
   * 0 is dummy particle, index starts at 1.
   */
  int numparticles = 1;
  foreach (ParticleSystem *psys, scene->particle_systems) {
    state.particle_offset[psys] = numparticles;
    numparticles += psys->particles.size();
  }

  /* as all the arrays are the same size, checking only dscene.objects is sufficient */
  const bool update_all = dscene->objects.need_realloc();

  /* Parallel object update, with grain size to avoid too much threading overhead
   * for individual objects. */
  static const int OBJECTS_PER_TASK = 32;
  parallel_for(blocked_range<size_t>(0, scene->objects.size(), OBJECTS_PER_TASK),
               [&](const blocked_range<size_t> &r) {
                 for (size_t i = r.begin(); i != r.end(); i++) {
                   Object *ob = state.scene->objects[i];
                   device_update_object_transform(&state, ob, update_all, scene);
                 }
               });

  if (progress.get_cancel()) {
    return;
  }

  dscene->objects.copy_to_device_if_modified();
  if (state.need_motion == Scene::MOTION_PASS) {
    dscene->object_motion_pass.copy_to_device();
  }
  else if (state.need_motion == Scene::MOTION_BLUR) {
    dscene->object_motion.copy_to_device();
  }

  dscene->data.bvh.have_motion = state.have_motion;
  dscene->data.bvh.have_curves = state.have_curves;
  dscene->data.bvh.have_points = state.have_points;
  dscene->data.bvh.have_volumes = state.have_volumes;

  dscene->objects.clear_modified();
  dscene->object_motion_pass.clear_modified();
  dscene->object_motion.clear_modified();
}

void ObjectManager::device_update(Device *device,
                                  DeviceScene *dscene,
                                  Scene *scene,
                                  Progress &progress)
{
  if (!need_update())
    return;

  if (update_flags & (OBJECT_ADDED | OBJECT_REMOVED)) {
    dscene->objects.tag_realloc();
    dscene->object_motion_pass.tag_realloc();
    dscene->object_motion.tag_realloc();
    dscene->object_flag.tag_realloc();
    dscene->object_volume_step.tag_realloc();
  }

  if (update_flags & HOLDOUT_MODIFIED) {
    dscene->object_flag.tag_modified();
  }

  if (update_flags & PARTICLE_MODIFIED) {
    dscene->objects.tag_modified();
  }

  VLOG_INFO << "Total " << scene->objects.size() << " objects.";

  device_free(device, dscene, false);

  if (scene->objects.size() == 0)
    return;

  {
    /* Assign object IDs. */
    scoped_callback_timer timer([scene](double time) {
      if (scene->update_stats) {
        scene->update_stats->object.times.add_entry({"device_update (assign index)", time});
      }
    });

    int index = 0;
    foreach (Object *object, scene->objects) {
      object->index = index++;

      /* this is a bit too broad, however a bigger refactor might be needed to properly separate
       * update each type of data (transform, flags, etc.) */
      if (object->is_modified()) {
        dscene->objects.tag_modified();
        dscene->object_motion_pass.tag_modified();
        dscene->object_motion.tag_modified();
        dscene->object_flag.tag_modified();
        dscene->object_volume_step.tag_modified();
      }
    }
  }

  {
    /* set object transform matrices, before applying static transforms */
    scoped_callback_timer timer([scene](double time) {
      if (scene->update_stats) {
        scene->update_stats->object.times.add_entry(
            {"device_update (copy objects to device)", time});
      }
    });

    progress.set_status("Updating Objects", "Copying Transformations to device");
    device_update_transforms(dscene, scene, progress);
  }

  if (progress.get_cancel())
    return;

  /* prepare for static BVH building */
  /* todo: do before to support getting object level coords? */
  if (scene->params.bvh_type == BVH_TYPE_STATIC) {
    scoped_callback_timer timer([scene](double time) {
      if (scene->update_stats) {
        scene->update_stats->object.times.add_entry(
            {"device_update (apply static transforms)", time});
      }
    });

    progress.set_status("Updating Objects", "Applying Static Transformations");
    apply_static_transforms(dscene, scene, progress);
  }

  foreach (Object *object, scene->objects) {
    object->clear_modified();
  }
}

void ObjectManager::device_update_flags(
    Device *, DeviceScene *dscene, Scene *scene, Progress & /*progress*/, bool bounds_valid)
{
  if (!need_update() && !need_flags_update)
    return;

  scoped_callback_timer timer([scene](double time) {
    if (scene->update_stats) {
      scene->update_stats->object.times.add_entry({"device_update_flags", time});
    }
  });

  update_flags = UPDATE_NONE;
  need_flags_update = false;

  if (scene->objects.size() == 0)
    return;

  /* Object info flag. */
  uint *object_flag = dscene->object_flag.data();
  float *object_volume_step = dscene->object_volume_step.data();

  /* Object volume intersection. */
  vector<Object *> volume_objects;
  bool has_volume_objects = false;
  foreach (Object *object, scene->objects) {
    if (object->geometry->has_volume) {
      if (bounds_valid) {
        volume_objects.push_back(object);
      }
      has_volume_objects = true;
      object_volume_step[object->index] = object->compute_volume_step_size();
    }
    else {
      object_volume_step[object->index] = FLT_MAX;
    }
  }

  foreach (Object *object, scene->objects) {
    if (object->geometry->has_volume) {
      object_flag[object->index] |= SD_OBJECT_HAS_VOLUME;
      object_flag[object->index] &= ~SD_OBJECT_HAS_VOLUME_ATTRIBUTES;

      foreach (Attribute &attr, object->geometry->attributes.attributes) {
        if (attr.element == ATTR_ELEMENT_VOXEL) {
          object_flag[object->index] |= SD_OBJECT_HAS_VOLUME_ATTRIBUTES;
        }
      }
    }
    else {
      object_flag[object->index] &= ~(SD_OBJECT_HAS_VOLUME | SD_OBJECT_HAS_VOLUME_ATTRIBUTES);
    }

    if (object->is_shadow_catcher) {
      object_flag[object->index] |= SD_OBJECT_SHADOW_CATCHER;
    }
    else {
      object_flag[object->index] &= ~SD_OBJECT_SHADOW_CATCHER;
    }

    if (bounds_valid) {
      object->intersects_volume = false;
      foreach (Object *volume_object, volume_objects) {
        if (object == volume_object) {
          continue;
        }
        if (object->bounds.intersects(volume_object->bounds)) {
          object_flag[object->index] |= SD_OBJECT_INTERSECTS_VOLUME;
          object->intersects_volume = true;
          break;
        }
      }
    }
    else if (has_volume_objects) {
      /* Not really valid, but can't make more reliable in the case
       * of bounds not being up to date.
       */
      object_flag[object->index] |= SD_OBJECT_INTERSECTS_VOLUME;
    }
  }

  /* Copy object flag. */
  dscene->object_flag.copy_to_device();
  dscene->object_volume_step.copy_to_device();

  dscene->object_flag.clear_modified();
  dscene->object_volume_step.clear_modified();
}

void ObjectManager::device_update_geom_offsets(Device *, DeviceScene *dscene, Scene *scene)
{
  if (dscene->objects.size() == 0) {
    return;
  }

  KernelObject *kobjects = dscene->objects.data();

  bool update = false;

  foreach (Object *object, scene->objects) {
    Geometry *geom = object->geometry;

    if (geom->geometry_type == Geometry::MESH) {
      Mesh *mesh = static_cast<Mesh *>(geom);
      if (mesh->patch_table) {
        uint patch_map_offset = 2 * (mesh->patch_table_offset + mesh->patch_table->total_size() -
                                     mesh->patch_table->num_nodes * PATCH_NODE_SIZE) -
                                mesh->patch_offset;

        if (kobjects[object->index].patch_map_offset != patch_map_offset) {
          kobjects[object->index].patch_map_offset = patch_map_offset;
          update = true;
        }
      }
    }

    size_t attr_map_offset = object->attr_map_offset;

    /* An object attribute map cannot have a zero offset because mesh maps come first. */
    if (attr_map_offset == 0) {
      attr_map_offset = geom->attr_map_offset;
    }

    if (kobjects[object->index].attribute_map_offset != attr_map_offset) {
      kobjects[object->index].attribute_map_offset = attr_map_offset;
      update = true;
    }
  }

  if (update) {
    dscene->objects.copy_to_device();
  }
}

void ObjectManager::device_free(Device *, DeviceScene *dscene, bool force_free)
{
  dscene->objects.free_if_need_realloc(force_free);
  dscene->object_motion_pass.free_if_need_realloc(force_free);
  dscene->object_motion.free_if_need_realloc(force_free);
  dscene->object_flag.free_if_need_realloc(force_free);
  dscene->object_volume_step.free_if_need_realloc(force_free);
  dscene->object_prim_offset.free_if_need_realloc(force_free);
}

void ObjectManager::apply_static_transforms(DeviceScene *dscene, Scene *scene, Progress &progress)
{
  /* todo: normals and displacement should be done before applying transform! */
  /* todo: create objects/geometry in right order! */

  /* counter geometry users */
  map<Geometry *, int> geometry_users;
  Scene::MotionType need_motion = scene->need_motion();
  bool motion_blur = need_motion == Scene::MOTION_BLUR;
  bool apply_to_motion = need_motion != Scene::MOTION_PASS;
  int i = 0;

  foreach (Object *object, scene->objects) {
    map<Geometry *, int>::iterator it = geometry_users.find(object->geometry);

    if (it == geometry_users.end())
      geometry_users[object->geometry] = 1;
    else
      it->second++;
  }

  if (progress.get_cancel())
    return;

  uint *object_flag = dscene->object_flag.data();

  /* apply transforms for objects with single user geometry */
  foreach (Object *object, scene->objects) {
    /* Annoying feedback loop here: we can't use is_instanced() because
     * it'll use uninitialized transform_applied flag.
     *
     * Could be solved by moving reference counter to Geometry.
     */
    Geometry *geom = object->geometry;
    bool apply = (geometry_users[geom] == 1) && !geom->has_surface_bssrdf &&
                 !geom->has_true_displacement();

    if (geom->geometry_type == Geometry::MESH) {
      Mesh *mesh = static_cast<Mesh *>(geom);
      apply = apply && mesh->get_subdivision_type() == Mesh::SUBDIVISION_NONE;
    }
    else if (geom->geometry_type == Geometry::HAIR) {
      /* Can't apply non-uniform scale to curves, this can't be represented by
       * control points and radius alone. */
      float scale;
      apply = apply && transform_uniform_scale(object->tfm, scale);
    }

    if (apply) {
      if (!(motion_blur && object->use_motion())) {
        if (!geom->transform_applied) {
          object->apply_transform(apply_to_motion);
          geom->transform_applied = true;

          if (progress.get_cancel())
            return;
        }

        object_flag[i] |= SD_OBJECT_TRANSFORM_APPLIED;
      }
    }

    i++;
  }
}

void ObjectManager::tag_update(Scene *scene, uint32_t flag)
{
  update_flags |= flag;

  /* avoid infinite loops if the geometry manager tagged us for an update */
  if ((flag & GEOMETRY_MANAGER) == 0) {
    uint32_t geometry_flag = GeometryManager::OBJECT_MANAGER;

    /* Also notify in case added or removed objects were instances, as no Geometry might have been
     * added or removed, but the BVH still needs to updated. */
    if ((flag & (OBJECT_ADDED | OBJECT_REMOVED)) != 0) {
      geometry_flag |= (GeometryManager::GEOMETRY_ADDED | GeometryManager::GEOMETRY_REMOVED);
    }

    if ((flag & TRANSFORM_MODIFIED) != 0) {
      geometry_flag |= GeometryManager::TRANSFORM_MODIFIED;
    }

    if ((flag & VISIBILITY_MODIFIED) != 0) {
      geometry_flag |= GeometryManager::VISIBILITY_MODIFIED;
    }

    scene->geometry_manager->tag_update(scene, geometry_flag);
  }

  scene->light_manager->tag_update(scene, LightManager::OBJECT_MANAGER);

  /* Integrator's shadow catcher settings depends on object visibility settings. */
  if (flag & (OBJECT_ADDED | OBJECT_REMOVED | OBJECT_MODIFIED)) {
    scene->integrator->tag_update(scene, Integrator::OBJECT_MANAGER);
  }
}

bool ObjectManager::need_update() const
{
  return update_flags != UPDATE_NONE;
}

string ObjectManager::get_cryptomatte_objects(Scene *scene)
{
  string manifest = "{";

  unordered_set<ustring, ustringHash> objects;
  foreach (Object *object, scene->objects) {
    if (objects.count(object->name)) {
      continue;
    }
    objects.insert(object->name);
    uint32_t hash_name = util_murmur_hash3(object->name.c_str(), object->name.length(), 0);
    manifest += string_printf("\"%s\":\"%08x\",", object->name.c_str(), hash_name);
  }
  manifest[manifest.size() - 1] = '}';
  return manifest;
}

string ObjectManager::get_cryptomatte_assets(Scene *scene)
{
  string manifest = "{";
  unordered_set<ustring, ustringHash> assets;
  foreach (Object *ob, scene->objects) {
    if (assets.count(ob->asset_name)) {
      continue;
    }
    assets.insert(ob->asset_name);
    uint32_t hash_asset = util_murmur_hash3(ob->asset_name.c_str(), ob->asset_name.length(), 0);
    manifest += string_printf("\"%s\":\"%08x\",", ob->asset_name.c_str(), hash_asset);
  }
  manifest[manifest.size() - 1] = '}';
  return manifest;
}

CCL_NAMESPACE_END
