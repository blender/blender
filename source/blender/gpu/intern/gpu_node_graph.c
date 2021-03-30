/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * Intermediate node graph for generating GLSL shaders.
 */

#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_node_types.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "GPU_texture.h"

#include "gpu_material_library.h"
#include "gpu_node_graph.h"

/* Node Link Functions */

static GPUNodeLink *gpu_node_link_create(void)
{
  GPUNodeLink *link = MEM_callocN(sizeof(GPUNodeLink), "GPUNodeLink");
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
      link->output->link = NULL;
    }
    MEM_freeN(link);
  }
}

/* Node Functions */

static GPUNode *gpu_node_create(const char *name)
{
  GPUNode *node = MEM_callocN(sizeof(GPUNode), "GPUNode");

  node->name = name;

  return node;
}

static void gpu_node_input_link(GPUNode *node, GPUNodeLink *link, const eGPUType type)
{
  GPUInput *input;
  GPUNode *outnode;
  const char *name;

  if (link->link_type == GPU_NODE_LINK_OUTPUT) {
    outnode = link->output->node;
    name = outnode->name;
    input = outnode->inputs.first;

    if ((STR_ELEM(name, "set_value", "set_rgb", "set_rgba")) && (input->type == type)) {
      input = MEM_dupallocN(outnode->inputs.first);
      if (input->link) {
        input->link->users++;
      }
      BLI_addtail(&node->inputs, input);
      return;
    }
  }

  input = MEM_callocN(sizeof(GPUInput), "GPUInput");
  input->node = node;
  input->type = type;

  switch (link->link_type) {
    case GPU_NODE_LINK_BUILTIN:
      input->source = GPU_SOURCE_BUILTIN;
      input->builtin = link->builtin;
      break;
    case GPU_NODE_LINK_OUTPUT:
      input->source = GPU_SOURCE_OUTPUT;
      input->link = link;
      link->users++;
      break;
    case GPU_NODE_LINK_IMAGE:
    case GPU_NODE_LINK_IMAGE_TILED:
    case GPU_NODE_LINK_COLORBAND:
      input->source = GPU_SOURCE_TEX;
      input->texture = link->texture;
      break;
    case GPU_NODE_LINK_IMAGE_TILED_MAPPING:
      input->source = GPU_SOURCE_TEX_TILED_MAPPING;
      input->texture = link->texture;
      break;
    case GPU_NODE_LINK_VOLUME_GRID:
      input->source = GPU_SOURCE_VOLUME_GRID;
      input->volume_grid = link->volume_grid;
      break;
    case GPU_NODE_LINK_VOLUME_GRID_TRANSFORM:
      input->source = GPU_SOURCE_VOLUME_GRID_TRANSFORM;
      input->volume_grid = link->volume_grid;
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
    case GPU_NODE_LINK_CONSTANT:
      input->source = (type == GPU_CLOSURE) ? GPU_SOURCE_STRUCT : GPU_SOURCE_CONSTANT;
      break;
    case GPU_NODE_LINK_UNIFORM:
      input->source = GPU_SOURCE_UNIFORM;
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
    /* For now INT is supported as float. */
    case SOCK_INT:
    case SOCK_FLOAT:
      return "set_value";
    case SOCK_VECTOR:
      return "set_rgb";
    case SOCK_RGBA:
      return "set_rgba";
    default:
      BLI_assert(!"No gpu function for non-supported eNodeSocketDatatype");
      return NULL;
  }
}

/**
 * Link stack uniform buffer.
 * This is called for the input/output sockets that are note connected.
 */
static GPUNodeLink *gpu_uniformbuffer_link(GPUMaterial *mat,
                                           bNode *node,
                                           GPUNodeStack *stack,
                                           const int index,
                                           const eNodeSocketInOut in_out)
{
  bNodeSocket *socket;

  if (in_out == SOCK_IN) {
    socket = BLI_findlink(&node->inputs, index);
  }
  else {
    socket = BLI_findlink(&node->outputs, index);
  }

  BLI_assert(socket != NULL);
  BLI_assert(socket->in_out == in_out);

  if ((socket->flag & SOCK_HIDE_VALUE) == 0) {
    GPUNodeLink *link;
    switch (socket->type) {
      case SOCK_FLOAT: {
        bNodeSocketValueFloat *socket_data = socket->default_value;
        link = GPU_uniform(&socket_data->value);
        break;
      }
      case SOCK_VECTOR: {
        bNodeSocketValueVector *socket_data = socket->default_value;
        link = GPU_uniform(socket_data->value);
        break;
      }
      case SOCK_RGBA: {
        bNodeSocketValueRGBA *socket_data = socket->default_value;
        link = GPU_uniform(socket_data->value);
        break;
      }
      default:
        return NULL;
        break;
    }

    if (in_out == SOCK_IN) {
      GPU_link(mat, gpu_uniform_set_function_from_type(socket->type), link, &stack->link);
    }
    return link;
  }
  return NULL;
}

static void gpu_node_input_socket(
    GPUMaterial *material, bNode *bnode, GPUNode *node, GPUNodeStack *sock, const int index)
{
  if (sock->link) {
    gpu_node_input_link(node, sock->link, sock->type);
  }
  else if ((material != NULL) &&
           (gpu_uniformbuffer_link(material, bnode, sock, index, SOCK_IN) != NULL)) {
    gpu_node_input_link(node, sock->link, sock->type);
  }
  else {
    gpu_node_input_link(node, GPU_constant(sock->vec), sock->type);
  }
}

static void gpu_node_output(GPUNode *node, const eGPUType type, GPUNodeLink **link)
{
  GPUOutput *output = MEM_callocN(sizeof(GPUOutput), "GPUOutput");

  output->type = type;
  output->node = node;

  if (link) {
    *link = output->link = gpu_node_link_create();
    output->link->link_type = GPU_NODE_LINK_OUTPUT;
    output->link->output = output;

    /* note: the caller owns the reference to the link, GPUOutput
     * merely points to it, and if the node is destroyed it will
     * set that pointer to NULL */
  }

  BLI_addtail(&node->outputs, output);
}

/* Uniform Attribute Functions */

static int uniform_attr_sort_cmp(const void *a, const void *b)
{
  const GPUUniformAttr *attr_a = a, *attr_b = b;

  int cmps = strcmp(attr_a->name, attr_b->name);
  if (cmps != 0) {
    return cmps > 0 ? 1 : 0;
  }

  return (attr_a->use_dupli && !attr_b->use_dupli);
}

static unsigned int uniform_attr_list_hash(const void *key)
{
  const GPUUniformAttrList *attrs = key;
  return attrs->hash_code;
}

static bool uniform_attr_list_cmp(const void *a, const void *b)
{
  const GPUUniformAttrList *set_a = a, *set_b = b;

  if (set_a->hash_code != set_b->hash_code || set_a->count != set_b->count) {
    return true;
  }

  GPUUniformAttr *attr_a = set_a->list.first, *attr_b = set_b->list.first;

  for (; attr_a && attr_b; attr_a = attr_a->next, attr_b = attr_b->next) {
    if (!STREQ(attr_a->name, attr_b->name) || attr_a->use_dupli != attr_b->use_dupli) {
      return true;
    }
  }

  return attr_a || attr_b;
}

struct GHash *GPU_uniform_attr_list_hash_new(const char *info)
{
  return BLI_ghash_new(uniform_attr_list_hash, uniform_attr_list_cmp, info);
}

void GPU_uniform_attr_list_copy(GPUUniformAttrList *dest, GPUUniformAttrList *src)
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

    attrs->hash_code ^= BLI_ghashutil_strhash_p(attr->name);

    if (attr->use_dupli) {
      attrs->hash_code ^= BLI_ghashutil_uinthash(attr->id);
    }
  }
}

/* Attributes and Textures */

/** Add a new varying attribute of given type and name. Returns NULL if out of slots. */
static GPUMaterialAttribute *gpu_node_graph_add_attribute(GPUNodeGraph *graph,
                                                          CustomDataType type,
                                                          const char *name)
{
  /* Fall back to the UV layer, which matches old behavior. */
  if (type == CD_AUTO_FROM_NAME && name[0] == '\0') {
    type = CD_MTFACE;
  }

  /* Find existing attribute. */
  int num_attributes = 0;
  GPUMaterialAttribute *attr = graph->attributes.first;
  for (; attr; attr = attr->next) {
    if (attr->type == type && STREQ(attr->name, name)) {
      break;
    }
    num_attributes++;
  }

  /* Add new requested attribute if it's within GPU limits. */
  if (attr == NULL && num_attributes < GPU_MAX_ATTR) {
    attr = MEM_callocN(sizeof(*attr), __func__);
    attr->type = type;
    STRNCPY(attr->name, name);
    attr->id = num_attributes;
    BLI_addtail(&graph->attributes, attr);
  }

  if (attr != NULL) {
    attr->users++;
  }

  return attr;
}

/** Add a new uniform attribute of given type and name. Returns NULL if out of slots. */
static GPUUniformAttr *gpu_node_graph_add_uniform_attribute(GPUNodeGraph *graph,
                                                            const char *name,
                                                            bool use_dupli)
{
  /* Find existing attribute. */
  GPUUniformAttrList *attrs = &graph->uniform_attrs;
  GPUUniformAttr *attr = attrs->list.first;

  for (; attr; attr = attr->next) {
    if (STREQ(attr->name, name) && attr->use_dupli == use_dupli) {
      break;
    }
  }

  /* Add new requested attribute if it's within GPU limits. */
  if (attr == NULL && attrs->count < GPU_MAX_UNIFORM_ATTR) {
    attr = MEM_callocN(sizeof(*attr), __func__);
    STRNCPY(attr->name, name);
    attr->use_dupli = use_dupli;
    attr->id = -1;
    BLI_addtail(&attrs->list, attr);
    attrs->count++;
  }

  if (attr != NULL) {
    attr->users++;
  }

  return attr;
}

static GPUMaterialTexture *gpu_node_graph_add_texture(GPUNodeGraph *graph,
                                                      Image *ima,
                                                      ImageUser *iuser,
                                                      struct GPUTexture **colorband,
                                                      GPUNodeLinkType link_type,
                                                      eGPUSamplerState sampler_state)
{
  /* Find existing texture. */
  int num_textures = 0;
  GPUMaterialTexture *tex = graph->textures.first;
  for (; tex; tex = tex->next) {
    if (tex->ima == ima && tex->colorband == colorband && tex->sampler_state == sampler_state) {
      break;
    }
    num_textures++;
  }

  /* Add new requested texture. */
  if (tex == NULL) {
    tex = MEM_callocN(sizeof(*tex), __func__);
    tex->ima = ima;
    tex->iuser = iuser;
    tex->colorband = colorband;
    tex->sampler_state = sampler_state;
    BLI_snprintf(tex->sampler_name, sizeof(tex->sampler_name), "samp%d", num_textures);
    if (ELEM(link_type, GPU_NODE_LINK_IMAGE_TILED, GPU_NODE_LINK_IMAGE_TILED_MAPPING)) {
      BLI_snprintf(
          tex->tiled_mapping_name, sizeof(tex->tiled_mapping_name), "tsamp%d", num_textures);
    }
    BLI_addtail(&graph->textures, tex);
  }

  tex->users++;

  return tex;
}

static GPUMaterialVolumeGrid *gpu_node_graph_add_volume_grid(GPUNodeGraph *graph,
                                                             const char *name,
                                                             eGPUVolumeDefaultValue default_value)
{
  /* Find existing volume grid. */
  int num_grids = 0;
  GPUMaterialVolumeGrid *grid = graph->volume_grids.first;
  for (; grid; grid = grid->next) {
    if (STREQ(grid->name, name) && grid->default_value == default_value) {
      break;
    }
    num_grids++;
  }

  /* Add new requested volume grid. */
  if (grid == NULL) {
    grid = MEM_callocN(sizeof(*grid), __func__);
    grid->name = BLI_strdup(name);
    grid->default_value = default_value;
    BLI_snprintf(grid->sampler_name, sizeof(grid->sampler_name), "vsamp%d", num_grids);
    BLI_snprintf(grid->transform_name, sizeof(grid->transform_name), "vtfm%d", num_grids);
    BLI_addtail(&graph->volume_grids, grid);
  }

  grid->users++;

  return grid;
}

/* Creating Inputs */

GPUNodeLink *GPU_attribute(GPUMaterial *mat, const CustomDataType type, const char *name)
{
  GPUNodeGraph *graph = gpu_material_node_graph(mat);
  GPUMaterialAttribute *attr = gpu_node_graph_add_attribute(graph, type, name);

  /* Dummy fallback if out of slots. */
  if (attr == NULL) {
    static const float zero_data[GPU_MAX_CONSTANT_DATA] = {0.0f};
    return GPU_constant(zero_data);
  }

  GPUNodeLink *link = gpu_node_link_create();
  link->link_type = GPU_NODE_LINK_ATTR;
  link->attr = attr;
  return link;
}

GPUNodeLink *GPU_uniform_attribute(GPUMaterial *mat, const char *name, bool use_dupli)
{
  GPUNodeGraph *graph = gpu_material_node_graph(mat);
  GPUUniformAttr *attr = gpu_node_graph_add_uniform_attribute(graph, name, use_dupli);

  /* Dummy fallback if out of slots. */
  if (attr == NULL) {
    static const float zero_data[GPU_MAX_CONSTANT_DATA] = {0.0f};
    return GPU_constant(zero_data);
  }

  GPUNodeLink *link = gpu_node_link_create();
  link->link_type = GPU_NODE_LINK_UNIFORM_ATTR;
  link->uniform_attr = attr;
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

GPUNodeLink *GPU_image(GPUMaterial *mat,
                       Image *ima,
                       ImageUser *iuser,
                       eGPUSamplerState sampler_state)
{
  GPUNodeGraph *graph = gpu_material_node_graph(mat);
  GPUNodeLink *link = gpu_node_link_create();
  link->link_type = GPU_NODE_LINK_IMAGE;
  link->texture = gpu_node_graph_add_texture(
      graph, ima, iuser, NULL, link->link_type, sampler_state);
  return link;
}

GPUNodeLink *GPU_image_tiled(GPUMaterial *mat,
                             Image *ima,
                             ImageUser *iuser,
                             eGPUSamplerState sampler_state)
{
  GPUNodeGraph *graph = gpu_material_node_graph(mat);
  GPUNodeLink *link = gpu_node_link_create();
  link->link_type = GPU_NODE_LINK_IMAGE_TILED;
  link->texture = gpu_node_graph_add_texture(
      graph, ima, iuser, NULL, link->link_type, sampler_state);
  return link;
}

GPUNodeLink *GPU_image_tiled_mapping(GPUMaterial *mat, Image *ima, ImageUser *iuser)
{
  GPUNodeGraph *graph = gpu_material_node_graph(mat);
  GPUNodeLink *link = gpu_node_link_create();
  link->link_type = GPU_NODE_LINK_IMAGE_TILED_MAPPING;
  link->texture = gpu_node_graph_add_texture(
      graph, ima, iuser, NULL, link->link_type, GPU_SAMPLER_MAX);
  return link;
}

GPUNodeLink *GPU_color_band(GPUMaterial *mat, int size, float *pixels, float *row)
{
  struct GPUTexture **colorband = gpu_material_ramp_texture_row_set(mat, size, pixels, row);
  MEM_freeN(pixels);

  GPUNodeGraph *graph = gpu_material_node_graph(mat);
  GPUNodeLink *link = gpu_node_link_create();
  link->link_type = GPU_NODE_LINK_COLORBAND;
  link->texture = gpu_node_graph_add_texture(
      graph, NULL, NULL, colorband, link->link_type, GPU_SAMPLER_MAX);
  return link;
}

GPUNodeLink *GPU_volume_grid(GPUMaterial *mat,
                             const char *name,
                             eGPUVolumeDefaultValue default_value)
{
  /* NOTE: this could be optimized by automatically merging duplicate
   * lookups of the same attribute. */
  GPUNodeGraph *graph = gpu_material_node_graph(mat);
  GPUNodeLink *link = gpu_node_link_create();
  link->link_type = GPU_NODE_LINK_VOLUME_GRID;
  link->volume_grid = gpu_node_graph_add_volume_grid(graph, name, default_value);

  GPUNodeLink *transform_link = gpu_node_link_create();
  transform_link->link_type = GPU_NODE_LINK_VOLUME_GRID_TRANSFORM;
  transform_link->volume_grid = link->volume_grid;
  transform_link->volume_grid->users++;

  /* Two special cases, where we adjust the output values of smoke grids to
   * bring the into standard range without having to modify the grid values. */
  if (STREQ(name, "color")) {
    GPU_link(mat, "node_attribute_volume_color", link, transform_link, &link);
  }
  else if (STREQ(name, "temperature")) {
    GPU_link(mat, "node_attribute_volume_temperature", link, transform_link, &link);
  }
  else {
    GPU_link(mat, "node_attribute_volume", link, transform_link, &link);
  }

  return link;
}

GPUNodeLink *GPU_builtin(eGPUBuiltin builtin)
{
  GPUNodeLink *link = gpu_node_link_create();
  link->link_type = GPU_NODE_LINK_BUILTIN;
  link->builtin = builtin;
  return link;
}

/* Creating Nodes */

bool GPU_link(GPUMaterial *mat, const char *name, ...)
{
  GSet *used_libraries = gpu_material_used_libraries(mat);
  GPUNode *node;
  GPUFunction *function;
  GPUNodeLink *link, **linkptr;
  va_list params;
  int i;

  function = gpu_material_library_use_function(used_libraries, name);
  if (!function) {
    fprintf(stderr, "GPU failed to find function %s\n", name);
    return false;
  }

  node = gpu_node_create(name);

  va_start(params, name);
  for (i = 0; i < function->totparam; i++) {
    if (function->paramqual[i] != FUNCTION_QUAL_IN) {
      linkptr = va_arg(params, GPUNodeLink **);
      gpu_node_output(node, function->paramtype[i], linkptr);
    }
    else {
      link = va_arg(params, GPUNodeLink *);
      gpu_node_input_link(node, link, function->paramtype[i]);
    }
  }
  va_end(params);

  GPUNodeGraph *graph = gpu_material_node_graph(mat);
  BLI_addtail(&graph->nodes, node);

  return true;
}

bool GPU_stack_link(GPUMaterial *material,
                    bNode *bnode,
                    const char *name,
                    GPUNodeStack *in,
                    GPUNodeStack *out,
                    ...)
{
  GSet *used_libraries = gpu_material_used_libraries(material);
  GPUNode *node;
  GPUFunction *function;
  GPUNodeLink *link, **linkptr;
  va_list params;
  int i, totin, totout;

  function = gpu_material_library_use_function(used_libraries, name);
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

  va_start(params, out);
  for (i = 0; i < function->totparam; i++) {
    if (function->paramqual[i] != FUNCTION_QUAL_IN) {
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
          gpu_node_input_socket(NULL, NULL, node, link->socket, -1);
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
  va_end(params);

  GPUNodeGraph *graph = gpu_material_node_graph(material);
  BLI_addtail(&graph->nodes, node);

  return true;
}

GPUNodeLink *GPU_uniformbuf_link_out(GPUMaterial *mat,
                                     bNode *node,
                                     GPUNodeStack *stack,
                                     const int index)
{
  return gpu_uniformbuffer_link(mat, node, stack, index, SOCK_OUT);
}

/* Node Graph */

static void gpu_inputs_free(ListBase *inputs)
{
  GPUInput *input;

  for (input = inputs->first; input; input = input->next) {
    if (input->source == GPU_SOURCE_ATTR) {
      input->attr->users--;
    }
    else if (input->source == GPU_SOURCE_UNIFORM_ATTR) {
      input->uniform_attr->users--;
    }
    else if (ELEM(input->source, GPU_SOURCE_TEX, GPU_SOURCE_TEX_TILED_MAPPING)) {
      input->texture->users--;
    }
    else if (ELEM(input->source, GPU_SOURCE_VOLUME_GRID, GPU_SOURCE_VOLUME_GRID_TRANSFORM)) {
      input->volume_grid->users--;
    }

    if (input->link) {
      gpu_node_link_free(input->link);
    }
  }

  BLI_freelistN(inputs);
}

static void gpu_node_free(GPUNode *node)
{
  GPUOutput *output;

  gpu_inputs_free(&node->inputs);

  for (output = node->outputs.first; output; output = output->next) {
    if (output->link) {
      output->link->output = NULL;
      gpu_node_link_free(output->link);
    }
  }

  BLI_freelistN(&node->outputs);
  MEM_freeN(node);
}

/* Free intermediate node graph. */
void gpu_node_graph_free_nodes(GPUNodeGraph *graph)
{
  GPUNode *node;

  while ((node = BLI_pophead(&graph->nodes))) {
    gpu_node_free(node);
  }

  graph->outlink = NULL;
}

/* Free both node graph and requested attributes and textures. */
void gpu_node_graph_free(GPUNodeGraph *graph)
{
  BLI_freelistN(&graph->outlink_aovs);
  gpu_node_graph_free_nodes(graph);

  LISTBASE_FOREACH (GPUMaterialVolumeGrid *, grid, &graph->volume_grids) {
    MEM_SAFE_FREE(grid->name);
  }
  BLI_freelistN(&graph->volume_grids);
  BLI_freelistN(&graph->textures);
  BLI_freelistN(&graph->attributes);
  GPU_uniform_attr_list_free(&graph->uniform_attrs);
}

/* Prune Unused Nodes */

static void gpu_nodes_tag(GPUNodeLink *link)
{
  GPUNode *node;
  GPUInput *input;

  if (!link->output) {
    return;
  }

  node = link->output->node;
  if (node->tag) {
    return;
  }

  node->tag = true;
  for (input = node->inputs.first; input; input = input->next) {
    if (input->link) {
      gpu_nodes_tag(input->link);
    }
  }
}

void gpu_node_graph_prune_unused(GPUNodeGraph *graph)
{
  LISTBASE_FOREACH (GPUNode *, node, &graph->nodes) {
    node->tag = false;
  }

  gpu_nodes_tag(graph->outlink);
  LISTBASE_FOREACH (GPUNodeGraphOutputLink *, aovlink, &graph->outlink_aovs) {
    gpu_nodes_tag(aovlink->outlink);
  }

  for (GPUNode *node = graph->nodes.first, *next = NULL; node; node = next) {
    next = node->next;

    if (!node->tag) {
      BLI_remlink(&graph->nodes, node);
      gpu_node_free(node);
    }
  }

  for (GPUMaterialAttribute *attr = graph->attributes.first, *next = NULL; attr; attr = next) {
    next = attr->next;
    if (attr->users == 0) {
      BLI_freelinkN(&graph->attributes, attr);
    }
  }

  for (GPUMaterialTexture *tex = graph->textures.first, *next = NULL; tex; tex = next) {
    next = tex->next;
    if (tex->users == 0) {
      BLI_freelinkN(&graph->textures, tex);
    }
  }

  for (GPUMaterialVolumeGrid *grid = graph->volume_grids.first, *next = NULL; grid; grid = next) {
    next = grid->next;
    if (grid->users == 0) {
      MEM_SAFE_FREE(grid->name);
      BLI_freelinkN(&graph->volume_grids, grid);
    }
  }

  GPUUniformAttrList *uattrs = &graph->uniform_attrs;

  LISTBASE_FOREACH_MUTABLE (GPUUniformAttr *, attr, &uattrs->list) {
    if (attr->users == 0) {
      BLI_freelinkN(&uattrs->list, attr);
      uattrs->count--;
    }
  }
}
