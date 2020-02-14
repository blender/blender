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

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_node_types.h"

#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_string.h"

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
    case GPU_NODE_LINK_COLORBAND:
      input->source = GPU_SOURCE_TEX;
      input->colorband = link->colorband;
      break;
    case GPU_NODE_LINK_IMAGE_BLENDER:
    case GPU_NODE_LINK_IMAGE_TILEMAP:
      input->source = GPU_SOURCE_TEX;
      input->ima = link->ima;
      input->iuser = link->iuser;
      break;
    case GPU_NODE_LINK_ATTR:
      input->source = GPU_SOURCE_ATTR;
      input->attr_type = link->attr_type;
      BLI_strncpy(input->attr_name, link->attr_name, sizeof(input->attr_name));
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

/* Creating Inputs */

GPUNodeLink *GPU_attribute(const CustomDataType type, const char *name)
{
  GPUNodeLink *link = gpu_node_link_create();
  link->link_type = GPU_NODE_LINK_ATTR;
  link->attr_name = name;
  /* Fall back to the UV layer, which matches old behavior. */
  if (type == CD_AUTO_FROM_NAME && name[0] == '\0') {
    link->attr_type = CD_MTFACE;
  }
  else {
    link->attr_type = type;
  }
  return link;
}

GPUNodeLink *GPU_constant(float *num)
{
  GPUNodeLink *link = gpu_node_link_create();
  link->link_type = GPU_NODE_LINK_CONSTANT;
  link->data = num;
  return link;
}

GPUNodeLink *GPU_uniform(float *num)
{
  GPUNodeLink *link = gpu_node_link_create();
  link->link_type = GPU_NODE_LINK_UNIFORM;
  link->data = num;
  return link;
}

GPUNodeLink *GPU_image(Image *ima, ImageUser *iuser)
{
  GPUNodeLink *link = gpu_node_link_create();
  link->link_type = GPU_NODE_LINK_IMAGE_BLENDER;
  link->ima = ima;
  link->iuser = iuser;
  return link;
}

GPUNodeLink *GPU_color_band(GPUMaterial *mat, int size, float *pixels, float *row)
{
  GPUNodeLink *link = gpu_node_link_create();
  link->link_type = GPU_NODE_LINK_COLORBAND;
  link->colorband = gpu_material_ramp_texture_row_set(mat, size, pixels, row);
  MEM_freeN(pixels);
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

  gpu_material_add_node(mat, node);

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

  gpu_material_add_node(material, node);

  return true;
}

GPUNodeLink *GPU_uniformbuffer_link_out(GPUMaterial *mat,
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

  gpu_inputs_free(&graph->inputs);
  graph->outlink = NULL;
}

/* Free both node graph and requested attributes and textures. */
void gpu_node_graph_free(GPUNodeGraph *graph)
{
  gpu_node_graph_free_nodes(graph);
  BLI_freelistN(&graph->attributes);
  BLI_freelistN(&graph->textures);
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
  GPUNode *node, *next;

  for (node = graph->nodes.first; node; node = node->next) {
    node->tag = false;
  }

  gpu_nodes_tag(graph->outlink);

  for (node = graph->nodes.first; node; node = next) {
    next = node->next;

    if (!node->tag) {
      BLI_remlink(&graph->nodes, node);
      gpu_node_free(node);
    }
  }
}
