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

struct DEGObjectIterData;
struct MeshSequenceCacheModifier;

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
  BlenderSync(::RenderEngine &b_engine,
              ::Main &b_data,
              ::Scene &b_scene,
              Scene *scene,
              bool preview,
              bool use_developer_ui,
              Progress &progress);
  ~BlenderSync();

  void reset(::Main &b_data, ::Scene &b_scene);

  void tag_update();

  void set_bake_target(::Object &b_object);

  /* sync */
  void sync_recalc(::Depsgraph &b_depsgraph,
                   ::bScreen *b_screen,
                   ::View3D *b_v3d,
                   ::RegionView3D *b_rv3d);
  void sync_data(::RenderData &b_render,
                 ::Depsgraph &b_depsgraph,
                 ::bScreen *b_screen,
                 ::View3D *b_v3d,
                 ::RegionView3D *b_rv3d,
                 const int width,
                 const int height,
                 void **python_thread_state,
                 const DeviceInfo &denoise_device_info);
  void sync_view_layer(::ViewLayer &b_view_layer);
  void sync_render_passes(::RenderLayer &b_rlay, ::ViewLayer &b_view_layer);
  void sync_integrator(::ViewLayer &b_view_layer,
                       bool background,
                       const DeviceInfo &denoise_device_info);
  void sync_camera(const ::RenderData &b_render,
                   const int width,
                   const int height,
                   const char *viewname);
  void sync_view(::View3D *b_v3d, ::RegionView3D *b_rv3d, const int width, const int height);
  int get_layer_samples()
  {
    return view_layer.samples;
  }
  int get_layer_bound_samples()
  {
    return view_layer.bound_samples;
  }

  /* Early data free. */
  void free_data_after_sync(::Depsgraph &b_depsgraph);

  /* get parameters */
  static SceneParams get_scene_params(::Scene &b_scene,
                                      const bool background,
                                      const bool use_developer_ui);
  static SessionParams get_session_params(::RenderEngine &b_engine,
                                          ::UserDef &b_preferences,
                                          ::Scene &b_scene,
                                          bool background);
  static bool get_session_pause(::Scene &b_scene, bool background);
  static BufferParams get_buffer_params(
      ::View3D *b_v3d, ::RegionView3D *b_rv3d, Camera *cam, const int width, const int height);

  static DenoiseParams get_denoise_params(::Scene &b_scene,
                                          ::ViewLayer *b_view_layer,
                                          bool background,
                                          const DeviceInfo &denoise_device);

 private:
  /* sync */
  void sync_lights(::Depsgraph &b_depsgraph, bool update_all);
  void sync_materials(::Depsgraph &b_depsgraph, bool update_all);
  void sync_objects(::Depsgraph &b_depsgraph,
                    ::bScreen *b_screen,
                    ::View3D *b_v3d,
                    const float motion_time = 0.0f);
  void sync_motion(::RenderData &b_render,
                   ::Depsgraph &b_depsgraph,
                   ::bScreen *b_screen,
                   ::View3D *b_v3d,
                   ::RegionView3D *b_rv3d,
                   const int width,
                   const int height,
                   void **python_thread_state);
  void sync_film(::ViewLayer &b_view_layer, ::bScreen *b_screen, ::View3D *b_v3d);
  void sync_view();

  /* Shader */
  array<Node *> find_used_shaders(::Object &b_ob);
  void sync_world(::Depsgraph &b_depsgraph, ::bScreen *b_screen, ::View3D *b_v3d, bool update_all);
  void sync_shaders(::Depsgraph &b_depsgraph,
                    ::bScreen *b_screen,
                    ::View3D *b_v3d,
                    bool update_all);
  void sync_nodes(Shader *shader, ::bNodeTree &b_ntree);

  bool scene_attr_needs_recalc(Shader *shader, ::Depsgraph &b_depsgraph);
  void resolve_view_layer_attributes(Shader *shader, ShaderGraph *graph, ::Depsgraph &b_depsgraph);

  /* Object */
  Object *sync_object(::ViewLayer &b_view_layer,
                      ::Object &b_ob,
                      ::DEGObjectIterData &b_deg_iter_data,
                      const float motion_time,
                      bool use_particle_hair,
                      bool show_lights,
                      BlenderObjectCulling &culling,
                      TaskPool *geom_task_pool);
  void sync_object_motion_init(::Object &b_parent, ::Object &b_ob, Object *object);

  void sync_procedural(::Object &b_ob,
                       ::MeshSequenceCacheModifier &b_mesh_cache,
                       bool has_subdivision);

  bool sync_object_attributes(::Object &b_ob,
                              ::DEGObjectIterData &b_deg_iter_data,
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
                          const ::Mesh &b_mesh,
                          BObjectInfo &b_ob_info,
                          bool motion,
                          const int motion_step = 0);
  bool object_has_particle_hair(::Object *b_ob);

  /* Point Cloud */
  void sync_pointcloud(PointCloud *pointcloud, BObjectInfo &b_ob_info);
  void sync_pointcloud_motion(PointCloud *pointcloud,
                              BObjectInfo &b_ob_info,
                              const int motion_step = 0);

  /* Camera */
  void sync_camera_motion(const ::RenderData &b_render,
                          ::Object *b_ob,
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
  void sync_background_light(::bScreen *b_screen, ::View3D *b_v3d);

  /* Particles */
  bool sync_dupli_particle(::Object &b_parent,
                           ::DEGObjectIterData &b_deg_iter_data,
                           ::Object &b_ob,
                           Object *object);

  /* Images. */
  void sync_images();

  /* util */
  void find_shader(const ::ID *id, array<Node *> &used_shaders, Shader *default_shader);
  bool BKE_object_is_modified(::Object &b_ob);
  bool object_is_geometry(BObjectInfo &b_ob_info);
  bool object_can_have_geometry(::Object &b_ob);
  bool object_is_light(::Object &b_ob);
  bool object_is_camera(::Object &b_ob);

  ::Object *get_camera_object(::View3D *b_v3d, ::RegionView3D *b_rv3d);
  ::Object *get_dicing_camera_object(::View3D *b_v3d, ::RegionView3D *b_rv3d);

  /* variables */
  ::RenderEngine *b_engine;
  ::Main *b_data;
  ::Scene *b_scene;
  ::Object *b_bake_target;

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
  map<void *, set<::ID *>> instance_geometries_by_object;
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
    RenderLayerInfo() : material_override(nullptr), world_override(nullptr) {}

    string name;
    ::Material *material_override;
    ::World *world_override;
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
