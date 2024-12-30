/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "bvh/params.h"

#include "scene/devicescene.h"
#include "scene/film.h"
#include "scene/image.h"
#include "scene/shader.h"

#include "util/param.h"
#include "util/string.h"
#include "util/thread.h"
#include "util/unique_ptr.h"
#include "util/unique_ptr_vector.h"

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
class PointCloud;
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
  bool use_bvh_compact_structure;
  bool use_bvh_unaligned_nodes;
  int num_bvh_time_steps;
  int hair_subdivisions;
  CurveShapeType hair_shape;
  int texture_limit;

  bool background;

  SceneParams()
  {
    shadingsystem = SHADINGSYSTEM_SVM;
    bvh_layout = BVH_LAYOUT_AUTO;
    bvh_type = BVH_TYPE_DYNAMIC;
    use_bvh_spatial_split = false;
    use_bvh_compact_structure = true;
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
             use_bvh_compact_structure == params.use_bvh_compact_structure &&
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

  /* Maps from Light group names to their pass ID. */
  map<ustring, int> lightgroups;

  /* data */
  unique_ptr<BVH> bvh;
  unique_ptr<LookupTables> lookup_tables;

  Camera *camera;
  Camera *dicing_camera;
  Film *film;
  Background *background;
  Integrator *integrator;

  /* data lists */
  unique_ptr_vector<Background> backgrounds;
  unique_ptr_vector<Film> films;
  unique_ptr_vector<Integrator> integrators;
  unique_ptr_vector<Camera> cameras;
  unique_ptr_vector<Shader> shaders;
  unique_ptr_vector<Pass> passes;
  unique_ptr_vector<ParticleSystem> particle_systems;
  unique_ptr_vector<Light> lights;
  unique_ptr_vector<Geometry> geometry;
  unique_ptr_vector<Object> objects;
  unique_ptr_vector<Procedural> procedurals;

  /* data managers */
  unique_ptr<ImageManager> image_manager;
  unique_ptr<LightManager> light_manager;
  unique_ptr<ShaderManager> shader_manager;
  unique_ptr<GeometryManager> geometry_manager;
  unique_ptr<ObjectManager> object_manager;
  unique_ptr<ParticleSystemManager> particle_system_manager;
  unique_ptr<BakeManager> bake_manager;
  unique_ptr<ProceduralManager> procedural_manager;

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
  unique_ptr<SceneUpdateStats> update_stats;

  Scene(const SceneParams &params, Device *device);
  ~Scene() override;

  void device_update(Device *device, Progress &progress);

  bool need_global_attribute(AttributeStandard std);
  void need_global_attributes(AttributeRequestSet &attributes);

  enum MotionType { MOTION_NONE = 0, MOTION_PASS, MOTION_BLUR };
  MotionType need_motion() const;
  float motion_shutter_time();

  bool need_update();
  bool need_reset(const bool check_camera = true);

  void reset();
  void device_free();

  void collect_statistics(RenderStats *stats);

  void enable_update_stats();

  bool load_kernels(Progress &progress);
  bool update(Progress &progress);

  bool has_shadow_catcher();
  void tag_shadow_catcher_modified();

  /* This function is used to create a node of a specified type instead of
   * calling 'new', and sets the scene as the owner of the node.
   * The function has overloads that will also add the created node to the right
   * node array (e.g. Scene::geometry for Geometry nodes) and tag the appropriate
   * manager for an update.
   */
  template<typename T, typename... Args> T *create_node(Args &&.../*args*/) = delete;

  /* This function is used to delete a node from the scene instead of calling 'delete'
   * and manually removing the node from the data array. It also tags the
   * appropriate manager for an update, if any, and checks that the scene is indeed
   * the owner of the node. Calling this function on a node not owned by the scene
   * will likely cause a crash which we want in order to detect such cases.
   */
  template<typename T> void delete_node(T *node) = delete;

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

  void update_kernel_features();

  bool has_shadow_catcher_ = false;
  bool shadow_catcher_modified_ = true;

  /* Maximum number of closure during session lifetime. */
  int max_closure_global;

  /* Get maximum number of closures to be used in kernel. */
  int get_max_closure_count();

  /* Get size of a volume stack needed to render this scene. */
  int get_volume_stack_size() const;
};

template<> Light *Scene::create_node<Light>();
template<> Mesh *Scene::create_node<Mesh>();
template<> Object *Scene::create_node<Object>();
template<> Hair *Scene::create_node<Hair>();
template<> Volume *Scene::create_node<Volume>();
template<> PointCloud *Scene::create_node<PointCloud>();
template<> ParticleSystem *Scene::create_node<ParticleSystem>();
template<> Shader *Scene::create_node<Shader>();
template<> AlembicProcedural *Scene::create_node<AlembicProcedural>();
template<> Pass *Scene::create_node<Pass>();
template<> Camera *Scene::create_node<Camera>();
template<> Background *Scene::create_node<Background>();
template<> Film *Scene::create_node<Film>();
template<> Integrator *Scene::create_node<Integrator>();

template<> void Scene::delete_node(Light *node);
template<> void Scene::delete_node(Mesh *node);
template<> void Scene::delete_node(Volume *node);
template<> void Scene::delete_node(PointCloud *node);
template<> void Scene::delete_node(Hair *node);
template<> void Scene::delete_node(Geometry *node);
template<> void Scene::delete_node(Object *node);
template<> void Scene::delete_node(ParticleSystem *node);
template<> void Scene::delete_node(Shader *node);
template<> void Scene::delete_node(Procedural *node);
template<> void Scene::delete_node(AlembicProcedural *node);
template<> void Scene::delete_node(Pass *node);

template<> void Scene::delete_nodes(const set<Light *> &nodes, const NodeOwner *owner);
template<> void Scene::delete_nodes(const set<Geometry *> &nodes, const NodeOwner *owner);
template<> void Scene::delete_nodes(const set<Object *> &nodes, const NodeOwner *owner);
template<> void Scene::delete_nodes(const set<ParticleSystem *> &nodes, const NodeOwner *owner);
template<> void Scene::delete_nodes(const set<Shader *> &nodes, const NodeOwner *owner);
template<> void Scene::delete_nodes(const set<Procedural *> &nodes, const NodeOwner *owner);
template<> void Scene::delete_nodes(const set<Pass *> &nodes, const NodeOwner *owner);

CCL_NAMESPACE_END
