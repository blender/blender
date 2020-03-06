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
 * \ingroup shdnodes
 */

#include "node_shader_util.h"

/* **************** VECTOR MATH ******************** */
static bNodeSocketTemplate sh_node_vector_math_in[] = {
    {SOCK_VECTOR, N_("Vector"), 0.0f, 0.0f, 0.0f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
    {SOCK_VECTOR, N_("Vector"), 0.0f, 0.0f, 0.0f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
    {SOCK_VECTOR, N_("Vector"), 0.0f, 0.0f, 0.0f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
    {SOCK_FLOAT, N_("Scale"), 1.0f, 1.0f, 1.0f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
    {-1, ""}};

static bNodeSocketTemplate sh_node_vector_math_out[] = {
    {SOCK_VECTOR, N_("Vector")}, {SOCK_FLOAT, N_("Value")}, {-1, ""}};

static int gpu_shader_vector_math(GPUMaterial *mat,
                                  bNode *node,
                                  bNodeExecData *UNUSED(execdata),
                                  GPUNodeStack *in,
                                  GPUNodeStack *out)
{
  static const char *names[] = {
      [NODE_VECTOR_MATH_ADD] = "vector_math_add",
      [NODE_VECTOR_MATH_SUBTRACT] = "vector_math_subtract",
      [NODE_VECTOR_MATH_MULTIPLY] = "vector_math_multiply",
      [NODE_VECTOR_MATH_DIVIDE] = "vector_math_divide",

      [NODE_VECTOR_MATH_CROSS_PRODUCT] = "vector_math_cross",
      [NODE_VECTOR_MATH_PROJECT] = "vector_math_project",
      [NODE_VECTOR_MATH_REFLECT] = "vector_math_reflect",
      [NODE_VECTOR_MATH_DOT_PRODUCT] = "vector_math_dot",

      [NODE_VECTOR_MATH_DISTANCE] = "vector_math_distance",
      [NODE_VECTOR_MATH_LENGTH] = "vector_math_length",
      [NODE_VECTOR_MATH_SCALE] = "vector_math_scale",
      [NODE_VECTOR_MATH_NORMALIZE] = "vector_math_normalize",

      [NODE_VECTOR_MATH_SNAP] = "vector_math_snap",
      [NODE_VECTOR_MATH_FLOOR] = "vector_math_floor",
      [NODE_VECTOR_MATH_CEIL] = "vector_math_ceil",
      [NODE_VECTOR_MATH_MODULO] = "vector_math_modulo",
      [NODE_VECTOR_MATH_FRACTION] = "vector_math_fraction",
      [NODE_VECTOR_MATH_ABSOLUTE] = "vector_math_absolute",
      [NODE_VECTOR_MATH_MINIMUM] = "vector_math_minimum",
      [NODE_VECTOR_MATH_MAXIMUM] = "vector_math_maximum",
      [NODE_VECTOR_MATH_WRAP] = "vector_math_wrap",
      [NODE_VECTOR_MATH_SINE] = "vector_math_sine",
      [NODE_VECTOR_MATH_COSINE] = "vector_math_cosine",
      [NODE_VECTOR_MATH_TANGENT] = "vector_math_tangent",
  };

  if (node->custom1 < ARRAY_SIZE(names) && names[node->custom1]) {
    return GPU_stack_link(mat, node, names[node->custom1], in, out);
  }
  else {
    return 0;
  }
}

static void node_shader_update_vector_math(bNodeTree *UNUSED(ntree), bNode *node)
{
  bNodeSocket *sockB = BLI_findlink(&node->inputs, 1);
  bNodeSocket *sockC = BLI_findlink(&node->inputs, 2);
  bNodeSocket *sockScale = nodeFindSocket(node, SOCK_IN, "Scale");

  bNodeSocket *sockVector = nodeFindSocket(node, SOCK_OUT, "Vector");
  bNodeSocket *sockValue = nodeFindSocket(node, SOCK_OUT, "Value");

  nodeSetSocketAvailability(sockB,
                            !ELEM(node->custom1,
                                  NODE_VECTOR_MATH_SINE,
                                  NODE_VECTOR_MATH_COSINE,
                                  NODE_VECTOR_MATH_TANGENT,
                                  NODE_VECTOR_MATH_CEIL,
                                  NODE_VECTOR_MATH_SCALE,
                                  NODE_VECTOR_MATH_FLOOR,
                                  NODE_VECTOR_MATH_LENGTH,
                                  NODE_VECTOR_MATH_ABSOLUTE,
                                  NODE_VECTOR_MATH_FRACTION,
                                  NODE_VECTOR_MATH_NORMALIZE));
  nodeSetSocketAvailability(sockC, ELEM(node->custom1, NODE_VECTOR_MATH_WRAP));
  nodeSetSocketAvailability(sockScale, node->custom1 == NODE_VECTOR_MATH_SCALE);
  nodeSetSocketAvailability(sockVector,
                            !ELEM(node->custom1,
                                  NODE_VECTOR_MATH_LENGTH,
                                  NODE_VECTOR_MATH_DISTANCE,
                                  NODE_VECTOR_MATH_DOT_PRODUCT));
  nodeSetSocketAvailability(sockValue,
                            ELEM(node->custom1,
                                 NODE_VECTOR_MATH_LENGTH,
                                 NODE_VECTOR_MATH_DISTANCE,
                                 NODE_VECTOR_MATH_DOT_PRODUCT));

  /* Labels */
  if (sockB->label[0] != '\0') {
    sockB->label[0] = '\0';
  }
  if (sockC->label[0] != '\0') {
    sockC->label[0] = '\0';
  }
  switch (node->custom1) {
    case NODE_VECTOR_MATH_WRAP:
      node_sock_label(sockB, "Max");
      node_sock_label(sockC, "Min");
      break;
    case NODE_VECTOR_MATH_SNAP:
      node_sock_label(sockB, "Increment");
      break;
  }
}

void register_node_type_sh_vect_math(void)
{
  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_VECTOR_MATH, "Vector Math", NODE_CLASS_OP_VECTOR, 0);
  node_type_socket_templates(&ntype, sh_node_vector_math_in, sh_node_vector_math_out);
  node_type_label(&ntype, node_vector_math_label);
  node_type_gpu(&ntype, gpu_shader_vector_math);
  node_type_update(&ntype, node_shader_update_vector_math);

  nodeRegisterType(&ntype);
}
