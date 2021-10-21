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

#ifndef __SCENE_H__
#define __SCENE_H__

#include "bvh/bvh_params.h"

#include "render/film.h"
#include "render/image.h"
#include "render/shader.h"

#include "device/device.h"
#include "device/device_memory.h"

#include "util/util_param.h"
#include "util/util_string.h"
#include "util/util_system.h"
#include "util/util_texture.h"
#include "util/util_thread.h"
#include "util/util_types.h"
#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

class AlembicProcedural;
class AttributeRequestSet;
class Background;
class BVH;
class Camera;
class Device;
class DeviceInfo;
class Film;
class Integrator;
class Light;
class LightManager;
class LookupTables;
class Geometry;
class GeometryManager;
class Object;
class ObjectManager;
class ParticleSystemManager;
class ParticleSystem;
class Procedural;
class ProceduralManager;
class CurveSystemManager;
class Shader;
class ShaderManager;
class Progress;
class BakeManager;
class BakeData;
class RenderStats;
class SceneUpdateStats;
class Volume;

/* Scene Device Data */

class DeviceScene {
 public:
  /* BVH */
  device_vector<int4> bvh_nodes;
  device_vector<int4> bvh_leaf_nodes;
  device_vector<int> object_node;
  device_vector<int> prim_type;
  device_vector<uint> prim_visibility;
  device_vector<int> prim_index;
  device_vector<int> prim_object;
  device_vector<float2> prim_time;

  /* mesh */
  device_vector<float4> tri_verts;
  device_vector<uint> tri_shader;
  device_vector<float4> tri_vnormal;
  device_vector<uint4> tri_vindex;
  device_vector<uint> tri_patch;
  device_vector<float2> tri_patch_uv;

  device_vector<KernelCurve> curves;
  device_vector<float4> curve_keys;
  device_vector<KernelCurveSegment> curve_segments;

  device_vector<uint> patches;

  /* objects */
  device_vector<KernelObject> objects;
  device_vector<Transform> object_motion_pass;
  device_vector<DecomposedTransform> object_motion;
  device_vector<uint> object_flag;
  device_vector<float> object_volume_step;

  /* cameras */
  device_vector<DecomposedTransform> camera_motion;

  /* attributes */
  device_vector<uint4> attributes_map;
  device_vector<float> attributes_float;
  device_vector<float2> attributes_float2;
  device_vector<float4> attributes_float3;
  device_vector<uchar4> attributes_uchar4;

  /* lights */
  device_vector<KernelLightDistribution> light_distribution;
  device_vector<KernelLight> lights;
  device_vector<float2> light_background_marginal_cdf;
  device_vector<float2> light_background_conditional_cdf;

  /* particles */
  device_vector<KernelParticle> particles;

  /* shaders */
  device_vector<int4> svm_nodes;
  device_vector<KernelShader> shaders;

  /* lookup tables */
  device_vector<float> lookup_table;

  /* integrator */
  device_vector<float> sample_pattern_lut;

  /* ies lights */
  device_vector<float> ies_lights;

  KernelData data;

  DeviceScene(Device *device);
};

/* Scene Parameters */

class SceneParams {
 public:
  ShadingSystem shadingsystem;

  /* Requested BVH layout.
   *
   * If it's not supported by the device, the widest one from supported ones
   * will be used, but BVH wider than this one will never be used.
   */
  BVHLayout bvh_layout;

  BVHType bvh_type;
  bool use_bvh_spatial_split;
  bool use_bvh_unaligned_nodes;
  int num_bvh_time_steps;
  int hair_subdivisions;
  CurveShapeType hair_shape;
  int texture_limit;

  bool background;

  SceneParams()
  {
    shadingsystem = SHADINGSYSTEM_SVM;
    bvh_layout = BVH_LAYOUT_BVH2;
    bvh_type = BVH_TYPE_DYNAMIC;
    use_bvh_spatial_split = false;
    use_bvh_unaligned_nodes = true;
    num_bvh_time_steps = 0;
    hair_subdivisions = 3;
    hair_shape = CURVE_RIBBON;
    texture_limit = 0;
    background = true;
  }

  bool modified(const SceneParams &params) const
  {
    return !(shadingsystem == params.shadingsystem && bvh_layout == params.bvh_layout &&
             bvh_type == params.bvh_type &&
             use_bvh_spatial_split == params.use_bvh_spatial_split &&
             use_bvh_unaligned_nodes == params.use_bvh_unaligned_nodes &&
             num_bvh_time_steps == params.num_bvh_time_steps &&
             hair_subdivisions == params.hair_subdivisions && hair_shape == params.hair_shape &&
             texture_limit == params.texture_limit);
  }

  int curve_subdivisions()
  {
    /* Matching the tessellation rate limit in Embree. */
    return clamp(1 << hair_subdivisions, 1, 16);
  }
};

/* Scene */

class Scene : public NodeOwner {
 public:
  /* Optional name. Is used for logging and reporting. */
  string name;

  /* data */
  BVH *bvh;
  Camera *camera;
  Camera *dicing_camera;
  LookupTables *lookup_tables;
  Film *film;
  Background *background;
  Integrator *integrator;

  /* data lists */
  vector<Object *> objects;
  vector<Geometry *> geometry;
  vector<Shader *> shaders;
  vector<Light *> lights;
  vector<ParticleSystem *> particle_systems;
  vector<Pass *> passes;
  vector<Procedural *> procedurals;

  /* data managers */
  ImageManager *image_manager;
  LightManager *light_manager;
  ShaderManager *shader_manager;
  GeometryManager *geometry_manager;
  ObjectManager *object_manager;
  ParticleSystemManager *particle_system_manager;
  BakeManager *bake_manager;
  ProceduralManager *procedural_manager;

  /* default shaders */
  Shader *default_surface;
  Shader *default_volume;
  Shader *default_light;
  Shader *default_background;
  Shader *default_empty;

  /* device */
  Device *device;
  DeviceScene dscene;

  /* parameters */
  SceneParams params;

  /* mutex must be locked manually by callers */
  thread_mutex mutex;

  /* scene update statistics */
  SceneUpdateStats *update_stats;

  Scene(const SceneParams &params, Device *device);
  ~Scene();

  void host_update(Progress &progress);

  void device_update(Device *device, Progress &progress);

  bool need_global_attribute(AttributeStandard std);
  void need_global_attributes(AttributeRequestSet &attributes);

  enum MotionType { MOTION_NONE = 0, MOTION_PASS, MOTION_BLUR };
  MotionType need_motion();
  float motion_shutter_time();

  bool need_update();
  bool need_reset();

  void reset();
  void device_free();

  void collect_statistics(RenderStats *stats);

  void enable_update_stats();

  void update_kernel_features();
  bool update(Progress &progress);

  bool has_shadow_catcher();
  void tag_shadow_catcher_modified();

  /* This function is used to create a node of a specified type instead of
   * calling 'new', and sets the scene as the owner of the node.
   * The function has overloads that will also add the created node to the right
   * node array (e.g. Scene::geometry for Geometry nodes) and tag the appropriate
   * manager for an update.
   */
  template<typename T, typename... Args> T *create_node(Args &&...args)
  {
    T *node = new T(args...);
    node->set_owner(this);
    return node;
  }

  /* This function is used to delete a node from the scene instead of calling 'delete'
   * and manually removing the node from the data array. It also tags the
   * appropriate manager for an update, if any, and checks that the scene is indeed
   * the owner of the node. Calling this function on a node not owned by the scene
   * will likely cause a crash which we want in order to detect such cases.
   */
  template<typename T> void delete_node(T *node)
  {
    assert(node->get_owner() == this);
    delete_node_impl(node);
  }

  /* Same as above, but specify the actual owner.
   */
  template<typename T> void delete_node(T *node, const NodeOwner *owner)
  {
    assert(node->get_owner() == owner);
    delete_node_impl(node);
    (void)owner;
  }

  /* Remove all nodes in the set from the appropriate data arrays, and tag the
   * specific managers for an update. This assumes that the scene owns the nodes.
   */
  template<typename T> void delete_nodes(const set<T *> &nodes)
  {
    delete_nodes(nodes, this);
  }

  /* Same as above, but specify the actual owner of all the nodes in the set.
   */
  template<typename T> void delete_nodes(const set<T *> &nodes, const NodeOwner *owner);

 protected:
  /* Check if some heavy data worth logging was updated.
   * Mainly used to suppress extra annoying logging.
   */
  bool need_data_update();

  void free_memory(bool final);

  bool kernels_loaded;
  uint loaded_kernel_features;

  bool load_kernels(Progress &progress, bool lock_scene = true);

  bool has_shadow_catcher_ = false;
  bool shadow_catcher_modified_ = true;

  /* Maximum number of closure during session lifetime. */
  int max_closure_global;

  /* Get maximum number of closures to be used in kernel. */
  int get_max_closure_count();

  /* Get size of a volume stack needed to render this scene.  */
  int get_volume_stack_size() const;

  template<typename T> void delete_node_impl(T *node)
  {
    delete node;
  }
};

template<> Light *Scene::create_node<Light>();

template<> Mesh *Scene::create_node<Mesh>();

template<> Object *Scene::create_node<Object>();

template<> Hair *Scene::create_node<Hair>();

template<> Volume *Scene::create_node<Volume>();

template<> ParticleSystem *Scene::create_node<ParticleSystem>();

template<> Shader *Scene::create_node<Shader>();

template<> AlembicProcedural *Scene::create_node<AlembicProcedural>();

template<> Pass *Scene::create_node<Pass>();

template<> void Scene::delete_node_impl(Light *node);

template<> void Scene::delete_node_impl(Mesh *node);

template<> void Scene::delete_node_impl(Volume *node);

template<> void Scene::delete_node_impl(Hair *node);

template<> void Scene::delete_node_impl(Geometry *node);

template<> void Scene::delete_node_impl(Object *node);

template<> void Scene::delete_node_impl(ParticleSystem *node);

template<> void Scene::delete_node_impl(Shader *node);

template<> void Scene::delete_node_impl(Procedural *node);

template<> void Scene::delete_node_impl(AlembicProcedural *node);

template<> void Scene::delete_node_impl(Pass *node);

template<> void Scene::delete_nodes(const set<Light *> &nodes, const NodeOwner *owner);

template<> void Scene::delete_nodes(const set<Geometry *> &nodes, const NodeOwner *owner);

template<> void Scene::delete_nodes(const set<Object *> &nodes, const NodeOwner *owner);

template<> void Scene::delete_nodes(const set<ParticleSystem *> &nodes, const NodeOwner *owner);

template<> void Scene::delete_nodes(const set<Shader *> &nodes, const NodeOwner *owner);

template<> void Scene::delete_nodes(const set<Procedural *> &nodes, const NodeOwner *owner);

template<> void Scene::delete_nodes(const set<Pass *> &nodes, const NodeOwner *owner);

CCL_NAMESPACE_END

#endif /*  __SCENE_H__ */
