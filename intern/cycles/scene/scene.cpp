/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include <stdlib.h>

#include "bvh/bvh.h"
#include "device/device.h"
#include "scene/alembic.h"
#include "scene/background.h"
#include "scene/bake.h"
#include "scene/camera.h"
#include "scene/curves.h"
#include "scene/film.h"
#include "scene/integrator.h"
#include "scene/light.h"
#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/osl.h"
#include "scene/particles.h"
#include "scene/pointcloud.h"
#include "scene/procedural.h"
#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/svm.h"
#include "scene/tables.h"
#include "scene/volume.h"
#include "session/session.h"

#include "util/foreach.h"
#include "util/guarded_allocator.h"
#include "util/log.h"
#include "util/progress.h"

CCL_NAMESPACE_BEGIN

DeviceScene::DeviceScene(Device *device)
    : bvh_nodes(device, "bvh_nodes", MEM_GLOBAL),
      bvh_leaf_nodes(device, "bvh_leaf_nodes", MEM_GLOBAL),
      object_node(device, "object_node", MEM_GLOBAL),
      prim_type(device, "prim_type", MEM_GLOBAL),
      prim_visibility(device, "prim_visibility", MEM_GLOBAL),
      prim_index(device, "prim_index", MEM_GLOBAL),
      prim_object(device, "prim_object", MEM_GLOBAL),
      prim_time(device, "prim_time", MEM_GLOBAL),
      tri_verts(device, "tri_verts", MEM_GLOBAL),
      tri_shader(device, "tri_shader", MEM_GLOBAL),
      tri_vnormal(device, "tri_vnormal", MEM_GLOBAL),
      tri_vindex(device, "tri_vindex", MEM_GLOBAL),
      tri_patch(device, "tri_patch", MEM_GLOBAL),
      tri_patch_uv(device, "tri_patch_uv", MEM_GLOBAL),
      curves(device, "curves", MEM_GLOBAL),
      curve_keys(device, "curve_keys", MEM_GLOBAL),
      curve_segments(device, "curve_segments", MEM_GLOBAL),
      patches(device, "patches", MEM_GLOBAL),
      points(device, "points", MEM_GLOBAL),
      points_shader(device, "points_shader", MEM_GLOBAL),
      objects(device, "objects", MEM_GLOBAL),
      object_motion_pass(device, "object_motion_pass", MEM_GLOBAL),
      object_motion(device, "object_motion", MEM_GLOBAL),
      object_flag(device, "object_flag", MEM_GLOBAL),
      object_volume_step(device, "object_volume_step", MEM_GLOBAL),
      object_prim_offset(device, "object_prim_offset", MEM_GLOBAL),
      camera_motion(device, "camera_motion", MEM_GLOBAL),
      attributes_map(device, "attributes_map", MEM_GLOBAL),
      attributes_float(device, "attributes_float", MEM_GLOBAL),
      attributes_float2(device, "attributes_float2", MEM_GLOBAL),
      attributes_float3(device, "attributes_float3", MEM_GLOBAL),
      attributes_float4(device, "attributes_float4", MEM_GLOBAL),
      attributes_uchar4(device, "attributes_uchar4", MEM_GLOBAL),
      light_distribution(device, "light_distribution", MEM_GLOBAL),
      lights(device, "lights", MEM_GLOBAL),
      light_background_marginal_cdf(device, "light_background_marginal_cdf", MEM_GLOBAL),
      light_background_conditional_cdf(device, "light_background_conditional_cdf", MEM_GLOBAL),
      light_tree_nodes(device, "light_tree_nodes", MEM_GLOBAL),
      light_tree_emitters(device, "light_tree_emitters", MEM_GLOBAL),
      light_to_tree(device, "light_to_tree", MEM_GLOBAL),
      object_lookup_offset(device, "object_lookup_offset", MEM_GLOBAL),
      triangle_to_tree(device, "triangle_to_tree", MEM_GLOBAL),
      particles(device, "particles", MEM_GLOBAL),
      svm_nodes(device, "svm_nodes", MEM_GLOBAL),
      shaders(device, "shaders", MEM_GLOBAL),
      lookup_table(device, "lookup_table", MEM_GLOBAL),
      sample_pattern_lut(device, "sample_pattern_lut", MEM_GLOBAL),
      ies_lights(device, "ies", MEM_GLOBAL)
{
  memset((void *)&data, 0, sizeof(data));
}

Scene::Scene(const SceneParams &params_, Device *device)
    : name("Scene"),
      bvh(NULL),
      default_surface(NULL),
      default_volume(NULL),
      default_light(NULL),
      default_background(NULL),
      default_empty(NULL),
      device(device),
      dscene(device),
      params(params_),
      update_stats(NULL),
      kernels_loaded(false),
      /* TODO(sergey): Check if it's indeed optimal value for the split kernel. */
      max_closure_global(1)
{
  memset((void *)&dscene.data, 0, sizeof(dscene.data));

  shader_manager = ShaderManager::create(
      device->info.has_osl ? params.shadingsystem : SHADINGSYSTEM_SVM, device);

  light_manager = new LightManager();
  geometry_manager = new GeometryManager();
  object_manager = new ObjectManager();
  image_manager = new ImageManager(device->info);
  particle_system_manager = new ParticleSystemManager();
  bake_manager = new BakeManager();
  procedural_manager = new ProceduralManager();

  /* Create nodes after managers, since create_node() can tag the managers. */
  camera = create_node<Camera>();
  dicing_camera = create_node<Camera>();
  lookup_tables = new LookupTables();
  film = create_node<Film>();
  background = create_node<Background>();
  integrator = create_node<Integrator>();

  film->add_default(this);
  shader_manager->add_default(this);
}

Scene::~Scene()
{
  free_memory(true);
}

void Scene::free_memory(bool final)
{
  delete bvh;
  bvh = NULL;

  /* The order of deletion is important to make sure data is freed based on possible dependencies
   * as the Nodes' reference counts are decremented in the destructors:
   *
   * - Procedurals can create and hold pointers to any other types.
   * - Objects can hold pointers to Geometries and ParticleSystems
   * - Lights and Geometries can hold pointers to Shaders.
   *
   * Similarly, we first delete all nodes and their associated device data, and then the managers
   * and their associated device data.
   */
  foreach (Procedural *p, procedurals)
    delete p;
  foreach (Object *o, objects)
    delete o;
  foreach (Geometry *g, geometry)
    delete g;
  foreach (ParticleSystem *p, particle_systems)
    delete p;
  foreach (Light *l, lights)
    delete l;
  foreach (Pass *p, passes)
    delete p;

  geometry.clear();
  objects.clear();
  lights.clear();
  particle_systems.clear();
  procedurals.clear();
  passes.clear();

  if (device) {
    camera->device_free(device, &dscene, this);
    film->device_free(device, &dscene, this);
    background->device_free(device, &dscene);
    integrator->device_free(device, &dscene, true);
  }

  if (final) {
    delete camera;
    delete dicing_camera;
    delete film;
    delete background;
    delete integrator;
  }

  /* Delete Shaders after every other nodes to ensure that we do not try to decrement the reference
   * count on some dangling pointer. */
  foreach (Shader *s, shaders)
    delete s;

  shaders.clear();

  /* Now that all nodes have been deleted, we can safely delete managers and device data. */
  if (device) {
    object_manager->device_free(device, &dscene, true);
    geometry_manager->device_free(device, &dscene, true);
    shader_manager->device_free(device, &dscene, this);
    light_manager->device_free(device, &dscene);

    particle_system_manager->device_free(device, &dscene);

    bake_manager->device_free(device, &dscene);

    if (final)
      image_manager->device_free(device);
    else
      image_manager->device_free_builtin(device);

    lookup_tables->device_free(device, &dscene);
  }

  if (final) {
    delete lookup_tables;
    delete object_manager;
    delete geometry_manager;
    delete shader_manager;
    delete light_manager;
    delete particle_system_manager;
    delete image_manager;
    delete bake_manager;
    delete update_stats;
    delete procedural_manager;
  }
}

void Scene::device_update(Device *device_, Progress &progress)
{
  if (!device)
    device = device_;

  bool print_stats = need_data_update();

  if (update_stats) {
    update_stats->clear();
  }

  scoped_callback_timer timer([this, print_stats](double time) {
    if (update_stats) {
      update_stats->scene.times.add_entry({"device_update", time});

      if (print_stats) {
        printf("Update statistics:\n%s\n", update_stats->full_report().c_str());
      }
    }
  });

  /* The order of updates is important, because there's dependencies between
   * the different managers, using data computed by previous managers.
   *
   * - Image manager uploads images used by shaders.
   * - Camera may be used for adaptive subdivision.
   * - Displacement shader must have all shader data available.
   * - Light manager needs lookup tables and final mesh data to compute emission CDF.
   * - Lookup tables are done a second time to handle film tables
   */

  if (film->update_lightgroups(this)) {
    light_manager->tag_update(this, ccl::LightManager::LIGHT_MODIFIED);
    object_manager->tag_update(this, ccl::ObjectManager::OBJECT_MODIFIED);
  }
  if (film->exposure_is_modified()) {
    integrator->tag_modified();
  }

  progress.set_status("Updating Shaders");
  shader_manager->device_update(device, &dscene, this, progress);

  if (progress.get_cancel() || device->have_error())
    return;

  procedural_manager->update(this, progress);

  if (progress.get_cancel())
    return;

  progress.set_status("Updating Background");
  background->device_update(device, &dscene, this);

  if (progress.get_cancel() || device->have_error())
    return;

  progress.set_status("Updating Camera");
  camera->device_update(device, &dscene, this);

  if (progress.get_cancel() || device->have_error())
    return;

  geometry_manager->device_update_preprocess(device, this, progress);

  if (progress.get_cancel() || device->have_error())
    return;

  progress.set_status("Updating Objects");
  object_manager->device_update(device, &dscene, this, progress);

  if (progress.get_cancel() || device->have_error())
    return;

  progress.set_status("Updating Particle Systems");
  particle_system_manager->device_update(device, &dscene, this, progress);

  if (progress.get_cancel() || device->have_error())
    return;

  progress.set_status("Updating Meshes");
  geometry_manager->device_update(device, &dscene, this, progress);

  if (progress.get_cancel() || device->have_error())
    return;

  progress.set_status("Updating Objects Flags");
  object_manager->device_update_flags(device, &dscene, this, progress);

  if (progress.get_cancel() || device->have_error())
    return;

  progress.set_status("Updating Primitive Offsets");
  object_manager->device_update_prim_offsets(device, &dscene, this);

  if (progress.get_cancel() || device->have_error())
    return;

  progress.set_status("Updating Images");
  image_manager->device_update(device, this, progress);

  if (progress.get_cancel() || device->have_error())
    return;

  progress.set_status("Updating Camera Volume");
  camera->device_update_volume(device, &dscene, this);

  if (progress.get_cancel() || device->have_error())
    return;

  progress.set_status("Updating Lookup Tables");
  lookup_tables->device_update(device, &dscene, this);

  if (progress.get_cancel() || device->have_error())
    return;

  progress.set_status("Updating Lights");
  light_manager->device_update(device, &dscene, this, progress);

  if (progress.get_cancel() || device->have_error())
    return;

  progress.set_status("Updating Integrator");
  integrator->device_update(device, &dscene, this);

  if (progress.get_cancel() || device->have_error())
    return;

  progress.set_status("Updating Film");
  film->device_update(device, &dscene, this);

  if (progress.get_cancel() || device->have_error())
    return;

  progress.set_status("Updating Lookup Tables");
  lookup_tables->device_update(device, &dscene, this);

  if (progress.get_cancel() || device->have_error())
    return;

  progress.set_status("Updating Baking");
  bake_manager->device_update(device, &dscene, this, progress);

  if (progress.get_cancel() || device->have_error())
    return;

  if (device->have_error() == false) {
    dscene.data.volume_stack_size = get_volume_stack_size();

    progress.set_status("Updating Device", "Writing constant memory");
    device->const_copy_to("data", &dscene.data, sizeof(dscene.data));
  }

  device->optimize_for_scene(this);

  if (print_stats) {
    size_t mem_used = util_guarded_get_mem_used();
    size_t mem_peak = util_guarded_get_mem_peak();

    VLOG_INFO << "System memory statistics after full device sync:\n"
              << "  Usage: " << string_human_readable_number(mem_used) << " ("
              << string_human_readable_size(mem_used) << ")\n"
              << "  Peak: " << string_human_readable_number(mem_peak) << " ("
              << string_human_readable_size(mem_peak) << ")";
  }
}

Scene::MotionType Scene::need_motion() const
{
  if (integrator->get_motion_blur())
    return MOTION_BLUR;
  else if (Pass::contains(passes, PASS_MOTION))
    return MOTION_PASS;
  else
    return MOTION_NONE;
}

float Scene::motion_shutter_time()
{
  if (need_motion() == Scene::MOTION_PASS)
    return 2.0f;
  else
    return camera->get_shuttertime();
}

bool Scene::need_global_attribute(AttributeStandard std)
{
  if (std == ATTR_STD_UV)
    return Pass::contains(passes, PASS_UV);
  else if (std == ATTR_STD_MOTION_VERTEX_POSITION)
    return need_motion() != MOTION_NONE;
  else if (std == ATTR_STD_MOTION_VERTEX_NORMAL)
    return need_motion() == MOTION_BLUR;
  else if (std == ATTR_STD_VOLUME_VELOCITY || std == ATTR_STD_VOLUME_VELOCITY_X ||
           std == ATTR_STD_VOLUME_VELOCITY_Y || std == ATTR_STD_VOLUME_VELOCITY_Z) {
    return need_motion() != MOTION_NONE;
  }

  return false;
}

void Scene::need_global_attributes(AttributeRequestSet &attributes)
{
  for (int std = ATTR_STD_NONE; std < ATTR_STD_NUM; std++)
    if (need_global_attribute((AttributeStandard)std))
      attributes.add((AttributeStandard)std);
}

bool Scene::need_update()
{
  return (need_reset() || film->is_modified());
}

bool Scene::need_data_update()
{
  return (background->is_modified() || image_manager->need_update() ||
          object_manager->need_update() || geometry_manager->need_update() ||
          light_manager->need_update() || lookup_tables->need_update() ||
          integrator->is_modified() || shader_manager->need_update() ||
          particle_system_manager->need_update() || bake_manager->need_update() ||
          film->is_modified() || procedural_manager->need_update());
}

bool Scene::need_reset(const bool check_camera)
{
  return need_data_update() || (check_camera && camera->is_modified());
}

void Scene::reset()
{
  shader_manager->reset(this);
  shader_manager->add_default(this);

  /* ensure all objects are updated */
  camera->tag_modified();
  dicing_camera->tag_modified();
  film->tag_modified();
  background->tag_modified();

  background->tag_update(this);
  integrator->tag_update(this, Integrator::UPDATE_ALL);
  object_manager->tag_update(this, ObjectManager::UPDATE_ALL);
  geometry_manager->tag_update(this, GeometryManager::UPDATE_ALL);
  light_manager->tag_update(this, LightManager::UPDATE_ALL);
  particle_system_manager->tag_update(this);
  procedural_manager->tag_update();
}

void Scene::device_free()
{
  free_memory(false);
}

void Scene::collect_statistics(RenderStats *stats)
{
  geometry_manager->collect_statistics(this, stats);
  image_manager->collect_statistics(stats);
}

void Scene::enable_update_stats()
{
  if (!update_stats) {
    update_stats = new SceneUpdateStats();
  }
}

void Scene::update_kernel_features()
{
  if (!need_update()) {
    return;
  }

  thread_scoped_lock scene_lock(mutex);

  /* These features are not being tweaked as often as shaders,
   * so could be done selective magic for the viewport as well. */
  uint kernel_features = shader_manager->get_kernel_features(this);

  bool use_motion = need_motion() == Scene::MotionType::MOTION_BLUR;
  kernel_features |= KERNEL_FEATURE_PATH_TRACING;
  if (params.hair_shape == CURVE_THICK) {
    kernel_features |= KERNEL_FEATURE_HAIR_THICK;
  }

  /* Figure out whether the scene will use shader ray-trace we need at least
   * one caustic light, one caustic caster and one caustic receiver to use
   * and enable the MNEE code path. */
  bool has_caustics_receiver = false;
  bool has_caustics_caster = false;
  bool has_caustics_light = false;

  foreach (Object *object, objects) {
    if (object->get_is_caustics_caster()) {
      has_caustics_caster = true;
    }
    else if (object->get_is_caustics_receiver()) {
      has_caustics_receiver = true;
    }
    Geometry *geom = object->get_geometry();
    if (use_motion) {
      if (object->use_motion() || geom->get_use_motion_blur()) {
        kernel_features |= KERNEL_FEATURE_OBJECT_MOTION;
      }
    }
    if (object->get_is_shadow_catcher()) {
      kernel_features |= KERNEL_FEATURE_SHADOW_CATCHER;
    }
    if (geom->is_mesh()) {
#ifdef WITH_OPENSUBDIV
      Mesh *mesh = static_cast<Mesh *>(geom);
      if (mesh->get_subdivision_type() != Mesh::SUBDIVISION_NONE) {
        kernel_features |= KERNEL_FEATURE_PATCH_EVALUATION;
      }
#endif
    }
    else if (geom->is_hair()) {
      kernel_features |= KERNEL_FEATURE_HAIR;
    }
    else if (geom->is_pointcloud()) {
      kernel_features |= KERNEL_FEATURE_POINTCLOUD;
    }
  }

  foreach (Light *light, lights) {
    if (light->get_use_caustics()) {
      has_caustics_light = true;
    }
  }

  dscene.data.integrator.use_caustics = false;
  if (has_caustics_caster && has_caustics_receiver && has_caustics_light) {
    dscene.data.integrator.use_caustics = true;
    kernel_features |= KERNEL_FEATURE_MNEE;
  }

  if (integrator->get_guiding_params(device).use) {
    kernel_features |= KERNEL_FEATURE_PATH_GUIDING;
  }

  if (bake_manager->get_baking()) {
    kernel_features |= KERNEL_FEATURE_BAKING;
  }

  kernel_features |= film->get_kernel_features(this);
  kernel_features |= integrator->get_kernel_features();

  dscene.data.kernel_features = kernel_features;

  /* Currently viewport render is faster with higher max_closures, needs investigating. */
  const uint max_closures = (params.background) ? get_max_closure_count() : MAX_CLOSURE;
  dscene.data.max_closures = max_closures;
  dscene.data.max_shaders = shaders.size();
}

bool Scene::update(Progress &progress)
{
  if (!need_update()) {
    return false;
  }

  /* Upload scene data to the GPU. */
  progress.set_status("Updating Scene");
  MEM_GUARDED_CALL(&progress, device_update, device, progress);

  return true;
}

static void log_kernel_features(const uint features)
{
  VLOG_INFO << "Requested features:\n";
  VLOG_INFO << "Use BSDF " << string_from_bool(features & KERNEL_FEATURE_NODE_BSDF) << "\n";
  VLOG_INFO << "Use Emission " << string_from_bool(features & KERNEL_FEATURE_NODE_EMISSION)
            << "\n";
  VLOG_INFO << "Use Volume " << string_from_bool(features & KERNEL_FEATURE_NODE_VOLUME) << "\n";
  VLOG_INFO << "Use Bump " << string_from_bool(features & KERNEL_FEATURE_NODE_BUMP) << "\n";
  VLOG_INFO << "Use Voronoi " << string_from_bool(features & KERNEL_FEATURE_NODE_VORONOI_EXTRA)
            << "\n";
  VLOG_INFO << "Use Shader Raytrace " << string_from_bool(features & KERNEL_FEATURE_NODE_RAYTRACE)
            << "\n";
  VLOG_INFO << "Use MNEE" << string_from_bool(features & KERNEL_FEATURE_MNEE) << "\n";
  VLOG_INFO << "Use Transparent " << string_from_bool(features & KERNEL_FEATURE_TRANSPARENT)
            << "\n";
  VLOG_INFO << "Use Denoising " << string_from_bool(features & KERNEL_FEATURE_DENOISING) << "\n";
  VLOG_INFO << "Use Path Tracing " << string_from_bool(features & KERNEL_FEATURE_PATH_TRACING)
            << "\n";
  VLOG_INFO << "Use Hair " << string_from_bool(features & KERNEL_FEATURE_HAIR) << "\n";
  VLOG_INFO << "Use Pointclouds " << string_from_bool(features & KERNEL_FEATURE_POINTCLOUD)
            << "\n";
  VLOG_INFO << "Use Object Motion " << string_from_bool(features & KERNEL_FEATURE_OBJECT_MOTION)
            << "\n";
  VLOG_INFO << "Use Baking " << string_from_bool(features & KERNEL_FEATURE_BAKING) << "\n";
  VLOG_INFO << "Use Subsurface " << string_from_bool(features & KERNEL_FEATURE_SUBSURFACE) << "\n";
  VLOG_INFO << "Use Volume " << string_from_bool(features & KERNEL_FEATURE_VOLUME) << "\n";
  VLOG_INFO << "Use Patch Evaluation "
            << string_from_bool(features & KERNEL_FEATURE_PATCH_EVALUATION) << "\n";
  VLOG_INFO << "Use Shadow Catcher " << string_from_bool(features & KERNEL_FEATURE_SHADOW_CATCHER)
            << "\n";
}

bool Scene::load_kernels(Progress &progress)
{
  update_kernel_features();

  const uint kernel_features = dscene.data.kernel_features;

  if (!kernels_loaded || loaded_kernel_features != kernel_features) {
    progress.set_status("Loading render kernels (may take a few minutes the first time)");

    scoped_timer timer;

    log_kernel_features(kernel_features);
    if (!device->load_kernels(kernel_features)) {
      string message = device->error_message();
      if (message.empty())
        message = "Failed loading render kernel, see console for errors";

      progress.set_error(message);
      progress.set_status(message);
      progress.set_update();
      return false;
    }

    kernels_loaded = true;
    loaded_kernel_features = kernel_features;
    return true;
  }
  return false;
}

int Scene::get_max_closure_count()
{
  if (shader_manager->use_osl()) {
    /* OSL always needs the maximum as we can't predict the
     * number of closures a shader might generate. */
    return MAX_CLOSURE;
  }

  int max_closures = 0;
  for (int i = 0; i < shaders.size(); i++) {
    Shader *shader = shaders[i];
    if (shader->reference_count()) {
      int num_closures = shader->graph->get_num_closures();
      max_closures = max(max_closures, num_closures);
    }
  }
  max_closure_global = max(max_closure_global, max_closures);

  if (max_closure_global > MAX_CLOSURE) {
    /* This is usually harmless as more complex shader tend to get many
     * closures discarded due to mixing or low weights. We need to limit
     * to MAX_CLOSURE as this is hardcoded in CPU/mega kernels, and it
     * avoids excessive memory usage for split kernels. */
    VLOG_WARNING << "Maximum number of closures exceeded: " << max_closure_global << " > "
                 << MAX_CLOSURE;

    max_closure_global = MAX_CLOSURE;
  }

  return max_closure_global;
}

int Scene::get_volume_stack_size() const
{
  int volume_stack_size = 0;

  /* Space for background volume and terminator.
   * Don't do optional here because camera ray initialization expects that there is space for
   * at least those elements (avoiding extra condition to check if there is actual volume or not).
   */
  volume_stack_size += 2;

  /* Quick non-expensive check. Can over-estimate maximum possible nested level, but does not
   * require expensive calculation during pre-processing. */
  bool has_volume_object = false;
  for (const Object *object : objects) {
    if (!object->get_geometry()->has_volume) {
      continue;
    }

    if (object->intersects_volume) {
      /* Object intersects another volume, assume it's possible to go deeper in the stack. */
      /* TODO(sergey): This might count nesting twice (A intersects B and B intersects A), but
       * can't think of a computationally cheap algorithm. Dividing my 2 doesn't work because of
       * Venn diagram example with 3 circles. */
      ++volume_stack_size;
    }
    else if (!has_volume_object) {
      /* Allocate space for at least one volume object. */
      ++volume_stack_size;
    }

    has_volume_object = true;

    if (volume_stack_size == MAX_VOLUME_STACK_SIZE) {
      break;
    }
  }

  volume_stack_size = min(volume_stack_size, MAX_VOLUME_STACK_SIZE);

  VLOG_WORK << "Detected required volume stack size " << volume_stack_size;

  return volume_stack_size;
}

bool Scene::has_shadow_catcher()
{
  if (shadow_catcher_modified_) {
    has_shadow_catcher_ = false;
    for (Object *object : objects) {
      if (object->get_is_shadow_catcher()) {
        has_shadow_catcher_ = true;
        break;
      }
    }

    shadow_catcher_modified_ = false;
  }

  return has_shadow_catcher_;
}

void Scene::tag_shadow_catcher_modified()
{
  shadow_catcher_modified_ = true;
}

template<> Light *Scene::create_node<Light>()
{
  Light *node = new Light();
  node->set_owner(this);
  lights.push_back(node);
  light_manager->tag_update(this, LightManager::LIGHT_ADDED);
  return node;
}

template<> Mesh *Scene::create_node<Mesh>()
{
  Mesh *node = new Mesh();
  node->set_owner(this);
  geometry.push_back(node);
  geometry_manager->tag_update(this, GeometryManager::MESH_ADDED);
  return node;
}

template<> Hair *Scene::create_node<Hair>()
{
  Hair *node = new Hair();
  node->set_owner(this);
  geometry.push_back(node);
  geometry_manager->tag_update(this, GeometryManager::HAIR_ADDED);
  return node;
}

template<> Volume *Scene::create_node<Volume>()
{
  Volume *node = new Volume();
  node->set_owner(this);
  geometry.push_back(node);
  geometry_manager->tag_update(this, GeometryManager::MESH_ADDED);
  return node;
}

template<> PointCloud *Scene::create_node<PointCloud>()
{
  PointCloud *node = new PointCloud();
  node->set_owner(this);
  geometry.push_back(node);
  geometry_manager->tag_update(this, GeometryManager::POINT_ADDED);
  return node;
}

template<> Object *Scene::create_node<Object>()
{
  Object *node = new Object();
  node->set_owner(this);
  objects.push_back(node);
  object_manager->tag_update(this, ObjectManager::OBJECT_ADDED);
  return node;
}

template<> ParticleSystem *Scene::create_node<ParticleSystem>()
{
  ParticleSystem *node = new ParticleSystem();
  node->set_owner(this);
  particle_systems.push_back(node);
  particle_system_manager->tag_update(this);
  return node;
}

template<> Shader *Scene::create_node<Shader>()
{
  Shader *node = new Shader();
  node->set_owner(this);
  shaders.push_back(node);
  shader_manager->tag_update(this, ShaderManager::SHADER_ADDED);
  return node;
}

template<> AlembicProcedural *Scene::create_node<AlembicProcedural>()
{
#ifdef WITH_ALEMBIC
  AlembicProcedural *node = new AlembicProcedural();
  node->set_owner(this);
  procedurals.push_back(node);
  procedural_manager->tag_update();
  return node;
#else
  return nullptr;
#endif
}

template<> Pass *Scene::create_node<Pass>()
{
  Pass *node = new Pass();
  node->set_owner(this);
  passes.push_back(node);
  film->tag_modified();
  return node;
}

template<typename T> void delete_node_from_array(vector<T> &nodes, T node)
{
  for (size_t i = 0; i < nodes.size(); ++i) {
    if (nodes[i] == node) {
      std::swap(nodes[i], nodes[nodes.size() - 1]);
      break;
    }
  }

  nodes.resize(nodes.size() - 1);

  delete node;
}

template<> void Scene::delete_node_impl(Light *node)
{
  delete_node_from_array(lights, node);
  light_manager->tag_update(this, LightManager::LIGHT_REMOVED);
}

template<> void Scene::delete_node_impl(Mesh *node)
{
  delete_node_from_array(geometry, static_cast<Geometry *>(node));
  geometry_manager->tag_update(this, GeometryManager::MESH_REMOVED);
}

template<> void Scene::delete_node_impl(Hair *node)
{
  delete_node_from_array(geometry, static_cast<Geometry *>(node));
  geometry_manager->tag_update(this, GeometryManager::HAIR_REMOVED);
}

template<> void Scene::delete_node_impl(Volume *node)
{
  delete_node_from_array(geometry, static_cast<Geometry *>(node));
  geometry_manager->tag_update(this, GeometryManager::MESH_REMOVED);
}

template<> void Scene::delete_node_impl(PointCloud *node)
{
  delete_node_from_array(geometry, static_cast<Geometry *>(node));
  geometry_manager->tag_update(this, GeometryManager::POINT_REMOVED);
}

template<> void Scene::delete_node_impl(Geometry *node)
{
  uint flag;
  if (node->is_hair()) {
    flag = GeometryManager::HAIR_REMOVED;
  }
  else {
    flag = GeometryManager::MESH_REMOVED;
  }

  delete_node_from_array(geometry, node);
  geometry_manager->tag_update(this, flag);
}

template<> void Scene::delete_node_impl(Object *node)
{
  delete_node_from_array(objects, node);
  object_manager->tag_update(this, ObjectManager::OBJECT_REMOVED);
}

template<> void Scene::delete_node_impl(ParticleSystem *node)
{
  delete_node_from_array(particle_systems, node);
  particle_system_manager->tag_update(this);
}

template<> void Scene::delete_node_impl(Shader *shader)
{
  /* don't delete unused shaders, not supported */
  shader->clear_reference_count();
}

template<> void Scene::delete_node_impl(Procedural *node)
{
  delete_node_from_array(procedurals, node);
  procedural_manager->tag_update();
}

template<> void Scene::delete_node_impl(AlembicProcedural *node)
{
#ifdef WITH_ALEMBIC
  delete_node_impl(static_cast<Procedural *>(node));
#else
  (void)node;
#endif
}

template<> void Scene::delete_node_impl(Pass *node)
{
  delete_node_from_array(passes, node);
  film->tag_modified();
}

template<typename T>
static void remove_nodes_in_set(const set<T *> &nodes_set,
                                vector<T *> &nodes_array,
                                const NodeOwner *owner)
{
  size_t new_size = nodes_array.size();

  for (size_t i = 0; i < new_size; ++i) {
    T *node = nodes_array[i];

    if (nodes_set.find(node) != nodes_set.end()) {
      std::swap(nodes_array[i], nodes_array[new_size - 1]);

      assert(node->get_owner() == owner);
      delete node;

      i -= 1;
      new_size -= 1;
    }
  }

  nodes_array.resize(new_size);
  (void)owner;
}

template<> void Scene::delete_nodes(const set<Light *> &nodes, const NodeOwner *owner)
{
  remove_nodes_in_set(nodes, lights, owner);
  light_manager->tag_update(this, LightManager::LIGHT_REMOVED);
}

template<> void Scene::delete_nodes(const set<Geometry *> &nodes, const NodeOwner *owner)
{
  remove_nodes_in_set(nodes, geometry, owner);
  geometry_manager->tag_update(this, GeometryManager::GEOMETRY_REMOVED);
}

template<> void Scene::delete_nodes(const set<Object *> &nodes, const NodeOwner *owner)
{
  remove_nodes_in_set(nodes, objects, owner);
  object_manager->tag_update(this, ObjectManager::OBJECT_REMOVED);
}

template<> void Scene::delete_nodes(const set<ParticleSystem *> &nodes, const NodeOwner *owner)
{
  remove_nodes_in_set(nodes, particle_systems, owner);
  particle_system_manager->tag_update(this);
}

template<> void Scene::delete_nodes(const set<Shader *> &nodes, const NodeOwner * /*owner*/)
{
  /* don't delete unused shaders, not supported */
  for (Shader *shader : nodes) {
    shader->clear_reference_count();
  }
}

template<> void Scene::delete_nodes(const set<Procedural *> &nodes, const NodeOwner *owner)
{
  remove_nodes_in_set(nodes, procedurals, owner);
  procedural_manager->tag_update();
}

template<> void Scene::delete_nodes(const set<Pass *> &nodes, const NodeOwner *owner)
{
  remove_nodes_in_set(nodes, passes, owner);
  film->tag_modified();
}

CCL_NAMESPACE_END
