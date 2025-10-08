/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Intermediate node graph for generating GLSL shaders.
 */

#include <cstdio>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_node_types.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_stack.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "GPU_texture.hh"
#include "GPU_vertex_format.hh"

#include "gpu_material_library.hh"
#include "gpu_node_graph.hh"

/* Node Link Functions */

static GPUNodeLink *gpu_node_link_create()
{
  GPUNodeLink *link = MEM_callocN<GPUNodeLink>("GPUNodeLink");
  link->users++;

  return link;
}

static void gpu_node_link_free(GPUNodeLink *link)
{
  link->users--;

  if (link->users < 0) {
    fprintf(stderr, "gpu_node_link_free: negative refcount\n");
  }

  if (link->users == 0) {
    if (link->output) {
      link->output->link = nullptr;
    }
    MEM_freeN(link);
  }
}

/* Node Functions */

static GPUNode *gpu_node_create(const char *name)
{
  GPUNode *node = MEM_callocN<GPUNode>("GPUNode");

  node->name = name;
  node->zone_index = -1;
  node->is_zone_end = false;

  return node;
}

static void gpu_node_input_link(GPUNode *node, GPUNodeLink *link, const GPUType type)
{
  GPUInput *input;
  GPUNode *outnode;
  const char *name;

  if (link->link_type == GPU_NODE_LINK_OUTPUT) {
    outnode = link->output->node;
    name = outnode->name;
    input = static_cast<GPUInput *>(outnode->inputs.first);

    if (STR_ELEM(name, "set_value", "set_rgb", "set_rgba") && (input->type == type)) {
      input = static_cast<GPUInput *>(MEM_dupallocN(outnode->inputs.first));

      switch (input->source) {
        case GPU_SOURCE_ATTR:
          input->attr->users++;
          break;
        case GPU_SOURCE_UNIFORM_ATTR:
          input->uniform_attr->users++;
          break;
        case GPU_SOURCE_LAYER_ATTR:
          input->layer_attr->users++;
          break;
        case GPU_SOURCE_TEX:
          input->texture->users++;
          break;
        case GPU_SOURCE_TEX_TILED_MAPPING:
          /* Already handled by GPU_SOURCE_TEX. */
        default:
          break;
      }

      if (input->link) {
        input->link->users++;
      }

      BLI_addtail(&node->inputs, input);
      return;
    }
  }

  input = MEM_callocN<GPUInput>("GPUInput");
  input->node = node;
  input->type = type;

  switch (link->link_type) {
    case GPU_NODE_LINK_OUTPUT:
      input->source = GPU_SOURCE_OUTPUT;
      input->link = link;
      link->users++;
      break;
    case GPU_NODE_LINK_IMAGE:
    case GPU_NODE_LINK_IMAGE_TILED:
    case GPU_NODE_LINK_IMAGE_SKY:
    case GPU_NODE_LINK_COLORBAND:
      input->source = GPU_SOURCE_TEX;
      input->texture = link->texture;
      break;
    case GPU_NODE_LINK_IMAGE_TILED_MAPPING:
      input->source = GPU_SOURCE_TEX_TILED_MAPPING;
      input->texture = link->texture;
      break;
    case GPU_NODE_LINK_ATTR:
      input->source = GPU_SOURCE_ATTR;
      input->attr = link->attr;
      /* Fail-safe handling if the same attribute is used with different data-types for
       * some reason (only really makes sense with float/vec2/vec3/vec4 though). This
       * can happen if mixing the generic Attribute node with specialized ones. */
      CLAMP_MIN(input->attr->gputype, type);
      break;
    case GPU_NODE_LINK_UNIFORM_ATTR:
      input->source = GPU_SOURCE_UNIFORM_ATTR;
      input->uniform_attr = link->uniform_attr;
      break;
    case GPU_NODE_LINK_LAYER_ATTR:
      input->source = GPU_SOURCE_LAYER_ATTR;
      input->layer_attr = link->layer_attr;
      break;
    case GPU_NODE_LINK_CONSTANT:
      input->source = (type == GPU_CLOSURE) ? GPU_SOURCE_STRUCT : GPU_SOURCE_CONSTANT;
      break;
    case GPU_NODE_LINK_UNIFORM:
      input->source = GPU_SOURCE_UNIFORM;
      break;
    case GPU_NODE_LINK_DIFFERENTIATE_FLOAT_FN:
      input->source = GPU_SOURCE_FUNCTION_CALL;
      /* NOTE(@fclem): End of function call is the return variable set during codegen. */
      SNPRINTF(input->function_call,
               "dF_branch_incomplete(%s(), %g, ",
               link->differentiate_float.function_name,
               link->differentiate_float.filter_width);
      break;
    default:
      break;
  }

  if (ELEM(input->source, GPU_SOURCE_CONSTANT, GPU_SOURCE_UNIFORM)) {
    memcpy(input->vec, link->data, type * sizeof(float));
  }

  if (link->link_type != GPU_NODE_LINK_OUTPUT) {
    MEM_freeN(link);
  }
  BLI_addtail(&node->inputs, input);
}

static const char *gpu_uniform_set_function_from_type(eNodeSocketDatatype type)
{
  switch (type) {
    /* For now INT & BOOL are supported as float. */
    case SOCK_INT:
    case SOCK_FLOAT:
    case SOCK_BOOLEAN:
      return "set_value";
    case SOCK_VECTOR:
      return "set_rgb";
    case SOCK_RGBA:
      return "set_rgba";
    default:
      BLI_assert_msg(0, "No gpu function for non-supported eNodeSocketDatatype");
      return nullptr;
  }
}

/**
 * Link stack uniform buffer.
 * This is called for the input/output sockets that are not connected.
 */
static GPUNodeLink *gpu_uniformbuffer_link(GPUMaterial *mat,
                                           const bNode *node,
                                           GPUNodeStack *stack,
                                           const int index,
                                           const eNodeSocketInOut in_out)
{
  bNodeSocket *socket;

  if (in_out == SOCK_IN) {
    socket = static_cast<bNodeSocket *>(BLI_findlink(&node->inputs, index));
  }
  else {
    socket = static_cast<bNodeSocket *>(BLI_findlink(&node->outputs, index));
  }

  BLI_assert(socket != nullptr);
  BLI_assert(socket->in_out == in_out);

  if (socket->flag & SOCK_HIDE_VALUE) {
    return nullptr;
  }

  if (!ELEM(socket->type, SOCK_INT, SOCK_FLOAT, SOCK_VECTOR, SOCK_RGBA)) {
    return nullptr;
  }

  GPUNodeLink *link = GPU_uniform(stack->vec);

  if (in_out == SOCK_IN) {
    GPU_link(mat,
             gpu_uniform_set_function_from_type(eNodeSocketDatatype(socket->type)),
             link,
             &stack->link);
  }

  return link;
}

static void gpu_node_input_socket(
    GPUMaterial *material, const bNode *bnode, GPUNode *node, GPUNodeStack *sock, const int index)
{
  if (sock->link) {
    gpu_node_input_link(node, sock->link, sock->type);
  }
  else if ((material != nullptr) &&
           (gpu_uniformbuffer_link(material, bnode, sock, index, SOCK_IN) != nullptr))
  {
    gpu_node_input_link(node, sock->link, sock->type);
  }
  else {
    gpu_node_input_link(node, GPU_constant(sock->vec), sock->type);
  }
}

static void gpu_node_output(GPUNode *node, const GPUType type, GPUNodeLink **link)
{
  GPUOutput *output = MEM_callocN<GPUOutput>("GPUOutput");

  output->type = type;
  output->node = node;

  if (link) {
    *link = output->link = gpu_node_link_create();
    output->link->link_type = GPU_NODE_LINK_OUTPUT;
    output->link->output = output;

    /* NOTE: the caller owns the reference to the link, GPUOutput
     * merely points to it, and if the node is destroyed it will
     * set that pointer to nullptr */
  }

  BLI_addtail(&node->outputs, output);
}

/* Uniform Attribute Functions */

static int uniform_attr_sort_cmp(const void *a, const void *b)
{
  const GPUUniformAttr *attr_a = static_cast<const GPUUniformAttr *>(a),
                       *attr_b = static_cast<const GPUUniformAttr *>(b);

  int cmps = strcmp(attr_a->name, attr_b->name);
  if (cmps != 0) {
    return cmps > 0 ? 1 : 0;
  }

  return (attr_a->use_dupli && !attr_b->use_dupli);
}

static uint uniform_attr_list_hash(const void *key)
{
  const GPUUniformAttrList *attrs = static_cast<const GPUUniformAttrList *>(key);
  return attrs->hash_code;
}

static bool uniform_attr_list_cmp(const void *a, const void *b)
{
  const GPUUniformAttrList *set_a = static_cast<const GPUUniformAttrList *>(a),
                           *set_b = static_cast<const GPUUniformAttrList *>(b);

  if (set_a->hash_code != set_b->hash_code || set_a->count != set_b->count) {
    return true;
  }

  GPUUniformAttr *attr_a = static_cast<GPUUniformAttr *>(set_a->list.first),
                 *attr_b = static_cast<GPUUniformAttr *>(set_b->list.first);

  for (; attr_a && attr_b; attr_a = attr_a->next, attr_b = attr_b->next) {
    if (!STREQ(attr_a->name, attr_b->name) || attr_a->use_dupli != attr_b->use_dupli) {
      return true;
    }
  }

  return attr_a || attr_b;
}

GHash *GPU_uniform_attr_list_hash_new(const char *info)
{
  return BLI_ghash_new(uniform_attr_list_hash, uniform_attr_list_cmp, info);
}

void GPU_uniform_attr_list_copy(GPUUniformAttrList *dest, const GPUUniformAttrList *src)
{
  dest->count = src->count;
  dest->hash_code = src->hash_code;
  BLI_duplicatelist(&dest->list, &src->list);
}

void GPU_uniform_attr_list_free(GPUUniformAttrList *set)
{
  set->count = 0;
  set->hash_code = 0;
  BLI_freelistN(&set->list);
}

void gpu_node_graph_finalize_uniform_attrs(GPUNodeGraph *graph)
{
  GPUUniformAttrList *attrs = &graph->uniform_attrs;
  BLI_assert(attrs->count == BLI_listbase_count(&attrs->list));

  /* Sort the attributes by name to ensure a stable order. */
  BLI_listbase_sort(&attrs->list, uniform_attr_sort_cmp);

  /* Compute the indices and the hash code. */
  int next_id = 0;
  attrs->hash_code = 0;

  LISTBASE_FOREACH (GPUUniformAttr *, attr, &attrs->list) {
    attr->id = next_id++;
    attrs->hash_code ^= BLI_ghashutil_uinthash(attr->hash_code + (1 << (attr->id + 1)));
  }
}

/* Attributes and Textures */

static char attr_prefix_get(const GPUMaterialAttribute *attr)
{
  if (attr->is_default_color) {
    return 'c';
  }
  if (attr->is_hair_length) {
    return 'l';
  }
  if (attr->is_hair_intercept) {
    return 'i';
  }
  switch (attr->type) {
    case CD_TANGENT:
      return 't';
    case CD_AUTO_FROM_NAME:
      return 'a';
    default:
      BLI_assert_msg(0, "GPUVertAttr Prefix type not found : This should not happen!");
      return '\0';
  }
}

static void attr_input_name(GPUMaterialAttribute *attr)
{
  /* NOTE: Replicate changes to mesh_render_data_create() in draw_cache_impl_mesh.cc */
  if (attr->type == CD_ORCO) {
    /* OPTI: orco is computed from local positions, but only if no modifier is present. */
    STRNCPY(attr->input_name, "orco");
  }
  else {
    attr->input_name[0] = attr_prefix_get(attr);
    attr->input_name[1] = '\0';
    if (attr->name[0] != '\0') {
      /* XXX FIXME: see notes in mesh_render_data_create() */
      GPU_vertformat_safe_attr_name(attr->name, &attr->input_name[1], GPU_MAX_SAFE_ATTR_NAME);
    }
  }
}

/** Add a new varying attribute of given type and name. Returns nullptr if out of slots. */
static GPUMaterialAttribute *gpu_node_graph_add_attribute(GPUNodeGraph *graph,
                                                          eCustomDataType type,
                                                          const char *name,
                                                          const bool is_default_color,
                                                          const bool is_hair_length,
                                                          const bool is_hair_intercept)
{
  /* Find existing attribute. */
  int num_attributes = 0;
  GPUMaterialAttribute *attr = static_cast<GPUMaterialAttribute *>(graph->attributes.first);
  for (; attr; attr = attr->next) {
    if (attr->type == type && STREQ(attr->name, name) &&
        attr->is_default_color == is_default_color && attr->is_hair_length == is_hair_length &&
        attr->is_hair_intercept == is_hair_intercept)
    {
      break;
    }
    num_attributes++;
  }

  /* Add new requested attribute if it's within GPU limits. */
  if (attr == nullptr) {
    attr = MEM_callocN<GPUMaterialAttribute>(__func__);
    attr->is_default_color = is_default_color;
    attr->is_hair_length = is_hair_length;
    attr->is_hair_intercept = is_hair_intercept;
    attr->type = type;
    STRNCPY(attr->name, name);
    attr_input_name(attr);
    attr->id = num_attributes;
    BLI_addtail(&graph->attributes, attr);
  }

  if (attr != nullptr) {
    attr->users++;
  }

  return attr;
}

/** Add a new uniform attribute of given type and name. Returns nullptr if out of slots. */
static GPUUniformAttr *gpu_node_graph_add_uniform_attribute(GPUNodeGraph *graph,
                                                            const char *name,
                                                            bool use_dupli)
{
  /* Find existing attribute. */
  GPUUniformAttrList *attrs = &graph->uniform_attrs;
  GPUUniformAttr *attr = static_cast<GPUUniformAttr *>(attrs->list.first);

  for (; attr; attr = attr->next) {
    if (STREQ(attr->name, name) && attr->use_dupli == use_dupli) {
      break;
    }
  }

  /* Add new requested attribute if it's within GPU limits. */
  if (attr == nullptr && attrs->count < GPU_MAX_UNIFORM_ATTR) {
    attr = MEM_callocN<GPUUniformAttr>(__func__);
    STRNCPY(attr->name, name);
    attr->use_dupli = use_dupli;
    attr->hash_code = BLI_ghashutil_strhash_p(attr->name) << 1 | (attr->use_dupli ? 0 : 1);
    attr->id = -1;
    BLI_addtail(&attrs->list, attr);
    attrs->count++;
  }

  if (attr != nullptr) {
    attr->users++;
  }

  return attr;
}

/** Add a new uniform attribute of given type and name. Returns nullptr if out of slots. */
static GPULayerAttr *gpu_node_graph_add_layer_attribute(GPUNodeGraph *graph, const char *name)
{
  /* Find existing attribute. */
  ListBase *attrs = &graph->layer_attrs;
  GPULayerAttr *attr = static_cast<GPULayerAttr *>(attrs->first);

  for (; attr; attr = attr->next) {
    if (STREQ(attr->name, name)) {
      break;
    }
  }

  /* Add new requested attribute to the list. */
  if (attr == nullptr) {
    attr = MEM_callocN<GPULayerAttr>(__func__);
    STRNCPY(attr->name, name);
    attr->hash_code = BLI_ghashutil_strhash_p(attr->name);
    BLI_addtail(attrs, attr);
  }

  if (attr != nullptr) {
    attr->users++;
  }

  return attr;
}

static GPUMaterialTexture *gpu_node_graph_add_texture(GPUNodeGraph *graph,
                                                      Image *ima,
                                                      ImageUser *iuser,
                                                      blender::gpu::Texture **colorband,
                                                      blender::gpu::Texture **sky,
                                                      bool is_tiled,
                                                      GPUSamplerState sampler_state)
{
  /* Find existing texture. */
  int num_textures = 0;
  GPUMaterialTexture *tex = static_cast<GPUMaterialTexture *>(graph->textures.first);
  for (; tex; tex = tex->next) {
    if (tex->ima == ima && tex->colorband == colorband && tex->sky == sky &&
        tex->sampler_state == sampler_state)
    {
      break;
    }
    num_textures++;
  }

  /* Add new requested texture. */
  if (tex == nullptr) {
    tex = MEM_callocN<GPUMaterialTexture>(__func__);
    tex->ima = ima;
    if (iuser != nullptr) {
      tex->iuser = *iuser;
      tex->iuser_available = true;
    }
    tex->colorband = colorband;
    tex->sky = sky;
    tex->sampler_state = sampler_state;
    SNPRINTF(tex->sampler_name, "samp%d", num_textures);
    if (is_tiled) {
      SNPRINTF(tex->tiled_mapping_name, "tsamp%d", num_textures);
    }
    BLI_addtail(&graph->textures, tex);
  }

  tex->users++;

  return tex;
}

/* Creating Inputs */

GPUNodeLink *GPU_attribute(GPUMaterial *mat, const eCustomDataType type, const char *name)
{
  GPUNodeGraph *graph = gpu_material_node_graph(mat);
  GPUMaterialAttribute *attr = gpu_node_graph_add_attribute(
      graph, type, name, false, false, false);

  if (type == CD_ORCO) {
    /* OPTI: orco might be computed from local positions and needs object information. */
    GPU_material_flag_set(mat, GPU_MATFLAG_OBJECT_INFO);
  }

  /* Dummy fallback if out of slots. */
  if (attr == nullptr) {
    static const float zero_data[GPU_MAX_CONSTANT_DATA] = {0.0f};
    return GPU_constant(zero_data);
  }

  GPUNodeLink *link = gpu_node_link_create();
  link->link_type = GPU_NODE_LINK_ATTR;
  link->attr = attr;
  return link;
}

GPUNodeLink *GPU_attribute_default_color(GPUMaterial *mat)
{
  GPUNodeGraph *graph = gpu_material_node_graph(mat);
  GPUMaterialAttribute *attr = gpu_node_graph_add_attribute(
      graph, CD_AUTO_FROM_NAME, "", true, false, false);
  if (attr == nullptr) {
    static const float zero_data[GPU_MAX_CONSTANT_DATA] = {0.0f};
    return GPU_constant(zero_data);
  }
  attr->is_default_color = true;
  GPUNodeLink *link = gpu_node_link_create();
  link->link_type = GPU_NODE_LINK_ATTR;
  link->attr = attr;
  return link;
}

GPUNodeLink *GPU_attribute_hair_length(GPUMaterial *mat)
{
  GPUNodeGraph *graph = gpu_material_node_graph(mat);
  GPUMaterialAttribute *attr = gpu_node_graph_add_attribute(
      graph, CD_AUTO_FROM_NAME, "", false, true, false);
  if (attr == nullptr) {
    static const float zero_data[GPU_MAX_CONSTANT_DATA] = {0.0f};
    return GPU_constant(zero_data);
  }
  GPUNodeLink *link = gpu_node_link_create();
  link->link_type = GPU_NODE_LINK_ATTR;
  link->attr = attr;
  return link;
}

GPUNodeLink *GPU_attribute_hair_intercept(GPUMaterial *mat)
{
  GPUNodeGraph *graph = gpu_material_node_graph(mat);
  GPUMaterialAttribute *attr = gpu_node_graph_add_attribute(
      graph, CD_AUTO_FROM_NAME, "", false, false, true);
  if (attr == nullptr) {
    static const float zero_data[GPU_MAX_CONSTANT_DATA] = {0.0f};
    return GPU_constant(zero_data);
  }
  GPUNodeLink *link = gpu_node_link_create();
  link->link_type = GPU_NODE_LINK_ATTR;
  link->attr = attr;
  return link;
}

GPUNodeLink *GPU_attribute_with_default(GPUMaterial *mat,
                                        const eCustomDataType type,
                                        const char *name,
                                        GPUDefaultValue default_value)
{
  GPUNodeLink *link = GPU_attribute(mat, type, name);
  if (link->link_type == GPU_NODE_LINK_ATTR) {
    link->attr->default_value = default_value;
  }
  return link;
}

GPUNodeLink *GPU_uniform_attribute(GPUMaterial *mat,
                                   const char *name,
                                   bool use_dupli,
                                   uint32_t *r_hash)
{
  GPUNodeGraph *graph = gpu_material_node_graph(mat);
  GPUUniformAttr *attr = gpu_node_graph_add_uniform_attribute(graph, name, use_dupli);

  /* Dummy fallback if out of slots. */
  if (attr == nullptr) {
    *r_hash = 0;
    static const float zero_data[GPU_MAX_CONSTANT_DATA] = {0.0f};
    return GPU_constant(zero_data);
  }
  *r_hash = attr->hash_code;

  GPUNodeLink *link = gpu_node_link_create();
  link->link_type = GPU_NODE_LINK_UNIFORM_ATTR;
  link->uniform_attr = attr;
  return link;
}

GPUNodeLink *GPU_layer_attribute(GPUMaterial *mat, const char *name)
{
  GPUNodeGraph *graph = gpu_material_node_graph(mat);
  GPULayerAttr *attr = gpu_node_graph_add_layer_attribute(graph, name);

  GPUNodeLink *link = gpu_node_link_create();
  link->link_type = GPU_NODE_LINK_LAYER_ATTR;
  link->layer_attr = attr;
  return link;
}

GPUNodeLink *GPU_constant(const float *num)
{
  GPUNodeLink *link = gpu_node_link_create();
  link->link_type = GPU_NODE_LINK_CONSTANT;
  link->data = num;
  return link;
}

GPUNodeLink *GPU_uniform(const float *num)
{
  GPUNodeLink *link = gpu_node_link_create();
  link->link_type = GPU_NODE_LINK_UNIFORM;
  link->data = num;
  return link;
}

GPUNodeLink *GPU_differentiate_float_function(const char *function_name, const float filter_width)
{
  GPUNodeLink *link = gpu_node_link_create();
  link->link_type = GPU_NODE_LINK_DIFFERENTIATE_FLOAT_FN;
  link->differentiate_float.function_name = function_name;
  link->differentiate_float.filter_width = filter_width;
  return link;
}

GPUNodeLink *GPU_image(GPUMaterial *mat,
                       Image *ima,
                       ImageUser *iuser,
                       GPUSamplerState sampler_state)
{
  GPUNodeGraph *graph = gpu_material_node_graph(mat);
  GPUNodeLink *link = gpu_node_link_create();
  link->link_type = GPU_NODE_LINK_IMAGE;
  link->texture = gpu_node_graph_add_texture(
      graph, ima, iuser, nullptr, nullptr, false, sampler_state);
  return link;
}

GPUNodeLink *GPU_image_sky(GPUMaterial *mat,
                           int width,
                           int height,
                           const float *pixels,
                           float *layer,
                           GPUSamplerState sampler_state)
{
  blender::gpu::Texture **sky = gpu_material_sky_texture_layer_set(
      mat, width, height, pixels, layer);

  GPUNodeGraph *graph = gpu_material_node_graph(mat);
  GPUNodeLink *link = gpu_node_link_create();
  link->link_type = GPU_NODE_LINK_IMAGE_SKY;
  link->texture = gpu_node_graph_add_texture(
      graph, nullptr, nullptr, nullptr, sky, false, sampler_state);
  return link;
}

void GPU_image_tiled(GPUMaterial *mat,
                     Image *ima,
                     ImageUser *iuser,
                     GPUSamplerState sampler_state,
                     GPUNodeLink **r_image_tiled_link,
                     GPUNodeLink **r_image_tiled_mapping_link)
{
  GPUNodeGraph *graph = gpu_material_node_graph(mat);
  GPUMaterialTexture *texture = gpu_node_graph_add_texture(
      graph, ima, iuser, nullptr, nullptr, true, sampler_state);

  (*r_image_tiled_link) = gpu_node_link_create();
  (*r_image_tiled_link)->link_type = GPU_NODE_LINK_IMAGE_TILED;
  (*r_image_tiled_link)->texture = texture;

  (*r_image_tiled_mapping_link) = gpu_node_link_create();
  (*r_image_tiled_mapping_link)->link_type = GPU_NODE_LINK_IMAGE_TILED_MAPPING;
  (*r_image_tiled_mapping_link)->texture = texture;
}

GPUNodeLink *GPU_color_band(GPUMaterial *mat, int size, float *pixels, float *r_row)
{
  blender::gpu::Texture **colorband = gpu_material_ramp_texture_row_set(mat, size, pixels, r_row);
  MEM_freeN(pixels);

  GPUNodeGraph *graph = gpu_material_node_graph(mat);
  GPUNodeLink *link = gpu_node_link_create();
  link->link_type = GPU_NODE_LINK_COLORBAND;
  link->texture = gpu_node_graph_add_texture(
      graph, nullptr, nullptr, colorband, nullptr, false, GPUSamplerState::internal_sampler());
  return link;
}

/* Creating Nodes */

bool GPU_link(GPUMaterial *mat, const char *name, ...)
{
  GPUNodeGraph *graph = gpu_material_node_graph(mat);
  GPUNode *node;
  GPUFunction *function;
  GPUNodeLink *link, **linkptr;
  va_list params;
  int i;

  function = gpu_material_library_get_function(name);
  if (!function) {
    fprintf(stderr, "GPU failed to find function %s\n", name);
    return false;
  }

  node = gpu_node_create(name);

  va_start(params, name);
  for (i = 0; i < function->totparam; i++) {
    if (function->paramqual[i] == FUNCTION_QUAL_OUT) {
      linkptr = va_arg(params, GPUNodeLink **);
      gpu_node_output(node, function->paramtype[i], linkptr);
    }
    else {
      link = va_arg(params, GPUNodeLink *);
      gpu_node_input_link(node, link, function->paramtype[i]);
    }
  }
  va_end(params);

  BLI_addtail(&graph->nodes, node);

  return true;
}

static bool gpu_stack_link_v(GPUMaterial *material,
                             const bNode *bnode,
                             const char *name,
                             GPUNodeStack *in,
                             GPUNodeStack *out,
                             va_list params)
{
  GPUNodeGraph *graph = gpu_material_node_graph(material);
  GPUNode *node;
  GPUFunction *function;
  GPUNodeLink *link, **linkptr;
  int i, totin, totout;

  function = gpu_material_library_get_function(name);
  if (!function) {
    fprintf(stderr, "GPU failed to find function %s\n", name);
    return false;
  }

  node = gpu_node_create(name);
  totin = 0;
  totout = 0;

  if (in) {
    for (i = 0; !in[i].end; i++) {
      if (in[i].type != GPU_NONE) {
        gpu_node_input_socket(material, bnode, node, &in[i], i);
        totin++;
      }
    }
  }

  if (out) {
    for (i = 0; !out[i].end; i++) {
      if (out[i].type != GPU_NONE) {
        gpu_node_output(node, out[i].type, &out[i].link);
        totout++;
      }
    }
  }

  for (i = 0; i < function->totparam; i++) {
    if (function->paramqual[i] == FUNCTION_QUAL_OUT) {
      if (totout == 0) {
        linkptr = va_arg(params, GPUNodeLink **);
        gpu_node_output(node, function->paramtype[i], linkptr);
      }
      else {
        totout--;
      }
    }
    else {
      if (totin == 0) {
        link = va_arg(params, GPUNodeLink *);
        if (link->socket) {
          gpu_node_input_socket(nullptr, nullptr, node, link->socket, -1);
        }
        else {
          gpu_node_input_link(node, link, function->paramtype[i]);
        }
      }
      else {
        totin--;
      }
    }
  }

  BLI_addtail(&graph->nodes, node);

  return true;
}

bool GPU_stack_link(GPUMaterial *material,
                    const bNode *bnode,
                    const char *name,
                    GPUNodeStack *in,
                    GPUNodeStack *out,
                    ...)
{
  va_list params;
  va_start(params, out);
  bool valid = gpu_stack_link_v(material, bnode, name, in, out, params);
  va_end(params);

  return valid;
}

bool GPU_stack_link_zone(GPUMaterial *material,
                         const bNode *bnode,
                         const char *name,
                         GPUNodeStack *in,
                         GPUNodeStack *out,
                         int zone_index,
                         bool is_zone_end,
                         int in_argument_count,
                         int out_argument_count)
{
  GPUNodeGraph *graph = gpu_material_node_graph(material);
  GPUNode *node;
  int i;

  node = gpu_node_create(name);
  node->zone_index = zone_index;
  node->is_zone_end = is_zone_end;

  if (in) {
    for (i = 0; !in[i].end; i++) {
      if (in[i].type != GPU_NONE) {
        gpu_node_input_socket(material, bnode, node, &in[i], i);
      }
    }
  }

  if (out) {
    for (i = 0; !out[i].end; i++) {
      if (out[i].type != GPU_NONE) {
        gpu_node_output(node, out[i].type, &out[i].link);
      }
    }
  }

  LISTBASE_FOREACH_INDEX (GPUInput *, input, &node->inputs, i) {
    input->is_zone_io = i >= in_argument_count;
    input->is_duplicate = input->is_zone_io && is_zone_end;
  }
  LISTBASE_FOREACH_INDEX (GPUOutput *, output, &node->outputs, i) {
    output->is_zone_io = i >= out_argument_count;
    output->is_duplicate = output->is_zone_io;
  }

  BLI_addtail(&graph->nodes, node);

  return true;
}

/* Node Graph */

static void gpu_inputs_free(ListBase *inputs)
{
  LISTBASE_FOREACH (GPUInput *, input, inputs) {
    switch (input->source) {
      case GPU_SOURCE_ATTR:
        input->attr->users--;
        break;
      case GPU_SOURCE_UNIFORM_ATTR:
        input->uniform_attr->users--;
        break;
      case GPU_SOURCE_LAYER_ATTR:
        input->layer_attr->users--;
        break;
      case GPU_SOURCE_TEX:
        input->texture->users--;
        break;
      case GPU_SOURCE_TEX_TILED_MAPPING:
        /* Already handled by GPU_SOURCE_TEX. */
      default:
        break;
    }

    if (input->link) {
      gpu_node_link_free(input->link);
    }
  }

  BLI_freelistN(inputs);
}

static void gpu_node_free(GPUNode *node)
{
  gpu_inputs_free(&node->inputs);

  LISTBASE_FOREACH (GPUOutput *, output, &node->outputs) {
    if (output->link) {
      output->link->output = nullptr;
      gpu_node_link_free(output->link);
    }
  }

  BLI_freelistN(&node->outputs);
  MEM_freeN(node);
}

void gpu_node_graph_free_nodes(GPUNodeGraph *graph)
{
  while (GPUNode *node = static_cast<GPUNode *>(BLI_pophead(&graph->nodes))) {
    gpu_node_free(node);
  }

  graph->outlink_surface = nullptr;
  graph->outlink_volume = nullptr;
  graph->outlink_displacement = nullptr;
  graph->outlink_thickness = nullptr;
}

void gpu_node_graph_free(GPUNodeGraph *graph)
{
  BLI_freelistN(&graph->outlink_aovs);
  BLI_freelistN(&graph->material_functions);
  BLI_freelistN(&graph->outlink_compositor);
  gpu_node_graph_free_nodes(graph);

  BLI_freelistN(&graph->textures);
  BLI_freelistN(&graph->attributes);
  GPU_uniform_attr_list_free(&graph->uniform_attrs);
  BLI_freelistN(&graph->layer_attrs);
}

/* Prune Unused Nodes */

void gpu_nodes_tag(GPUNodeGraph *graph, GPUNodeLink *link_start, GPUNodeTag tag)
{
  if (!link_start || !link_start->output) {
    return;
  }

  blender::Stack<GPUNode *> stack;
  blender::Stack<GPUNode *> zone_stack;
  stack.push(link_start->output->node);

  while (!stack.is_empty() || !zone_stack.is_empty()) {
    GPUNode *node = !stack.is_empty() ? stack.pop() : zone_stack.pop();

    if (node->tag & tag) {
      continue;
    }

    node->tag |= tag;
    LISTBASE_FOREACH (GPUInput *, input, &node->inputs) {
      if (input->link && input->link->output) {
        stack.push(input->link->output->node);
      }
    }

    /* Zone input nodes are implicitly linked to their corresponding zone output nodes,
     * even if there is no GPUNodeLink between them. */
    if (node->is_zone_end) {
      LISTBASE_FOREACH (GPUNode *, node2, &graph->nodes) {
        if (node2->zone_index == node->zone_index && !node2->is_zone_end && !(node2->tag & tag)) {
          node2->tag |= tag;
          LISTBASE_FOREACH (GPUInput *, input, &node2->inputs) {
            if (input->link && input->link->output) {
              zone_stack.push(input->link->output->node);
            }
          }
        }
      }
    }
  }
}

void gpu_node_graph_prune_unused(GPUNodeGraph *graph)
{
  LISTBASE_FOREACH (GPUNode *, node, &graph->nodes) {
    node->tag = GPU_NODE_TAG_NONE;
  }

  gpu_nodes_tag(graph, graph->outlink_surface, GPU_NODE_TAG_SURFACE);
  gpu_nodes_tag(graph, graph->outlink_volume, GPU_NODE_TAG_VOLUME);
  gpu_nodes_tag(graph, graph->outlink_displacement, GPU_NODE_TAG_DISPLACEMENT);
  gpu_nodes_tag(graph, graph->outlink_thickness, GPU_NODE_TAG_THICKNESS);

  LISTBASE_FOREACH (GPUNodeGraphOutputLink *, aovlink, &graph->outlink_aovs) {
    gpu_nodes_tag(graph, aovlink->outlink, GPU_NODE_TAG_AOV);
  }
  LISTBASE_FOREACH (GPUNodeGraphFunctionLink *, funclink, &graph->material_functions) {
    gpu_nodes_tag(graph, funclink->outlink, GPU_NODE_TAG_FUNCTION);
  }
  LISTBASE_FOREACH (GPUNodeGraphOutputLink *, compositor_link, &graph->outlink_compositor) {
    gpu_nodes_tag(graph, compositor_link->outlink, GPU_NODE_TAG_COMPOSITOR);
  }

  for (GPUNode *node = static_cast<GPUNode *>(graph->nodes.first), *next = nullptr; node;
       node = next)
  {
    next = node->next;

    if (node->tag == GPU_NODE_TAG_NONE) {
      BLI_remlink(&graph->nodes, node);
      gpu_node_free(node);
    }
  }

  for (GPUMaterialAttribute *attr = static_cast<GPUMaterialAttribute *>(graph->attributes.first),
                            *next = nullptr;
       attr;
       attr = next)
  {
    next = attr->next;
    if (attr->users == 0) {
      BLI_freelinkN(&graph->attributes, attr);
    }
  }

  for (GPUMaterialTexture *tex = static_cast<GPUMaterialTexture *>(graph->textures.first),
                          *next = nullptr;
       tex;
       tex = next)
  {
    next = tex->next;
    if (tex->users == 0) {
      BLI_freelinkN(&graph->textures, tex);
    }
  }

  GPUUniformAttrList *uattrs = &graph->uniform_attrs;

  LISTBASE_FOREACH_MUTABLE (GPUUniformAttr *, attr, &uattrs->list) {
    if (attr->users == 0) {
      BLI_freelinkN(&uattrs->list, attr);
      uattrs->count--;
    }
  }

  LISTBASE_FOREACH_MUTABLE (GPULayerAttr *, attr, &graph->layer_attrs) {
    if (attr->users == 0) {
      BLI_freelinkN(&graph->layer_attrs, attr);
    }
  }
}

void gpu_node_graph_optimize(GPUNodeGraph *graph)
{
  /* Replace all uniform node links with constant. */
  LISTBASE_FOREACH (GPUNode *, node, &graph->nodes) {
    LISTBASE_FOREACH (GPUInput *, input, &node->inputs) {
      if (input->link) {
        if (input->link->link_type == GPU_NODE_LINK_UNIFORM) {
          input->link->link_type = GPU_NODE_LINK_CONSTANT;
        }
      }
      if (input->source == GPU_SOURCE_UNIFORM) {
        input->source = (input->type == GPU_CLOSURE) ? GPU_SOURCE_STRUCT : GPU_SOURCE_CONSTANT;
      }
    }
  }

  /* TODO: Consider performing other node graph optimizations here. */
}
