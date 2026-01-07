/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "RNA_types.hh"

#include "blender/id_map.h"
#include "blender/util.h"
#include "blender/viewport.h"

#include "scene/scene.h"
#include "session/session.h"

#include "util/map.h"
#include "util/set.h"

namespace blender {
struct DEGObjectIterData;
struct MeshSequenceCacheModifier;
}  // namespace blender

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
  BlenderSync(blender::RenderEngine &b_engine,
              blender::Main &b_data,
              blender::Scene &b_scene,
              Scene *scene,
              bool preview,
              bool use_developer_ui,
              Progress &progress);
  ~BlenderSync();

  void reset(blender::Main &b_data, blender::Scene &b_scene);

  void tag_update();

  void set_bake_target(blender::Object &b_object);

  /* sync */
  void sync_recalc(blender::Depsgraph &b_depsgraph,
                   blender::bScreen *b_screen,
                   blender::View3D *b_v3d,
                   blender::RegionView3D *b_rv3d);
  void sync_data(blender::RenderData &b_render,
                 blender::Depsgraph &b_depsgraph,
                 blender::bScreen *b_screen,
                 blender::View3D *b_v3d,
                 blender::RegionView3D *b_rv3d,
                 const int width,
                 const int height,
                 void **python_thread_state,
                 const DeviceInfo &denoise_device_info);
  void sync_view_layer(blender::ViewLayer &b_view_layer);
  void sync_render_passes(blender::RenderLayer &b_rlay, blender::ViewLayer &b_view_layer);
  void sync_integrator(blender::ViewLayer &b_view_layer,
                       bool background,
                       const DeviceInfo &denoise_device_info);
  void sync_camera(const blender::RenderData &b_render,
                   const int width,
                   const int height,
                   const char *viewname);
  void sync_view(blender::View3D *b_v3d,
                 blender::RegionView3D *b_rv3d,
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
  void free_data_after_sync(blender::Depsgraph &b_depsgraph);

  /* get parameters */
  static SceneParams get_scene_params(blender::Scene &b_scene,
                                      const bool background,
                                      const bool use_developer_ui);
  static SessionParams get_session_params(blender::RenderEngine &b_engine,
                                          blender::UserDef &b_preferences,
                                          blender::Scene &b_scene,
                                          bool background);
  static bool get_session_pause(blender::Scene &b_scene, bool background);
  static BufferParams get_buffer_params(blender::View3D *b_v3d,
                                        blender::RegionView3D *b_rv3d,
                                        Camera *cam,
                                        const int width,
                                        const int height);

  static DenoiseParams get_denoise_params(blender::Scene &b_scene,
                                          blender::ViewLayer *b_view_layer,
                                          bool background,
                                          const DeviceInfo &denoise_device);

 private:
  /* sync */
  void sync_lights(blender::Depsgraph &b_depsgraph, bool update_all);
  void sync_materials(blender::Depsgraph &b_depsgraph, bool update_all);
  void sync_objects(blender::Depsgraph &b_depsgraph,
                    blender::bScreen *b_screen,
                    blender::View3D *b_v3d,
                    const float motion_time = 0.0f);
  void sync_motion(blender::RenderData &b_render,
                   blender::Depsgraph &b_depsgraph,
                   blender::bScreen *b_screen,
                   blender::View3D *b_v3d,
                   blender::RegionView3D *b_rv3d,
                   const int width,
                   const int height,
                   void **python_thread_state);
  void sync_film(blender::ViewLayer &b_view_layer,
                 blender::bScreen *b_screen,
                 blender::View3D *b_v3d);
  void sync_view();

  /* Shader */
  array<Node *> find_used_shaders(blender::Object &b_ob);
  void sync_world(blender::Depsgraph &b_depsgraph,
                  blender::bScreen *b_screen,
                  blender::View3D *b_v3d,
                  bool update_all);
  void sync_shaders(blender::Depsgraph &b_depsgraph,
                    blender::bScreen *b_screen,
                    blender::View3D *b_v3d,
                    bool update_all);
  void sync_nodes(Shader *shader, blender::bNodeTree &b_ntree);

  bool scene_attr_needs_recalc(Shader *shader, blender::Depsgraph &b_depsgraph);
  void resolve_view_layer_attributes(Shader *shader,
                                     ShaderGraph *graph,
                                     blender::Depsgraph &b_depsgraph);

  /* Object */
  Object *sync_object(blender::ViewLayer &b_view_layer,
                      blender::Object &b_ob,
                      blender::DEGObjectIterData &b_deg_iter_data,
                      const float motion_time,
                      bool use_particle_hair,
                      bool show_lights,
                      BlenderObjectCulling &culling,
                      TaskPool *geom_task_pool);
  void sync_object_motion_init(blender::Object &b_parent, blender::Object &b_ob, Object *object);

  void sync_procedural(blender::Object &b_ob,
                       blender::MeshSequenceCacheModifier &b_mesh_cache,
                       bool has_subdivision);

  bool sync_object_attributes(blender::Object &b_ob,
                              blender::DEGObjectIterData &b_deg_iter_data,
                              Object *object);

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
                          const blender::Mesh &b_mesh,
                          BObjectInfo &b_ob_info,
                          bool motion,
                          const int motion_step = 0);
  bool object_has_particle_hair(blender::Object *b_ob);

  /* Point Cloud */
  void sync_pointcloud(PointCloud *pointcloud, BObjectInfo &b_ob_info);
  void sync_pointcloud_motion(PointCloud *pointcloud,
                              BObjectInfo &b_ob_info,
                              const int motion_step = 0);

  /* Camera */
  void sync_camera_motion(const blender::RenderData &b_render,
                          blender::Object *b_ob,
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
  void sync_background_light(blender::bScreen *b_screen, blender::View3D *b_v3d);

  /* Particles */
  bool sync_dupli_particle(blender::Object &b_parent,
                           blender::DEGObjectIterData &b_deg_iter_data,
                           blender::Object &b_ob,
                           Object *object);

  /* Images. */
  void sync_images();

  /* util */
  void find_shader(const blender::ID *id, array<Node *> &used_shaders, Shader *default_shader);
  bool BKE_object_is_modified(blender::Object &b_ob);
  bool object_is_geometry(BObjectInfo &b_ob_info);
  bool object_can_have_geometry(blender::Object &b_ob);
  bool object_is_light(blender::Object &b_ob);
  bool object_is_camera(blender::Object &b_ob);

  blender::Object *get_camera_object(blender::View3D *b_v3d, blender::RegionView3D *b_rv3d);
  blender::Object *get_dicing_camera_object(blender::View3D *b_v3d, blender::RegionView3D *b_rv3d);

  /* variables */
  blender::RenderEngine *b_engine;
  blender::Main *b_data;
  blender::Scene *b_scene;
  blender::Object *b_bake_target;

  enum ShaderFlags { SHADER_WITH_LAYER_ATTRS };

  id_map<const void *, Shader, ShaderFlags> shader_map;
  id_map<ObjectKey, Object> object_map;
  id_map<void *, Procedural> procedural_map;
  id_map<GeometryKey, Geometry> geometry_map;
  id_map<ParticleSystemKey, ParticleSystem> particle_system_map;
  set<Geometry *> geometry_synced;
  set<Geometry *> geometry_motion_synced;
  set<Geometry *> geometry_motion_attribute_synced;
  /** Remember which geometries come from which objects to be able to sync them after changes. */
  map<void *, set<blender::ID *>> instance_geometries_by_object;
  set<float> motion_times;
  void *world_map;
  bool world_recalc;
  BlenderViewportParameters viewport_parameters;

  Scene *scene;
  bool preview;
  bool use_adaptive_subdivision = false;
  bool use_developer_ui;

  CurveShapeType curve_shape = CURVE_RIBBON;

  float dicing_rate;
  int max_subdivisions;

  struct RenderLayerInfo {
    string name;
    blender::Material *material_override = nullptr;
    blender::World *world_override = nullptr;
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
