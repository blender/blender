/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __BLENDER_SYNC_H__
#define __BLENDER_SYNC_H__

#include "MEM_guardedalloc.h"
#include "RNA_access.hh"
#include "RNA_blender_cpp.h"
#include "RNA_path.hh"
#include "RNA_types.hh"

#include "blender/id_map.h"
#include "blender/util.h"
#include "blender/viewport.h"

#include "scene/scene.h"
#include "session/session.h"

#include "util/map.h"
#include "util/set.h"
#include "util/transform.h"
#include "util/vector.h"

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
class ViewLayer;
class Shader;
class ShaderGraph;
class ShaderNode;
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

  /* sync */
  void sync_recalc(BL::Depsgraph &b_depsgraph, BL::SpaceView3D &b_v3d);
  void sync_data(BL::RenderSettings &b_render,
                 BL::Depsgraph &b_depsgraph,
                 BL::SpaceView3D &b_v3d,
                 BL::Object &b_override,
                 int width,
                 int height,
                 void **python_thread_state);
  void sync_view_layer(BL::ViewLayer &b_view_layer);
  void sync_render_passes(BL::RenderLayer &b_render_layer, BL::ViewLayer &b_view_layer);
  void sync_integrator(BL::ViewLayer &b_view_layer, bool background);
  void sync_camera(BL::RenderSettings &b_render,
                   BL::Object &b_override,
                   int width,
                   int height,
                   const char *viewname);
  void sync_view(BL::SpaceView3D &b_v3d, BL::RegionView3D &b_rv3d, int width, int height);
  inline int get_layer_samples()
  {
    return view_layer.samples;
  }
  inline int get_layer_bound_samples()
  {
    return view_layer.bound_samples;
  }

  /* get parameters */
  static SceneParams get_scene_params(BL::Scene &b_scene,
                                      const bool background,
                                      const bool use_developer_ui);
  static SessionParams get_session_params(BL::RenderEngine &b_engine,
                                          BL::Preferences &b_userpref,
                                          BL::Scene &b_scene,
                                          bool background);
  static bool get_session_pause(BL::Scene &b_scene, bool background);
  static BufferParams get_buffer_params(
      BL::SpaceView3D &b_v3d, BL::RegionView3D &b_rv3d, Camera *cam, int width, int height);

  static DenoiseParams get_denoise_params(BL::Scene &b_scene,
                                          BL::ViewLayer &b_view_layer,
                                          bool background);

 private:
  /* sync */
  void sync_lights(BL::Depsgraph &b_depsgraph, bool update_all);
  void sync_materials(BL::Depsgraph &b_depsgraph, bool update_all);
  void sync_objects(BL::Depsgraph &b_depsgraph, BL::SpaceView3D &b_v3d, float motion_time = 0.0f);
  void sync_motion(BL::RenderSettings &b_render,
                   BL::Depsgraph &b_depsgraph,
                   BL::SpaceView3D &b_v3d,
                   BL::Object &b_override,
                   int width,
                   int height,
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
  Object *sync_object(BL::Depsgraph &b_depsgraph,
                      BL::ViewLayer &b_view_layer,
                      BL::DepsgraphObjectInstance &b_instance,
                      float motion_time,
                      bool use_particle_hair,
                      bool show_lights,
                      BlenderObjectCulling &culling,
                      bool *use_portal,
                      TaskPool *geom_task_pool);
  void sync_object_motion_init(BL::Object &b_parent, BL::Object &b_ob, Object *object);

  void sync_procedural(BL::Object &b_ob,
                       BL::MeshSequenceCacheModifier &b_mesh_cache,
                       bool has_subdivision);

  bool sync_object_attributes(BL::DepsgraphObjectInstance &b_instance, Object *object);

  /* Volume */
  void sync_volume(BObjectInfo &b_ob_info, Volume *volume);

  /* Mesh */
  void sync_mesh(BL::Depsgraph b_depsgraph, BObjectInfo &b_ob_info, Mesh *mesh);
  void sync_mesh_motion(BL::Depsgraph b_depsgraph,
                        BObjectInfo &b_ob_info,
                        Mesh *mesh,
                        int motion_step);

  /* Hair */
  void sync_hair(BL::Depsgraph b_depsgraph, BObjectInfo &b_ob_info, Hair *hair);
  void sync_hair_motion(BL::Depsgraph b_depsgraph,
                        BObjectInfo &b_ob_info,
                        Hair *hair,
                        int motion_step);
  void sync_hair(Hair *hair, BObjectInfo &b_ob_info, bool motion, int motion_step = 0);
  void sync_particle_hair(
      Hair *hair, BL::Mesh &b_mesh, BObjectInfo &b_ob_info, bool motion, int motion_step = 0);
  bool object_has_particle_hair(BL::Object b_ob);

  /* Point Cloud */
  void sync_pointcloud(PointCloud *pointcloud, BObjectInfo &b_ob_info);
  void sync_pointcloud_motion(PointCloud *pointcloud, BObjectInfo &b_ob_info, int motion_step = 0);

  /* Camera */
  void sync_camera_motion(
      BL::RenderSettings &b_render, BL::Object &b_ob, int width, int height, float motion_time);

  /* Geometry */
  Geometry *sync_geometry(BL::Depsgraph &b_depsgraph,
                          BObjectInfo &b_ob_info,
                          bool object_updated,
                          bool use_particle_hair,
                          TaskPool *task_pool);

  void sync_geometry_motion(BL::Depsgraph &b_depsgraph,
                            BObjectInfo &b_ob_info,
                            Object *object,
                            float motion_time,
                            bool use_particle_hair,
                            TaskPool *task_pool);

  /* Light */
  void sync_light(BL::Object &b_parent,
                  int persistent_id[OBJECT_PERSISTENT_ID_SIZE],
                  BObjectInfo &b_ob_info,
                  int random_id,
                  Transform &tfm,
                  bool *use_portal);
  void sync_background_light(BL::SpaceView3D &b_v3d, bool use_portal);

  /* Particles */
  bool sync_dupli_particle(BL::Object &b_ob,
                           BL::DepsgraphObjectInstance &b_instance,
                           Object *object);

  /* Images. */
  void sync_images();

  /* Early data free. */
  void free_data_after_sync(BL::Depsgraph &b_depsgraph);

  /* util */
  void find_shader(BL::ID &id, array<Node *> &used_shaders, Shader *default_shader);
  bool BKE_object_is_modified(BL::Object &b_ob);
  bool object_is_geometry(BObjectInfo &b_ob_info);
  bool object_can_have_geometry(BL::Object &b_ob);
  bool object_is_light(BL::Object &b_ob);
  bool object_is_camera(BL::Object &b_ob);

  /* variables */
  BL::RenderEngine b_engine;
  BL::BlendData b_data;
  BL::Scene b_scene;

  enum ShaderFlags { SHADER_WITH_LAYER_ATTRS };

  id_map<void *, Shader, ShaderFlags> shader_map;
  id_map<ObjectKey, Object> object_map;
  id_map<void *, Procedural> procedural_map;
  id_map<GeometryKey, Geometry> geometry_map;
  id_map<ObjectKey, Light> light_map;
  id_map<ParticleSystemKey, ParticleSystem> particle_system_map;
  set<Geometry *> geometry_synced;
  set<Geometry *> geometry_motion_synced;
  set<Geometry *> geometry_motion_attribute_synced;
  /** Remember which geometries come from which objects to be able to sync them after changes. */
  map<void *, set<BL::ID>> instance_geometries_by_object;
  set<float> motion_times;
  void *world_map;
  bool world_recalc;
  BlenderViewportParameters viewport_parameters;

  Scene *scene;
  bool preview;
  bool experimental;
  bool use_developer_ui;

  float dicing_rate;
  int max_subdivisions;

  struct RenderLayerInfo {
    RenderLayerInfo()
        : material_override(PointerRNA_NULL),
          use_background_shader(true),
          use_surfaces(true),
          use_hair(true),
          use_volumes(true),
          use_motion_blur(true),
          samples(0),
          bound_samples(false)
    {
    }

    string name;
    BL::Material material_override;
    bool use_background_shader;
    bool use_surfaces;
    bool use_hair;
    bool use_volumes;
    bool use_motion_blur;
    int samples;
    bool bound_samples;
  } view_layer;

  Progress &progress;

  /* Indicates that `sync_recalc()` detected changes in the scene.
   * If this flag is false then the data is considered to be up-to-date and will not be
   * synchronized at all. */
  bool has_updates_ = true;
};

CCL_NAMESPACE_END

#endif /* __BLENDER_SYNC_H__ */
