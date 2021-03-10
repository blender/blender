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

#include "device/device.h"

#include "render/alembic.h"
#include "render/background.h"
#include "render/camera.h"
#include "render/colorspace.h"
#include "render/graph.h"
#include "render/integrator.h"
#include "render/light.h"
#include "render/mesh.h"
#include "render/nodes.h"
#include "render/object.h"
#include "render/osl.h"
#include "render/scene.h"
#include "render/shader.h"
#include "render/svm.h"
#include "render/tables.h"

#include "util/util_foreach.h"
#include "util/util_murmurhash.h"
#include "util/util_task.h"
#include "util/util_transform.h"

#ifdef WITH_OCIO
#  include <OpenColorIO/OpenColorIO.h>
namespace OCIO = OCIO_NAMESPACE;
#endif

CCL_NAMESPACE_BEGIN

thread_mutex ShaderManager::lookup_table_mutex;
vector<float> ShaderManager::beckmann_table;
bool ShaderManager::beckmann_table_ready = false;

/* Beckmann sampling precomputed table, see bsdf_microfacet.h */

/* 2D slope distribution (alpha = 1.0) */
static float beckmann_table_P22(const float slope_x, const float slope_y)
{
  return expf(-(slope_x * slope_x + slope_y * slope_y));
}

/* maximal slope amplitude (range that contains 99.99% of the distribution) */
static float beckmann_table_slope_max()
{
  return 6.0;
}

/* MSVC 2015 needs this ugly hack to prevent a codegen bug on x86
 * see T50176 for details
 */
#if defined(_MSC_VER) && (_MSC_VER == 1900)
#  define MSVC_VOLATILE volatile
#else
#  define MSVC_VOLATILE
#endif

/* Paper used: Importance Sampling Microfacet-Based BSDFs with the
 * Distribution of Visible Normals. Supplemental Material 2/2.
 *
 * http://hal.inria.fr/docs/01/00/66/20/ANNEX/supplemental2.pdf
 */
static void beckmann_table_rows(float *table, int row_from, int row_to)
{
  /* allocate temporary data */
  const int DATA_TMP_SIZE = 512;
  vector<double> slope_x(DATA_TMP_SIZE);
  vector<double> CDF_P22_omega_i(DATA_TMP_SIZE);

  /* loop over incident directions */
  for (int index_theta = row_from; index_theta < row_to; index_theta++) {
    /* incident vector */
    const float cos_theta = index_theta / (BECKMANN_TABLE_SIZE - 1.0f);
    const float sin_theta = safe_sqrtf(1.0f - cos_theta * cos_theta);

    /* for a given incident vector
     * integrate P22_{omega_i}(x_slope, 1, 1), Eq. (10) */
    slope_x[0] = (double)-beckmann_table_slope_max();
    CDF_P22_omega_i[0] = 0;

    for (MSVC_VOLATILE int index_slope_x = 1; index_slope_x < DATA_TMP_SIZE; ++index_slope_x) {
      /* slope_x */
      slope_x[index_slope_x] = (double)(-beckmann_table_slope_max() +
                                        2.0f * beckmann_table_slope_max() * index_slope_x /
                                            (DATA_TMP_SIZE - 1.0f));

      /* dot product with incident vector */
      float dot_product = fmaxf(0.0f, -(float)slope_x[index_slope_x] * sin_theta + cos_theta);
      /* marginalize P22_{omega_i}(x_slope, 1, 1), Eq. (10) */
      float P22_omega_i = 0.0f;

      for (int j = 0; j < 100; ++j) {
        float slope_y = -beckmann_table_slope_max() +
                        2.0f * beckmann_table_slope_max() * j * (1.0f / 99.0f);
        P22_omega_i += dot_product * beckmann_table_P22((float)slope_x[index_slope_x], slope_y);
      }

      /* CDF of P22_{omega_i}(x_slope, 1, 1), Eq. (10) */
      CDF_P22_omega_i[index_slope_x] = CDF_P22_omega_i[index_slope_x - 1] + (double)P22_omega_i;
    }

    /* renormalize CDF_P22_omega_i */
    for (int index_slope_x = 1; index_slope_x < DATA_TMP_SIZE; ++index_slope_x)
      CDF_P22_omega_i[index_slope_x] /= CDF_P22_omega_i[DATA_TMP_SIZE - 1];

    /* loop over random number U1 */
    int index_slope_x = 0;

    for (int index_U = 0; index_U < BECKMANN_TABLE_SIZE; ++index_U) {
      const double U = 0.0000001 + 0.9999998 * index_U / (double)(BECKMANN_TABLE_SIZE - 1);

      /* inverse CDF_P22_omega_i, solve Eq.(11) */
      while (CDF_P22_omega_i[index_slope_x] <= U)
        ++index_slope_x;

      const double interp = (CDF_P22_omega_i[index_slope_x] - U) /
                            (CDF_P22_omega_i[index_slope_x] - CDF_P22_omega_i[index_slope_x - 1]);

      /* store value */
      table[index_U + index_theta * BECKMANN_TABLE_SIZE] =
          (float)(interp * slope_x[index_slope_x - 1] + (1.0 - interp) * slope_x[index_slope_x]);
    }
  }
}

#undef MSVC_VOLATILE

static void beckmann_table_build(vector<float> &table)
{
  table.resize(BECKMANN_TABLE_SIZE * BECKMANN_TABLE_SIZE);

  /* multithreaded build */
  TaskPool pool;

  for (int i = 0; i < BECKMANN_TABLE_SIZE; i += 8)
    pool.push(function_bind(&beckmann_table_rows, &table[0], i, i + 8));

  pool.wait_work();
}

/* Shader */

NODE_DEFINE(Shader)
{
  NodeType *type = NodeType::add("shader", create);

  SOCKET_BOOLEAN(use_mis, "Use MIS", true);
  SOCKET_BOOLEAN(use_transparent_shadow, "Use Transparent Shadow", true);
  SOCKET_BOOLEAN(heterogeneous_volume, "Heterogeneous Volume", true);

  static NodeEnum volume_sampling_method_enum;
  volume_sampling_method_enum.insert("distance", VOLUME_SAMPLING_DISTANCE);
  volume_sampling_method_enum.insert("equiangular", VOLUME_SAMPLING_EQUIANGULAR);
  volume_sampling_method_enum.insert("multiple_importance", VOLUME_SAMPLING_MULTIPLE_IMPORTANCE);
  SOCKET_ENUM(volume_sampling_method,
              "Volume Sampling Method",
              volume_sampling_method_enum,
              VOLUME_SAMPLING_MULTIPLE_IMPORTANCE);

  static NodeEnum volume_interpolation_method_enum;
  volume_interpolation_method_enum.insert("linear", VOLUME_INTERPOLATION_LINEAR);
  volume_interpolation_method_enum.insert("cubic", VOLUME_INTERPOLATION_CUBIC);
  SOCKET_ENUM(volume_interpolation_method,
              "Volume Interpolation Method",
              volume_interpolation_method_enum,
              VOLUME_INTERPOLATION_LINEAR);

  SOCKET_FLOAT(volume_step_rate, "Volume Step Rate", 1.0f);

  static NodeEnum displacement_method_enum;
  displacement_method_enum.insert("bump", DISPLACE_BUMP);
  displacement_method_enum.insert("true", DISPLACE_TRUE);
  displacement_method_enum.insert("both", DISPLACE_BOTH);
  SOCKET_ENUM(displacement_method, "Displacement Method", displacement_method_enum, DISPLACE_BUMP);

  SOCKET_INT(pass_id, "Pass ID", 0);

  return type;
}

Shader::Shader() : Node(node_type)
{
  pass_id = 0;

  graph = NULL;

  has_surface = false;
  has_surface_transparent = false;
  has_surface_emission = false;
  has_surface_bssrdf = false;
  has_volume = false;
  has_displacement = false;
  has_bump = false;
  has_bssrdf_bump = false;
  has_surface_spatial_varying = false;
  has_volume_spatial_varying = false;
  has_volume_attribute_dependency = false;
  has_integrator_dependency = false;
  has_volume_connected = false;
  prev_volume_step_rate = 0.0f;

  displacement_method = DISPLACE_BUMP;

  id = -1;
  used = false;

  need_update_uvs = true;
  need_update_attribute = true;
  need_update_displacement = true;
}

Shader::~Shader()
{
  delete graph;
}

bool Shader::is_constant_emission(float3 *emission)
{
  /* If the shader has AOVs, they need to be evaluated, so we can't skip the shader. */
  foreach (ShaderNode *node, graph->nodes) {
    if (node->special_type == SHADER_SPECIAL_TYPE_OUTPUT_AOV) {
      return false;
    }
  }

  ShaderInput *surf = graph->output()->input("Surface");

  if (surf->link == NULL) {
    return false;
  }

  if (surf->link->parent->type == EmissionNode::node_type) {
    EmissionNode *node = (EmissionNode *)surf->link->parent;

    assert(node->input("Color"));
    assert(node->input("Strength"));

    if (node->input("Color")->link || node->input("Strength")->link) {
      return false;
    }

    *emission = node->get_color() * node->get_strength();
  }
  else if (surf->link->parent->type == BackgroundNode::node_type) {
    BackgroundNode *node = (BackgroundNode *)surf->link->parent;

    assert(node->input("Color"));
    assert(node->input("Strength"));

    if (node->input("Color")->link || node->input("Strength")->link) {
      return false;
    }

    *emission = node->get_color() * node->get_strength();
  }
  else {
    return false;
  }

  return true;
}

void Shader::set_graph(ShaderGraph *graph_)
{
  /* do this here already so that we can detect if mesh or object attributes
   * are needed, since the node attribute callbacks check if their sockets
   * are connected but proxy nodes should not count */
  if (graph_) {
    graph_->remove_proxy_nodes();

    if (displacement_method != DISPLACE_BUMP) {
      graph_->compute_displacement_hash();
    }
  }

  /* update geometry if displacement changed */
  if (displacement_method != DISPLACE_BUMP) {
    const char *old_hash = (graph) ? graph->displacement_hash.c_str() : "";
    const char *new_hash = (graph_) ? graph_->displacement_hash.c_str() : "";

    if (strcmp(old_hash, new_hash) != 0) {
      need_update_displacement = true;
    }
  }

  /* assign graph */
  delete graph;
  graph = graph_;

  /* Store info here before graph optimization to make sure that
   * nodes that get optimized away still count. */
  has_volume_connected = (graph->output()->input("Volume")->link != NULL);
}

void Shader::tag_update(Scene *scene)
{
  /* update tag */
  tag_modified();

  scene->shader_manager->tag_update(scene, ShaderManager::SHADER_MODIFIED);

  /* if the shader previously was emissive, update light distribution,
   * if the new shader is emissive, a light manager update tag will be
   * done in the shader manager device update. */
  if (use_mis && has_surface_emission)
    scene->light_manager->tag_update(scene, LightManager::SHADER_MODIFIED);

  /* Special handle of background MIS light for now: for some reason it
   * has use_mis set to false. We are quite close to release now, so
   * better to be safe.
   */
  if (this == scene->background->get_shader(scene)) {
    scene->light_manager->need_update_background = true;
    if (scene->light_manager->has_background_light(scene)) {
      scene->light_manager->tag_update(scene, LightManager::SHADER_MODIFIED);
    }
  }

  /* quick detection of which kind of shaders we have to avoid loading
   * e.g. surface attributes when there is only a volume shader. this could
   * be more fine grained but it's better than nothing */
  OutputNode *output = graph->output();
  bool prev_has_volume = has_volume;
  has_surface = has_surface || output->input("Surface")->link;
  has_volume = has_volume || output->input("Volume")->link;
  has_displacement = has_displacement || output->input("Displacement")->link;

  /* get requested attributes. this could be optimized by pruning unused
   * nodes here already, but that's the job of the shader manager currently,
   * and may not be so great for interactive rendering where you temporarily
   * disconnect a node */

  AttributeRequestSet prev_attributes = attributes;

  attributes.clear();
  foreach (ShaderNode *node, graph->nodes)
    node->attributes(this, &attributes);

  if (has_displacement) {
    if (displacement_method == DISPLACE_BOTH) {
      attributes.add(ATTR_STD_POSITION_UNDISPLACED);
    }
    if (displacement_method_is_modified()) {
      need_update_displacement = true;
      scene->geometry_manager->tag_update(scene, GeometryManager::SHADER_DISPLACEMENT_MODIFIED);
      scene->object_manager->need_flags_update = true;
    }
  }

  /* compare if the attributes changed, mesh manager will check
   * need_update_attribute, update the relevant meshes and clear it. */
  if (attributes.modified(prev_attributes)) {
    need_update_attribute = true;
    scene->geometry_manager->tag_update(scene, GeometryManager::SHADER_ATTRIBUTE_MODIFIED);
    scene->procedural_manager->tag_update();
  }

  if (has_volume != prev_has_volume || volume_step_rate != prev_volume_step_rate) {
    scene->geometry_manager->need_flags_update = true;
    scene->object_manager->need_flags_update = true;
    prev_volume_step_rate = volume_step_rate;
  }
}

void Shader::tag_used(Scene *scene)
{
  /* if an unused shader suddenly gets used somewhere, it needs to be
   * recompiled because it was skipped for compilation before */
  if (!used) {
    tag_modified();
    scene->shader_manager->tag_update(scene, ShaderManager::SHADER_MODIFIED);
  }
}

bool Shader::need_update_geometry() const
{
  return need_update_uvs || need_update_attribute || need_update_displacement;
}

/* Shader Manager */

ShaderManager::ShaderManager()
{
  update_flags = UPDATE_ALL;
  beckmann_table_offset = TABLE_OFFSET_INVALID;

  init_xyz_transforms();
}

ShaderManager::~ShaderManager()
{
}

ShaderManager *ShaderManager::create(int shadingsystem)
{
  ShaderManager *manager;

  (void)shadingsystem; /* Ignored when built without OSL. */

#ifdef WITH_OSL
  if (shadingsystem == SHADINGSYSTEM_OSL) {
    manager = new OSLShaderManager();
  }
  else
#endif
  {
    manager = new SVMShaderManager();
  }

  return manager;
}

uint ShaderManager::get_attribute_id(ustring name)
{
  thread_scoped_spin_lock lock(attribute_lock_);

  /* get a unique id for each name, for SVM attribute lookup */
  AttributeIDMap::iterator it = unique_attribute_id.find(name);

  if (it != unique_attribute_id.end())
    return it->second;

  uint id = (uint)ATTR_STD_NUM + unique_attribute_id.size();
  unique_attribute_id[name] = id;
  return id;
}

uint ShaderManager::get_attribute_id(AttributeStandard std)
{
  return (uint)std;
}

int ShaderManager::get_shader_id(Shader *shader, bool smooth)
{
  /* get a shader id to pass to the kernel */
  int id = shader->id;

  /* smooth flag */
  if (smooth)
    id |= SHADER_SMOOTH_NORMAL;

  /* default flags */
  id |= SHADER_CAST_SHADOW | SHADER_AREA_LIGHT;

  return id;
}

void ShaderManager::update_shaders_used(Scene *scene)
{
  if (!need_update()) {
    return;
  }

  /* figure out which shaders are in use, so SVM/OSL can skip compiling them
   * for speed and avoid loading image textures into memory */
  uint id = 0;
  foreach (Shader *shader, scene->shaders) {
    shader->used = false;
    shader->id = id++;
  }

  scene->default_surface->used = true;
  scene->default_light->used = true;
  scene->default_background->used = true;
  scene->default_empty->used = true;

  if (scene->background->get_shader())
    scene->background->get_shader()->used = true;

#ifdef WITH_ALEMBIC
  foreach (Procedural *procedural, scene->procedurals) {
    AlembicProcedural *abc_proc = static_cast<AlembicProcedural *>(procedural);

    foreach (Node *abc_node, abc_proc->get_objects()) {
      AlembicObject *abc_object = static_cast<AlembicObject *>(abc_node);

      foreach (Node *node, abc_object->get_used_shaders()) {
        Shader *shader = static_cast<Shader *>(node);
        shader->used = true;
      }
    }
  }
#endif

  foreach (Geometry *geom, scene->geometry)
    foreach (Node *node, geom->get_used_shaders()) {
      Shader *shader = static_cast<Shader *>(node);
      shader->used = true;
    }

  foreach (Light *light, scene->lights)
    if (light->get_shader())
      const_cast<Shader *>(light->get_shader())->used = true;
}

void ShaderManager::device_update_common(Device *device,
                                         DeviceScene *dscene,
                                         Scene *scene,
                                         Progress & /*progress*/)
{
  dscene->shaders.free();

  if (scene->shaders.size() == 0)
    return;

  KernelShader *kshader = dscene->shaders.alloc(scene->shaders.size());
  bool has_volumes = false;
  bool has_transparent_shadow = false;

  foreach (Shader *shader, scene->shaders) {
    uint flag = 0;

    if (shader->get_use_mis())
      flag |= SD_USE_MIS;
    if (shader->has_surface_transparent && shader->get_use_transparent_shadow())
      flag |= SD_HAS_TRANSPARENT_SHADOW;
    if (shader->has_volume) {
      flag |= SD_HAS_VOLUME;
      has_volumes = true;

      /* todo: this could check more fine grained, to skip useless volumes
       * enclosed inside an opaque bsdf.
       */
      flag |= SD_HAS_TRANSPARENT_SHADOW;
    }
    /* in this case we can assume transparent surface */
    if (shader->has_volume_connected && !shader->has_surface)
      flag |= SD_HAS_ONLY_VOLUME;
    if (shader->has_volume) {
      if (shader->get_heterogeneous_volume() && shader->has_volume_spatial_varying)
        flag |= SD_HETEROGENEOUS_VOLUME;
    }
    if (shader->has_volume_attribute_dependency)
      flag |= SD_NEED_VOLUME_ATTRIBUTES;
    if (shader->has_bssrdf_bump)
      flag |= SD_HAS_BSSRDF_BUMP;
    if (device->info.has_volume_decoupled) {
      if (shader->get_volume_sampling_method() == VOLUME_SAMPLING_EQUIANGULAR)
        flag |= SD_VOLUME_EQUIANGULAR;
      if (shader->get_volume_sampling_method() == VOLUME_SAMPLING_MULTIPLE_IMPORTANCE)
        flag |= SD_VOLUME_MIS;
    }
    if (shader->get_volume_interpolation_method() == VOLUME_INTERPOLATION_CUBIC)
      flag |= SD_VOLUME_CUBIC;
    if (shader->has_bump)
      flag |= SD_HAS_BUMP;
    if (shader->get_displacement_method() != DISPLACE_BUMP)
      flag |= SD_HAS_DISPLACEMENT;

    /* constant emission check */
    float3 constant_emission = zero_float3();
    if (shader->is_constant_emission(&constant_emission))
      flag |= SD_HAS_CONSTANT_EMISSION;

    uint32_t cryptomatte_id = util_murmur_hash3(shader->name.c_str(), shader->name.length(), 0);

    /* regular shader */
    kshader->flags = flag;
    kshader->pass_id = shader->get_pass_id();
    kshader->constant_emission[0] = constant_emission.x;
    kshader->constant_emission[1] = constant_emission.y;
    kshader->constant_emission[2] = constant_emission.z;
    kshader->cryptomatte_id = util_hash_to_float(cryptomatte_id);
    kshader++;

    has_transparent_shadow |= (flag & SD_HAS_TRANSPARENT_SHADOW) != 0;
  }

  dscene->shaders.copy_to_device();

  /* lookup tables */
  KernelTables *ktables = &dscene->data.tables;

  /* beckmann lookup table */
  if (beckmann_table_offset == TABLE_OFFSET_INVALID) {
    if (!beckmann_table_ready) {
      thread_scoped_lock lock(lookup_table_mutex);
      if (!beckmann_table_ready) {
        beckmann_table_build(beckmann_table);
        beckmann_table_ready = true;
      }
    }
    beckmann_table_offset = scene->lookup_tables->add_table(dscene, beckmann_table);
  }
  ktables->beckmann_offset = (int)beckmann_table_offset;

  /* integrator */
  KernelIntegrator *kintegrator = &dscene->data.integrator;
  kintegrator->use_volumes = has_volumes;
  /* TODO(sergey): De-duplicate with flags set in integrator.cpp. */
  kintegrator->transparent_shadows = has_transparent_shadow;

  /* film */
  KernelFilm *kfilm = &dscene->data.film;
  /* color space, needs to be here because e.g. displacement shaders could depend on it */
  kfilm->xyz_to_r = float3_to_float4(xyz_to_r);
  kfilm->xyz_to_g = float3_to_float4(xyz_to_g);
  kfilm->xyz_to_b = float3_to_float4(xyz_to_b);
  kfilm->rgb_to_y = float3_to_float4(rgb_to_y);
}

void ShaderManager::device_free_common(Device *, DeviceScene *dscene, Scene *scene)
{
  scene->lookup_tables->remove_table(&beckmann_table_offset);

  dscene->shaders.free();
}

void ShaderManager::add_default(Scene *scene)
{
  /* default surface */
  {
    ShaderGraph *graph = new ShaderGraph();

    DiffuseBsdfNode *diffuse = graph->create_node<DiffuseBsdfNode>();
    diffuse->set_color(make_float3(0.8f, 0.8f, 0.8f));
    graph->add(diffuse);

    graph->connect(diffuse->output("BSDF"), graph->output()->input("Surface"));

    Shader *shader = scene->create_node<Shader>();
    shader->name = "default_surface";
    shader->set_graph(graph);
    scene->default_surface = shader;
    shader->tag_update(scene);
  }

  /* default volume */
  {
    ShaderGraph *graph = new ShaderGraph();

    PrincipledVolumeNode *principled = graph->create_node<PrincipledVolumeNode>();
    graph->add(principled);

    graph->connect(principled->output("Volume"), graph->output()->input("Volume"));

    Shader *shader = scene->create_node<Shader>();
    shader->name = "default_volume";
    shader->set_graph(graph);
    scene->default_volume = shader;
    shader->tag_update(scene);
  }

  /* default light */
  {
    ShaderGraph *graph = new ShaderGraph();

    EmissionNode *emission = graph->create_node<EmissionNode>();
    emission->set_color(make_float3(0.8f, 0.8f, 0.8f));
    emission->set_strength(0.0f);
    graph->add(emission);

    graph->connect(emission->output("Emission"), graph->output()->input("Surface"));

    Shader *shader = scene->create_node<Shader>();
    shader->name = "default_light";
    shader->set_graph(graph);
    scene->default_light = shader;
    shader->tag_update(scene);
  }

  /* default background */
  {
    ShaderGraph *graph = new ShaderGraph();

    Shader *shader = scene->create_node<Shader>();
    shader->name = "default_background";
    shader->set_graph(graph);
    scene->default_background = shader;
    shader->tag_update(scene);
  }

  /* default empty */
  {
    ShaderGraph *graph = new ShaderGraph();

    Shader *shader = scene->create_node<Shader>();
    shader->name = "default_empty";
    shader->set_graph(graph);
    scene->default_empty = shader;
    shader->tag_update(scene);
  }
}

void ShaderManager::get_requested_graph_features(ShaderGraph *graph,
                                                 DeviceRequestedFeatures *requested_features)
{
  foreach (ShaderNode *node, graph->nodes) {
    requested_features->max_nodes_group = max(requested_features->max_nodes_group,
                                              node->get_group());
    requested_features->nodes_features |= node->get_feature();
    if (node->special_type == SHADER_SPECIAL_TYPE_CLOSURE) {
      BsdfBaseNode *bsdf_node = static_cast<BsdfBaseNode *>(node);
      if (CLOSURE_IS_VOLUME(bsdf_node->get_closure_type())) {
        requested_features->nodes_features |= NODE_FEATURE_VOLUME;
      }
      else if (CLOSURE_IS_PRINCIPLED(bsdf_node->get_closure_type())) {
        requested_features->use_principled = true;
      }
    }
    if (node->has_surface_bssrdf()) {
      requested_features->use_subsurface = true;
    }
    if (node->has_surface_transparent()) {
      requested_features->use_transparent = true;
    }
    if (node->has_raytrace()) {
      requested_features->use_shader_raytrace = true;
    }
  }
}

void ShaderManager::get_requested_features(Scene *scene,
                                           DeviceRequestedFeatures *requested_features)
{
  requested_features->max_nodes_group = NODE_GROUP_LEVEL_0;
  requested_features->nodes_features = 0;
  for (int i = 0; i < scene->shaders.size(); i++) {
    Shader *shader = scene->shaders[i];
    if (!shader->used) {
      continue;
    }

    /* Gather requested features from all the nodes from the graph nodes. */
    get_requested_graph_features(shader->graph, requested_features);
    ShaderNode *output_node = shader->graph->output();
    if (output_node->input("Displacement")->link != NULL) {
      requested_features->nodes_features |= NODE_FEATURE_BUMP;
      if (shader->get_displacement_method() == DISPLACE_BOTH) {
        requested_features->nodes_features |= NODE_FEATURE_BUMP_STATE;
        requested_features->max_nodes_group = max(requested_features->max_nodes_group,
                                                  NODE_GROUP_LEVEL_1);
      }
    }
    /* On top of volume nodes, also check if we need volume sampling because
     * e.g. an Emission node would slip through the NODE_FEATURE_VOLUME check */
    if (shader->has_volume)
      requested_features->use_volume |= true;
  }
}

void ShaderManager::free_memory()
{
  beckmann_table.free_memory();

#ifdef WITH_OSL
  OSLShaderManager::free_memory();
#endif

  ColorSpaceManager::free_memory();
}

float ShaderManager::linear_rgb_to_gray(float3 c)
{
  return dot(c, rgb_to_y);
}

string ShaderManager::get_cryptomatte_materials(Scene *scene)
{
  string manifest = "{";
  unordered_set<ustring, ustringHash> materials;
  foreach (Shader *shader, scene->shaders) {
    if (materials.count(shader->name)) {
      continue;
    }
    materials.insert(shader->name);
    uint32_t cryptomatte_id = util_murmur_hash3(shader->name.c_str(), shader->name.length(), 0);
    manifest += string_printf("\"%s\":\"%08x\",", shader->name.c_str(), cryptomatte_id);
  }
  manifest[manifest.size() - 1] = '}';
  return manifest;
}

void ShaderManager::tag_update(Scene * /*scene*/, uint32_t /*flag*/)
{
  /* update everything for now */
  update_flags = ShaderManager::UPDATE_ALL;
}

bool ShaderManager::need_update() const
{
  return update_flags != UPDATE_NONE;
}

#ifdef WITH_OCIO
static bool to_scene_linear_transform(OCIO::ConstConfigRcPtr &config,
                                      const char *colorspace,
                                      Transform &to_scene_linear)
{
  OCIO::ConstProcessorRcPtr processor;
  try {
    processor = config->getProcessor(OCIO::ROLE_SCENE_LINEAR, colorspace);
  }
  catch (OCIO::Exception &) {
    return false;
  }

  if (!processor) {
    return false;
  }

  OCIO::ConstCPUProcessorRcPtr device_processor = processor->getDefaultCPUProcessor();
  if (!device_processor) {
    return false;
  }

  to_scene_linear = transform_identity();
  device_processor->applyRGB(&to_scene_linear.x.x);
  device_processor->applyRGB(&to_scene_linear.y.x);
  device_processor->applyRGB(&to_scene_linear.z.x);
  to_scene_linear = transform_transposed_inverse(to_scene_linear);
  return true;
}
#endif

void ShaderManager::init_xyz_transforms()
{
  /* Default to ITU-BT.709 in case no appropriate transform found.
   * Note XYZ here is defined as having a D65 white point. */
  xyz_to_r = make_float3(3.2404542f, -1.5371385f, -0.4985314f);
  xyz_to_g = make_float3(-0.9692660f, 1.8760108f, 0.0415560f);
  xyz_to_b = make_float3(0.0556434f, -0.2040259f, 1.0572252f);
  rgb_to_y = make_float3(0.2126729f, 0.7151522f, 0.0721750f);

#ifdef WITH_OCIO
  /* Get from OpenColorO config if it has the required roles. */
  OCIO::ConstConfigRcPtr config = OCIO::GetCurrentConfig();
  if (!(config && config->hasRole(OCIO::ROLE_SCENE_LINEAR))) {
    return;
  }

  Transform xyz_to_rgb;

  if (config->hasRole("aces_interchange")) {
    /* Standard OpenColorIO role, defined as ACES2065-1. */
    const Transform xyz_E_to_aces = make_transform(1.0498110175f,
                                                   0.0f,
                                                   -0.0000974845f,
                                                   0.0f,
                                                   -0.4959030231f,
                                                   1.3733130458f,
                                                   0.0982400361f,
                                                   0.0f,
                                                   0.0f,
                                                   0.0f,
                                                   0.9912520182f,
                                                   0.0f);
    const Transform xyz_D65_to_E = make_transform(
        1.0521111f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.9184170f, 0.0f);

    Transform aces_to_rgb;
    if (!to_scene_linear_transform(config, "aces_interchange", aces_to_rgb)) {
      return;
    }

    xyz_to_rgb = aces_to_rgb * xyz_E_to_aces * xyz_D65_to_E;
  }
  else if (config->hasRole("XYZ")) {
    /* Custom role used before the standard existed. */
    if (!to_scene_linear_transform(config, "XYZ", xyz_to_rgb)) {
      return;
    }
  }
  else {
    /* No reference role found to determine XYZ. */
    return;
  }

  xyz_to_r = float4_to_float3(xyz_to_rgb.x);
  xyz_to_g = float4_to_float3(xyz_to_rgb.y);
  xyz_to_b = float4_to_float3(xyz_to_rgb.z);

  const Transform rgb_to_xyz = transform_inverse(xyz_to_rgb);
  rgb_to_y = float4_to_float3(rgb_to_xyz.y);
#endif
}

CCL_NAMESPACE_END
