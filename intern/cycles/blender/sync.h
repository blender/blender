/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "RNA_blender_cpp.hh"
#include "RNA_types.hh"

#include "blender/id_map.h"
#include "blender/util.h"
#include "blender/viewport.h"

#include "scene/scene.h"
#include "session/session.h"

#include "util/map.h"
#include "util/set.h"

CCL_NAMESPACE_BEGIN

class Background;
class BlenderObjectCulling;
class BlenderViewportParameters;
class Camera;
class Film;
class Hair;
class Light;
class Mesh;
class Object;
class ParticleSystem;
class Scene;
class Shader;
class ShaderGraph;
class TaskPool;

class BlenderSync {
 public:
  BlenderSync(BL::RenderEngine &b_engine,
              BL::BlendData &b_data,
              BL::Scene &b_scene,
              Scene *scene,
              bool preview,
              bool use_developer_ui,
              Progress &progress);
  ~BlenderSync();

  void reset(BL::BlendData &b_data, BL::Scene &b_scene);

  void tag_update();

  void set_bake_target(BL::Object &b_object);

  /* sync */
  void sync_recalc(BL::Depsgraph &b_depsgraph, BL::SpaceView3D &b_v3d, BL::RegionView3D &b_rv3d);
  void sync_data(BL::RenderSettings &b_render,
                 BL::Depsgraph &b_depsgraph,
                 BL::SpaceView3D &b_v3d,
                 BL::RegionView3D &b_rv3d,
                 const int width,
                 const int height,
                 void **python_thread_state,
                 const DeviceInfo &denoise_device_info);
  void sync_view_layer(BL::ViewLayer &b_view_layer);
  void sync_render_passes(BL::RenderLayer &b_rlay, BL::ViewLayer &b_view_layer);
  void sync_integrator(BL::ViewLayer &b_view_layer,
                       bool background,
                       const DeviceInfo &denoise_device_info);
  void sync_camera(BL::RenderSettings &b_render,
                   const int width,
                   const int height,
                   const char *viewname);
  void sync_view(BL::SpaceView3D &b_v3d,
                 BL::RegionView3D &b_rv3d,
                 const int width,
                 const int height);
  int get_layer_samples()
  {
    return view_layer.samples;
  }
  int get_layer_bound_samples()
  {
    return view_layer.bound_samples;
  }

  /* Early data free. */
  void free_data_after_sync(BL::Depsgraph &b_depsgraph);

  /* get parameters */
  static SceneParams get_scene_params(BL::Scene &b_scene,
                                      const bool background,
                                      const bool use_developer_ui);
  static SessionParams get_session_params(BL::RenderEngine &b_engine,
                                          BL::Preferences &b_preferences,
                                          BL::Scene &b_scene,
                                          bool background);
  static bool get_session_pause(BL::Scene &b_scene, bool background);
  static BufferParams get_buffer_params(BL::SpaceView3D &b_v3d,
                                        BL::RegionView3D &b_rv3d,
                                        Camera *cam,
                                        const int width,
                                        const int height);

  static DenoiseParams get_denoise_params(BL::Scene &b_scene,
                                          BL::ViewLayer &b_view_layer,
                                          bool background,
                                          const DeviceInfo &denoise_device);

 private:
  /* sync */
  void sync_lights(BL::Depsgraph &b_depsgraph, bool update_all);
  void sync_materials(BL::Depsgraph &b_depsgraph, bool update_all);
  void sync_objects(BL::Depsgraph &b_depsgraph,
                    BL::SpaceView3D &b_v3d,
                    const float motion_time = 0.0f);
  void sync_motion(BL::RenderSettings &b_render,
                   BL::Depsgraph &b_depsgraph,
                   BL::SpaceView3D &b_v3d,
                   BL::RegionView3D &b_rv3d,
                   const int width,
                   const int height,
                   void **python_thread_state);
  void sync_film(BL::ViewLayer &b_view_layer, BL::SpaceView3D &b_v3d);
  void sync_view();

  /* Shader */
  array<Node *> find_used_shaders(BL::Object &b_ob);
  void sync_world(BL::Depsgraph &b_depsgraph, BL::SpaceView3D &b_v3d, bool update_all);
  void sync_shaders(BL::Depsgraph &b_depsgraph, BL::SpaceView3D &b_v3d, bool update_all);
  void sync_nodes(Shader *shader, BL::ShaderNodeTree &b_ntree);

  bool scene_attr_needs_recalc(Shader *shader, BL::Depsgraph &b_depsgraph);
  void resolve_view_layer_attributes(Shader *shader,
                                     ShaderGraph *graph,
                                     BL::Depsgraph &b_depsgraph);

  /* Object */
  Object *sync_object(BL::ViewLayer &b_view_layer,
                      BL::DepsgraphObjectInstance &b_instance,
                      const float motion_time,
                      bool use_particle_hair,
                      bool show_lights,
                      BlenderObjectCulling &culling,
                      TaskPool *geom_task_pool);
  void sync_object_motion_init(BL::Object &b_parent, BL::Object &b_ob, Object *object);

  void sync_procedural(BL::Object &b_ob,
                       BL::MeshSequenceCacheModifier &b_mesh_cache,
                       bool has_subdivision);

  bool sync_object_attributes(BL::DepsgraphObjectInstance &b_instance, Object *object);

  /* Volume */
  void sync_volume(BObjectInfo &b_ob_info, Volume *volume);

  /* Mesh */
  void sync_mesh(BObjectInfo &b_ob_info, Mesh *mesh);
  void sync_mesh_motion(BObjectInfo &b_ob_info, Mesh *mesh, int motion_step);

  /* Hair */
  void sync_hair(BObjectInfo &b_ob_info, Hair *hair);
  void sync_hair_motion(BObjectInfo &b_ob_info, Hair *hair, int motion_step);
  void sync_hair(Hair *hair, BObjectInfo &b_ob_info, bool motion, const int motion_step = 0);
  void sync_particle_hair(Hair *hair,
                          BL::Mesh &b_mesh,
                          BObjectInfo &b_ob_info,
                          bool motion,
                          const int motion_step = 0);
  bool object_has_particle_hair(BL::Object b_ob);

  /* Point Cloud */
  void sync_pointcloud(PointCloud *pointcloud, BObjectInfo &b_ob_info);
  void sync_pointcloud_motion(PointCloud *pointcloud,
                              BObjectInfo &b_ob_info,
                              const int motion_step = 0);

  /* Camera */
  void sync_camera_motion(BL::RenderSettings &b_render,
                          BL::Object &b_ob,
                          const int width,
                          const int height,
                          const float motion_time);

  /* Geometry */
  Geometry *sync_geometry(BObjectInfo &b_ob_info,
                          bool object_updated,
                          bool use_particle_hair,
                          TaskPool *task_pool);

  void sync_geometry_motion(BObjectInfo &b_ob_info,
                            Object *object,
                            const float motion_time,
                            bool use_particle_hair,
                            TaskPool *task_pool);

  /* Light */
  void sync_light(BObjectInfo &b_ob_info, Light *light);
  void sync_background_light(BL::SpaceView3D &b_v3d);

  /* Particles */
  bool sync_dupli_particle(BL::Object &b_ob,
                           BL::DepsgraphObjectInstance &b_instance,
                           Object *object);

  /* Images. */
  void sync_images();

  /* util */
  void find_shader(const BL::ID &id, array<Node *> &used_shaders, Shader *default_shader);
  bool BKE_object_is_modified(BL::Object &b_ob);
  bool object_is_geometry(BObjectInfo &b_ob_info);
  bool object_can_have_geometry(BL::Object &b_ob);
  bool object_is_light(BL::Object &b_ob);
  bool object_is_camera(BL::Object &b_ob);

  BL::Object get_camera_object(BL::SpaceView3D b_v3d, BL::RegionView3D b_rv3d);
  BL::Object get_dicing_camera_object(BL::SpaceView3D b_v3d, BL::RegionView3D b_rv3d);

  /* variables */
  BL::RenderEngine b_engine;
  BL::BlendData b_data;
  BL::Scene b_scene;
  BL::Object b_bake_target;

  enum ShaderFlags { SHADER_WITH_LAYER_ATTRS };

  id_map<void *, Shader, ShaderFlags> shader_map;
  id_map<ObjectKey, Object> object_map;
  id_map<void *, Procedural> procedural_map;
  id_map<GeometryKey, Geometry> geometry_map;
  id_map<ParticleSystemKey, ParticleSystem> particle_system_map;
  set<Geometry *> geometry_synced;
  set<Geometry *> geometry_motion_synced;
  set<Geometry *> geometry_motion_attribute_synced;
  /** Remember which geometries come from which objects to be able to sync them after changes. */
  map<void *, set<BL::ID>> instance_geometries_by_object;
  set<float> motion_times;
  void *world_map;
  bool world_recalc;
  bool world_use_portal = false;
  BlenderViewportParameters viewport_parameters;

  Scene *scene;
  bool preview;
  bool use_experimental_procedural = false;
  bool use_adaptive_subdivision = false;
  bool use_developer_ui;

  float dicing_rate;
  int max_subdivisions;

  struct RenderLayerInfo {
    RenderLayerInfo() : material_override(PointerRNA_NULL), world_override(PointerRNA_NULL) {}

    string name;
    BL::Material material_override;
    BL::World world_override;
    bool use_background_shader = true;
    bool use_surfaces = true;
    bool use_hair = true;
    bool use_volumes = true;
    bool use_motion_blur = true;
    int samples = 0;
    bool bound_samples = false;
  } view_layer;

  Progress &progress;

  /* Indicates that `sync_recalc()` detected changes in the scene.
   * If this flag is false then the data is considered to be up-to-date and will not be
   * synchronized at all. */
  bool has_updates_ = true;
};

CCL_NAMESPACE_END
