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

#ifndef __BLENDER_SYNC_H__
#define __BLENDER_SYNC_H__

#include "MEM_guardedalloc.h"
#include "RNA_access.h"
#include "RNA_blender_cpp.h"
#include "RNA_types.h"

#include "blender/blender_id_map.h"
#include "blender/blender_viewport.h"

#include "render/scene.h"
#include "render/session.h"

#include "util/util_map.h"
#include "util/util_set.h"
#include "util/util_transform.h"
#include "util/util_vector.h"

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

class BlenderSync {
 public:
  BlenderSync(BL::RenderEngine &b_engine,
              BL::BlendData &b_data,
              BL::Scene &b_scene,
              Scene *scene,
              bool preview,
              Progress &progress);
  ~BlenderSync();

  void reset(BL::BlendData &b_data, BL::Scene &b_scene);

  /* sync */
  void sync_recalc(BL::Depsgraph &b_depsgraph, BL::SpaceView3D &b_v3d);
  void sync_data(BL::RenderSettings &b_render,
                 BL::Depsgraph &b_depsgraph,
                 BL::SpaceView3D &b_v3d,
                 BL::Object &b_override,
                 int width,
                 int height,
                 void **python_thread_state);
  void sync_view_layer(BL::SpaceView3D &b_v3d, BL::ViewLayer &b_view_layer);
  vector<Pass> sync_render_passes(BL::RenderLayer &b_render_layer,
                                  BL::ViewLayer &b_view_layer,
                                  bool adaptive_sampling);
  void sync_integrator();
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
  static SceneParams get_scene_params(BL::Scene &b_scene, bool background);
  static SessionParams get_session_params(BL::RenderEngine &b_engine,
                                          BL::Preferences &b_userpref,
                                          BL::Scene &b_scene,
                                          bool background);
  static bool get_session_pause(BL::Scene &b_scene, bool background);
  static BufferParams get_buffer_params(BL::Scene &b_scene,
                                        BL::RenderSettings &b_render,
                                        BL::SpaceView3D &b_v3d,
                                        BL::RegionView3D &b_rv3d,
                                        Camera *cam,
                                        int width,
                                        int height);

  static PassType get_pass_type(BL::RenderPass &b_pass);
  static int get_denoising_pass(BL::RenderPass &b_pass);

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
  void sync_film(BL::SpaceView3D &b_v3d);
  void sync_view();

  /* Shader */
  void sync_world(BL::Depsgraph &b_depsgraph, BL::SpaceView3D &b_v3d, bool update_all);
  void sync_shaders(BL::Depsgraph &b_depsgraph, BL::SpaceView3D &b_v3d);
  void sync_nodes(Shader *shader, BL::ShaderNodeTree &b_ntree);

  /* Object */
  Object *sync_object(BL::Depsgraph &b_depsgraph,
                      BL::ViewLayer &b_view_layer,
                      BL::DepsgraphObjectInstance &b_instance,
                      float motion_time,
                      bool use_particle_hair,
                      bool show_lights,
                      BlenderObjectCulling &culling,
                      bool *use_portal);

  /* Volume */
  void sync_volume(BL::Object &b_ob, Mesh *mesh, const vector<Shader *> &used_shaders);

  /* Mesh */
  void sync_mesh(BL::Depsgraph b_depsgraph,
                 BL::Object b_ob,
                 Mesh *mesh,
                 const vector<Shader *> &used_shaders);
  void sync_mesh_motion(BL::Depsgraph b_depsgraph, BL::Object b_ob, Mesh *mesh, int motion_step);

  /* Hair */
  void sync_hair(BL::Depsgraph b_depsgraph,
                 BL::Object b_ob,
                 Hair *hair,
                 const vector<Shader *> &used_shaders);
  void sync_hair_motion(BL::Depsgraph b_depsgraph, BL::Object b_ob, Hair *hair, int motion_step);
  void sync_hair(Hair *hair, BL::Object &b_ob, bool motion, int motion_step = 0);
  void sync_particle_hair(
      Hair *hair, BL::Mesh &b_mesh, BL::Object &b_ob, bool motion, int motion_step = 0);
  void sync_curve_settings(BL::Depsgraph &b_depsgraph);
  bool object_has_particle_hair(BL::Object b_ob);

  /* Camera */
  void sync_camera_motion(
      BL::RenderSettings &b_render, BL::Object &b_ob, int width, int height, float motion_time);

  /* Geometry */
  Geometry *sync_geometry(BL::Depsgraph &b_depsgrpah,
                          BL::Object &b_ob,
                          BL::Object &b_ob_instance,
                          bool object_updated,
                          bool use_particle_hair);
  void sync_geometry_motion(BL::Depsgraph &b_depsgraph,
                            BL::Object &b_ob,
                            Object *object,
                            float motion_time,
                            bool use_particle_hair);

  /* Light */
  void sync_light(BL::Object &b_parent,
                  int persistent_id[OBJECT_PERSISTENT_ID_SIZE],
                  BL::Object &b_ob,
                  BL::Object &b_ob_instance,
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
  void find_shader(BL::ID &id, vector<Shader *> &used_shaders, Shader *default_shader);
  bool BKE_object_is_modified(BL::Object &b_ob);
  bool object_is_mesh(BL::Object &b_ob);
  bool object_is_light(BL::Object &b_ob);

  /* variables */
  BL::RenderEngine b_engine;
  BL::BlendData b_data;
  BL::Scene b_scene;

  id_map<void *, Shader> shader_map;
  id_map<ObjectKey, Object> object_map;
  id_map<GeometryKey, Geometry> geometry_map;
  id_map<ObjectKey, Light> light_map;
  id_map<ParticleSystemKey, ParticleSystem> particle_system_map;
  set<Geometry *> geometry_synced;
  set<Geometry *> geometry_motion_synced;
  set<float> motion_times;
  void *world_map;
  bool world_recalc;
  BlenderViewportParameters viewport_parameters;

  Scene *scene;
  bool preview;
  bool experimental;

  float dicing_rate;
  int max_subdivisions;

  struct RenderLayerInfo {
    RenderLayerInfo()
        : material_override(PointerRNA_NULL),
          use_background_shader(true),
          use_background_ao(true),
          use_surfaces(true),
          use_hair(true),
          use_volumes(true),
          samples(0),
          bound_samples(false)
    {
    }

    string name;
    BL::Material material_override;
    bool use_background_shader;
    bool use_background_ao;
    bool use_surfaces;
    bool use_hair;
    bool use_volumes;
    int samples;
    bool bound_samples;
  } view_layer;

  Progress &progress;
};

CCL_NAMESPACE_END

#endif /* __BLENDER_SYNC_H__ */
