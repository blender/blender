/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "device/device.h"

#include "scene/background.h"
#include "scene/camera.h"
#include "scene/colorspace.h"
#include "scene/integrator.h"
#include "scene/light.h"
#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/osl.h"
#include "scene/procedural.h"
#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"
#include "scene/svm.h"
#include "scene/tables.h"

#include "util/foreach.h"
#include "util/murmurhash.h"
#include "util/task.h"
#include "util/transform.h"

#ifdef WITH_OCIO
#  include <OpenColorIO/OpenColorIO.h>
namespace OCIO = OCIO_NAMESPACE;
#endif

#include "scene/shader.tables"

CCL_NAMESPACE_BEGIN

thread_mutex ShaderManager::lookup_table_mutex;

/* Shader */

NODE_DEFINE(Shader)
{
  NodeType *type = NodeType::add("shader", create);

  static NodeEnum emission_sampling_method_enum;
  emission_sampling_method_enum.insert("none", EMISSION_SAMPLING_NONE);
  emission_sampling_method_enum.insert("auto", EMISSION_SAMPLING_AUTO);
  emission_sampling_method_enum.insert("front", EMISSION_SAMPLING_FRONT);
  emission_sampling_method_enum.insert("back", EMISSION_SAMPLING_BACK);
  emission_sampling_method_enum.insert("front_back", EMISSION_SAMPLING_FRONT_BACK);
  SOCKET_ENUM(emission_sampling_method,
              "Emission Sampling Method",
              emission_sampling_method_enum,
              EMISSION_SAMPLING_AUTO);

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

Shader::Shader() : Node(get_node_type())
{
  pass_id = 0;

  graph = NULL;

  has_surface = false;
  has_surface_transparent = false;
  has_surface_raytrace = false;
  has_surface_bssrdf = false;
  has_volume = false;
  has_displacement = false;
  has_bump = false;
  has_bssrdf_bump = false;
  has_surface_spatial_varying = false;
  has_volume_spatial_varying = false;
  has_volume_attribute_dependency = false;
  has_volume_connected = false;
  prev_volume_step_rate = 0.0f;

  emission_estimate = zero_float3();
  emission_sampling = EMISSION_SAMPLING_NONE;
  emission_is_constant = true;

  displacement_method = DISPLACE_BUMP;

  id = -1;

  need_update_uvs = true;
  need_update_attribute = true;
  need_update_displacement = true;
}

Shader::~Shader()
{
  delete graph;
}

static float3 output_estimate_emission(ShaderOutput *output, bool &is_constant)
{
  /* Only supports a few nodes for now, not arbitrary shader graphs. */
  ShaderNode *node = (output) ? output->parent : nullptr;

  if (node == nullptr) {
    return zero_float3();
  }
  else if (node->type == EmissionNode::get_node_type() ||
           node->type == BackgroundNode::get_node_type() ||
           node->type == PrincipledBsdfNode::get_node_type())
  {
    const bool is_principled = (node->type == PrincipledBsdfNode::get_node_type());
    /* Emission and Background node. */
    ShaderInput *color_in = node->input(is_principled ? "Emission" : "Color");
    ShaderInput *strength_in = node->input(is_principled ? "Emission Strength" : "Strength");

    if (is_principled) {
      /* Too many parameters (coat, sheen, alpha) influence Emission for the Principled BSDF. */
      is_constant = false;
    }

    float3 estimate = one_float3();

    if (color_in->link) {
      is_constant = false;
    }
    else {
      estimate *= node->get_float3(color_in->socket_type);
    }

    if (strength_in->link) {
      is_constant = false;
      estimate *= output_estimate_emission(strength_in->link, is_constant);
    }
    else {
      estimate *= node->get_float(strength_in->socket_type);
    }

    /* Lower importance of emission nodes from automatic value/color to shader
     * conversion, as these are likely used for previewing and can be slow to
     * build a light tree for on dense meshes. */
    if (node->type == EmissionNode::get_node_type()) {
      EmissionNode *emission_node = static_cast<EmissionNode *>(node);
      if (emission_node->from_auto_conversion) {
        estimate *= 0.1f;
      }
    }

    return estimate;
  }
  else if (node->type == LightFalloffNode::get_node_type() ||
           node->type == IESLightNode::get_node_type())
  {
    /* Get strength from Light Falloff and IES texture node. */
    ShaderInput *strength_in = node->input("Strength");
    is_constant = false;

    return (strength_in->link) ? output_estimate_emission(strength_in->link, is_constant) :
                                 make_float3(node->get_float(strength_in->socket_type));
  }
  else if (node->type == AddClosureNode::get_node_type()) {
    /* Add Closure. */
    ShaderInput *closure1_in = node->input("Closure1");
    ShaderInput *closure2_in = node->input("Closure2");

    const float3 estimate1 = (closure1_in->link) ?
                                 output_estimate_emission(closure1_in->link, is_constant) :
                                 zero_float3();
    const float3 estimate2 = (closure2_in->link) ?
                                 output_estimate_emission(closure2_in->link, is_constant) :
                                 zero_float3();

    return estimate1 + estimate2;
  }
  else if (node->type == MixClosureNode::get_node_type()) {
    /* Mix Closure. */
    ShaderInput *fac_in = node->input("Fac");
    ShaderInput *closure1_in = node->input("Closure1");
    ShaderInput *closure2_in = node->input("Closure2");

    const float3 estimate1 = (closure1_in->link) ?
                                 output_estimate_emission(closure1_in->link, is_constant) :
                                 zero_float3();
    const float3 estimate2 = (closure2_in->link) ?
                                 output_estimate_emission(closure2_in->link, is_constant) :
                                 zero_float3();

    if (fac_in->link) {
      is_constant = false;
      return estimate1 + estimate2;
    }
    else {
      const float fac = node->get_float(fac_in->socket_type);
      return (1.0f - fac) * estimate1 + fac * estimate2;
    }
  }
  else {
    /* Other nodes, potentially OSL nodes with arbitrary code for which all we can
     * determine is if it has emission or not. */
    const bool has_emission = node->has_surface_emission();
    float3 estimate;

    if (output->type() == SocketType::CLOSURE) {
      if (has_emission) {
        estimate = one_float3();
        is_constant = false;
      }
      else {
        estimate = zero_float3();
      }

      foreach (const ShaderInput *in, node->inputs) {
        if (in->type() == SocketType::CLOSURE && in->link) {
          estimate += output_estimate_emission(in->link, is_constant);
        }
      }
    }
    else {
      estimate = one_float3();
      is_constant = false;
    }

    return estimate;
  }
}

void Shader::estimate_emission()
{
  /* If the shader has AOVs, they need to be evaluated, so we can't skip the shader. */
  emission_is_constant = true;

  foreach (ShaderNode *node, graph->nodes) {
    if (node->special_type == SHADER_SPECIAL_TYPE_OUTPUT_AOV) {
      emission_is_constant = false;
    }
  }

  ShaderInput *surf = graph->output()->input("Surface");
  emission_estimate = fabs(output_estimate_emission(surf->link, emission_is_constant));

  if (is_zero(emission_estimate)) {
    emission_sampling = EMISSION_SAMPLING_NONE;
  }
  else if (emission_sampling_method == EMISSION_SAMPLING_AUTO) {
    /* Automatically disable MIS when emission is low, to avoid weakly emitting
     * using a lot of memory in the light tree and potentially wasting samples
     * where indirect light samples are sufficient.
     * Possible optimization: estimate front and back emission separately. */
    emission_sampling = (reduce_max(emission_estimate) > 0.5f) ? EMISSION_SAMPLING_FRONT_BACK :
                                                                 EMISSION_SAMPLING_NONE;
  }
  else {
    emission_sampling = emission_sampling_method;
  }
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
  if (emission_sampling != EMISSION_SAMPLING_NONE)
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

  if (!has_surface) {
    foreach (ShaderNode *node, graph->nodes) {
      if (node->special_type == SHADER_SPECIAL_TYPE_OUTPUT_AOV) {
        foreach (const ShaderInput *in, node->inputs) {
          if (in->link) {
            has_surface = true;
          }
        }
      }
    }
  }

  /* get requested attributes. this could be optimized by pruning unused
   * nodes here already, but that's the job of the shader manager currently,
   * and may not be so great for interactive rendering where you temporarily
   * disconnect a node */

  AttributeRequestSet prev_attributes = attributes;

  attributes.clear();
  foreach (ShaderNode *node, graph->nodes) {
    node->attributes(this, &attributes);
  }

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
  if (!reference_count()) {
    tag_modified();
    /* We do not reference here as the shader will be referenced when added to a socket. */
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

  init_xyz_transforms();
}

ShaderManager::~ShaderManager() {}

ShaderManager *ShaderManager::create(int shadingsystem, Device *device)
{
  ShaderManager *manager;

  (void)shadingsystem; /* Ignored when built without OSL. */
  (void)device;

#ifdef WITH_OSL
  if (shadingsystem == SHADINGSYSTEM_OSL) {
    manager = new OSLShaderManager(device);
  }
  else
#endif
  {
    manager = new SVMShaderManager();
  }

  return manager;
}

uint64_t ShaderManager::get_attribute_id(ustring name)
{
  thread_scoped_spin_lock lock(attribute_lock_);

  /* get a unique id for each name, for SVM attribute lookup */
  AttributeIDMap::iterator it = unique_attribute_id.find(name);

  if (it != unique_attribute_id.end())
    return it->second;

  uint64_t id = ATTR_STD_NUM + unique_attribute_id.size();
  unique_attribute_id[name] = id;
  return id;
}

uint64_t ShaderManager::get_attribute_id(AttributeStandard std)
{
  return (uint64_t)std;
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

void ShaderManager::device_update(Device *device,
                                  DeviceScene *dscene,
                                  Scene *scene,
                                  Progress &progress)
{
  if (!need_update()) {
    return;
  }

  uint id = 0;
  foreach (Shader *shader, scene->shaders) {
    shader->id = id++;
  }

  /* Those shaders should always be compiled as they are used as fallback if a shader cannot be
   * found, e.g. bad shader index for the triangle shaders on a Mesh. */
  assert(scene->default_surface->reference_count() != 0);
  assert(scene->default_light->reference_count() != 0);
  assert(scene->default_background->reference_count() != 0);
  assert(scene->default_empty->reference_count() != 0);

  device_update_specific(device, dscene, scene, progress);
}

void ShaderManager::device_update_common(Device * /*device*/,
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

    if (shader->emission_sampling == EMISSION_SAMPLING_FRONT) {
      flag |= SD_MIS_FRONT;
    }
    else if (shader->emission_sampling == EMISSION_SAMPLING_BACK) {
      flag |= SD_MIS_BACK;
    }
    else if (shader->emission_sampling == EMISSION_SAMPLING_FRONT_BACK) {
      flag |= SD_MIS_FRONT | SD_MIS_BACK;
    }

    if (!is_zero(shader->emission_estimate))
      flag |= SD_HAS_EMISSION;
    if (shader->has_surface_transparent && shader->get_use_transparent_shadow())
      flag |= SD_HAS_TRANSPARENT_SHADOW;
    if (shader->has_surface_raytrace)
      flag |= SD_HAS_RAYTRACE;
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
    if (shader->get_volume_sampling_method() == VOLUME_SAMPLING_EQUIANGULAR)
      flag |= SD_VOLUME_EQUIANGULAR;
    if (shader->get_volume_sampling_method() == VOLUME_SAMPLING_MULTIPLE_IMPORTANCE)
      flag |= SD_VOLUME_MIS;
    if (shader->get_volume_interpolation_method() == VOLUME_INTERPOLATION_CUBIC)
      flag |= SD_VOLUME_CUBIC;
    if (shader->has_bump)
      flag |= SD_HAS_BUMP;
    if (shader->get_displacement_method() != DISPLACE_BUMP)
      flag |= SD_HAS_DISPLACEMENT;

    /* constant emission check */
    if (shader->emission_is_constant)
      flag |= SD_HAS_CONSTANT_EMISSION;

    uint32_t cryptomatte_id = util_murmur_hash3(shader->name.c_str(), shader->name.length(), 0);

    /* regular shader */
    kshader->flags = flag;
    kshader->pass_id = shader->get_pass_id();
    kshader->constant_emission[0] = shader->emission_estimate.x;
    kshader->constant_emission[1] = shader->emission_estimate.y;
    kshader->constant_emission[2] = shader->emission_estimate.z;
    kshader->cryptomatte_id = util_hash_to_float(cryptomatte_id);
    kshader++;

    has_transparent_shadow |= (flag & SD_HAS_TRANSPARENT_SHADOW) != 0;
  }

  dscene->shaders.copy_to_device();

  /* lookup tables */
  KernelTables *ktables = &dscene->data.tables;
  ktables->ggx_E = ensure_bsdf_table(dscene, scene, table_ggx_E);
  ktables->ggx_Eavg = ensure_bsdf_table(dscene, scene, table_ggx_Eavg);
  ktables->ggx_glass_E = ensure_bsdf_table(dscene, scene, table_ggx_glass_E);
  ktables->ggx_glass_Eavg = ensure_bsdf_table(dscene, scene, table_ggx_glass_Eavg);
  ktables->ggx_glass_inv_E = ensure_bsdf_table(dscene, scene, table_ggx_glass_inv_E);
  ktables->ggx_glass_inv_Eavg = ensure_bsdf_table(dscene, scene, table_ggx_glass_inv_Eavg);
  ktables->sheen_ltc = ensure_bsdf_table(dscene, scene, table_sheen_ltc);
  ktables->ggx_gen_schlick_ior_s = ensure_bsdf_table(dscene, scene, table_ggx_gen_schlick_ior_s);
  ktables->ggx_gen_schlick_s = ensure_bsdf_table(dscene, scene, table_ggx_gen_schlick_s);

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
  kfilm->rec709_to_r = float3_to_float4(rec709_to_r);
  kfilm->rec709_to_g = float3_to_float4(rec709_to_g);
  kfilm->rec709_to_b = float3_to_float4(rec709_to_b);
  kfilm->is_rec709 = is_rec709;
}

void ShaderManager::device_free_common(Device * /*device*/, DeviceScene *dscene, Scene *scene)
{
  for (auto &entry : bsdf_tables) {
    scene->lookup_tables->remove_table(&entry.second);
  }
  bsdf_tables.clear();

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
    shader->reference();
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
    /* No default reference for the volume to avoid compiling volume kernels if there are no
     * actual volumes in the scene */
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
    shader->reference();
    scene->default_light = shader;
    shader->tag_update(scene);
  }

  /* default background */
  {
    ShaderGraph *graph = new ShaderGraph();

    Shader *shader = scene->create_node<Shader>();
    shader->name = "default_background";
    shader->set_graph(graph);
    shader->reference();
    scene->default_background = shader;
    shader->tag_update(scene);
  }

  /* default empty */
  {
    ShaderGraph *graph = new ShaderGraph();

    Shader *shader = scene->create_node<Shader>();
    shader->name = "default_empty";
    shader->set_graph(graph);
    shader->reference();
    scene->default_empty = shader;
    shader->tag_update(scene);
  }
}

uint ShaderManager::get_graph_kernel_features(ShaderGraph *graph)
{
  uint kernel_features = 0;

  foreach (ShaderNode *node, graph->nodes) {
    kernel_features |= node->get_feature();
    if (node->special_type == SHADER_SPECIAL_TYPE_CLOSURE) {
      BsdfBaseNode *bsdf_node = static_cast<BsdfBaseNode *>(node);
      if (CLOSURE_IS_VOLUME(bsdf_node->get_closure_type())) {
        kernel_features |= KERNEL_FEATURE_NODE_VOLUME;
      }
    }
    if (node->has_surface_bssrdf()) {
      kernel_features |= KERNEL_FEATURE_SUBSURFACE;
    }
    if (node->has_surface_transparent()) {
      kernel_features |= KERNEL_FEATURE_TRANSPARENT;
    }
  }

  return kernel_features;
}

uint ShaderManager::get_kernel_features(Scene *scene)
{
  uint kernel_features = KERNEL_FEATURE_NODE_BSDF | KERNEL_FEATURE_NODE_EMISSION;
  for (int i = 0; i < scene->shaders.size(); i++) {
    Shader *shader = scene->shaders[i];
    if (!shader->reference_count()) {
      continue;
    }

    /* Gather requested features from all the nodes from the graph nodes. */
    kernel_features |= get_graph_kernel_features(shader->graph);
    ShaderNode *output_node = shader->graph->output();
    if (output_node->input("Displacement")->link != NULL) {
      kernel_features |= KERNEL_FEATURE_NODE_BUMP;
      if (shader->get_displacement_method() == DISPLACE_BOTH) {
        kernel_features |= KERNEL_FEATURE_NODE_BUMP_STATE;
      }
    }
    /* On top of volume nodes, also check if we need volume sampling because
     * e.g. an Emission node would slip through the KERNEL_FEATURE_NODE_VOLUME check */
    if (shader->has_volume_connected) {
      kernel_features |= KERNEL_FEATURE_VOLUME;
    }
  }

  if (use_osl()) {
    kernel_features |= KERNEL_FEATURE_OSL;
  }

  return kernel_features;
}

void ShaderManager::free_memory()
{

#ifdef WITH_OSL
  OSLShaderManager::free_memory();
#endif

  ColorSpaceManager::free_memory();
}

float ShaderManager::linear_rgb_to_gray(float3 c)
{
  return dot(c, rgb_to_y);
}

float3 ShaderManager::rec709_to_scene_linear(float3 c)
{
  return make_float3(dot(rec709_to_r, c), dot(rec709_to_g, c), dot(rec709_to_b, c));
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
    processor = config->getProcessor("scene_linear", colorspace);
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
  const Transform xyz_to_rec709 = make_transform(3.2404542f,
                                                 -1.5371385f,
                                                 -0.4985314f,
                                                 0.0f,
                                                 -0.9692660f,
                                                 1.8760108f,
                                                 0.0415560f,
                                                 0.0f,
                                                 0.0556434f,
                                                 -0.2040259f,
                                                 1.0572252f,
                                                 0.0f);

  xyz_to_r = float4_to_float3(xyz_to_rec709.x);
  xyz_to_g = float4_to_float3(xyz_to_rec709.y);
  xyz_to_b = float4_to_float3(xyz_to_rec709.z);
  rgb_to_y = make_float3(0.2126729f, 0.7151522f, 0.0721750f);

  rec709_to_r = make_float3(1.0f, 0.0f, 0.0f);
  rec709_to_g = make_float3(0.0f, 1.0f, 0.0f);
  rec709_to_b = make_float3(0.0f, 0.0f, 1.0f);
  is_rec709 = true;

#ifdef WITH_OCIO
  /* Get from OpenColorO config if it has the required roles. */
  OCIO::ConstConfigRcPtr config = OCIO::GetCurrentConfig();
  if (!(config && config->hasRole("scene_linear"))) {
    return;
  }

  Transform xyz_to_rgb;

  if (config->hasRole("aces_interchange")) {
    /* Standard OpenColorIO role, defined as ACES AP0 (ACES2065-1). */
    Transform aces_to_rgb;
    if (!to_scene_linear_transform(config, "aces_interchange", aces_to_rgb)) {
      return;
    }

    /* This is the OpenColorIO builtin transform:
     * UTILITY - ACES-AP0_to_CIE-XYZ-D65_BFD. */
    const Transform ACES_AP0_to_xyz_D65 = make_transform(0.938280f,
                                                         -0.004451f,
                                                         0.016628f,
                                                         0.000000f,
                                                         0.337369f,
                                                         0.729522f,
                                                         -0.066890f,
                                                         0.000000f,
                                                         0.001174f,
                                                         -0.003711f,
                                                         1.091595f,
                                                         0.000000f);
    const Transform xyz_to_aces = transform_inverse(ACES_AP0_to_xyz_D65);
    xyz_to_rgb = aces_to_rgb * xyz_to_aces;
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

  const Transform rec709_to_rgb = xyz_to_rgb * transform_inverse(xyz_to_rec709);
  rec709_to_r = float4_to_float3(rec709_to_rgb.x);
  rec709_to_g = float4_to_float3(rec709_to_rgb.y);
  rec709_to_b = float4_to_float3(rec709_to_rgb.z);
  is_rec709 = transform_equal_threshold(xyz_to_rgb, xyz_to_rec709, 0.0001f);
#endif
}

size_t ShaderManager::ensure_bsdf_table_impl(DeviceScene *dscene,
                                             Scene *scene,
                                             const float *table,
                                             size_t n)
{
  /* Since the BSDF tables are static arrays, we can use their address to identify them. */
  if (!(bsdf_tables.count(table))) {
    vector<float> entries(table, table + n);
    bsdf_tables[table] = scene->lookup_tables->add_table(dscene, entries);
  }
  return bsdf_tables[table];
}

CCL_NAMESPACE_END
