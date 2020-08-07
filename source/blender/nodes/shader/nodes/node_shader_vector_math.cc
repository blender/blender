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

static const char *gpu_shader_get_name(int mode)
{
  switch (mode) {
    case NODE_VECTOR_MATH_ADD:
      return "vector_math_add";
    case NODE_VECTOR_MATH_SUBTRACT:
      return "vector_math_subtract";
    case NODE_VECTOR_MATH_MULTIPLY:
      return "vector_math_multiply";
    case NODE_VECTOR_MATH_DIVIDE:
      return "vector_math_divide";

    case NODE_VECTOR_MATH_CROSS_PRODUCT:
      return "vector_math_cross";
    case NODE_VECTOR_MATH_PROJECT:
      return "vector_math_project";
    case NODE_VECTOR_MATH_REFLECT:
      return "vector_math_reflect";
    case NODE_VECTOR_MATH_DOT_PRODUCT:
      return "vector_math_dot";

    case NODE_VECTOR_MATH_DISTANCE:
      return "vector_math_distance";
    case NODE_VECTOR_MATH_LENGTH:
      return "vector_math_length";
    case NODE_VECTOR_MATH_SCALE:
      return "vector_math_scale";
    case NODE_VECTOR_MATH_NORMALIZE:
      return "vector_math_normalize";

    case NODE_VECTOR_MATH_SNAP:
      return "vector_math_snap";
    case NODE_VECTOR_MATH_FLOOR:
      return "vector_math_floor";
    case NODE_VECTOR_MATH_CEIL:
      return "vector_math_ceil";
    case NODE_VECTOR_MATH_MODULO:
      return "vector_math_modulo";
    case NODE_VECTOR_MATH_FRACTION:
      return "vector_math_fraction";
    case NODE_VECTOR_MATH_ABSOLUTE:
      return "vector_math_absolute";
    case NODE_VECTOR_MATH_MINIMUM:
      return "vector_math_minimum";
    case NODE_VECTOR_MATH_MAXIMUM:
      return "vector_math_maximum";
    case NODE_VECTOR_MATH_WRAP:
      return "vector_math_wrap";
    case NODE_VECTOR_MATH_SINE:
      return "vector_math_sine";
    case NODE_VECTOR_MATH_COSINE:
      return "vector_math_cosine";
    case NODE_VECTOR_MATH_TANGENT:
      return "vector_math_tangent";
  }

  return nullptr;
}

static int gpu_shader_vector_math(GPUMaterial *mat,
                                  bNode *node,
                                  bNodeExecData *UNUSED(execdata),
                                  GPUNodeStack *in,
                                  GPUNodeStack *out)
{
  const char *name = gpu_shader_get_name(node->custom1);
  if (name != nullptr) {
    return GPU_stack_link(mat, node, name, in, out);
  }

  return 0;
}

static void node_shader_update_vector_math(bNodeTree *UNUSED(ntree), bNode *node)
{
  bNodeSocket *sockB = (bNodeSocket *)BLI_findlink(&node->inputs, 1);
  bNodeSocket *sockC = (bNodeSocket *)BLI_findlink(&node->inputs, 2);
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

static const blender::fn::MultiFunction &get_multi_function(
    blender::nodes::NodeMFNetworkBuilder &builder)
{
  using blender::float3;

  const int mode = builder.bnode().custom1;
  switch (mode) {
    case NODE_VECTOR_MATH_ADD: {
      static blender::fn::CustomMF_SI_SI_SO<float3, float3, float3> fn{
          "Add", [](float3 a, float3 b) { return a + b; }};
      return fn;
    }
    case NODE_VECTOR_MATH_SUBTRACT: {
      static blender::fn::CustomMF_SI_SI_SO<float3, float3, float3> fn{
          "Subtract", [](float3 a, float3 b) { return a - b; }};
      return fn;
    }
    case NODE_VECTOR_MATH_MULTIPLY: {
      static blender::fn::CustomMF_SI_SI_SO<float3, float3, float3> fn{
          "Multiply", [](float3 a, float3 b) { return a * b; }};
      return fn;
    }
    case NODE_VECTOR_MATH_DIVIDE: {
      static blender::fn::CustomMF_SI_SI_SO<float3, float3, float3> fn{
          "Divide", [](float3 a, float3 b) { return float3::safe_divide(a, b); }};
      return fn;
    }

    case NODE_VECTOR_MATH_CROSS_PRODUCT: {
      static blender::fn::CustomMF_SI_SI_SO<float3, float3, float3> fn{
          "Cross Product", float3::cross_high_precision};
      return fn;
    }
    case NODE_VECTOR_MATH_PROJECT: {
      static blender::fn::CustomMF_SI_SI_SO<float3, float3, float3> fn{"Project", float3::project};
      return fn;
    }
    case NODE_VECTOR_MATH_REFLECT: {
      static blender::fn::CustomMF_SI_SI_SO<float3, float3, float3> fn{
          "Reflect", [](float3 a, float3 b) { return a.reflected(b); }};
      return fn;
    }
    case NODE_VECTOR_MATH_DOT_PRODUCT: {
      static blender::fn::CustomMF_SI_SI_SO<float3, float3, float> fn{"Dot Product", float3::dot};
      return fn;
    }

    case NODE_VECTOR_MATH_DISTANCE: {
      static blender::fn::CustomMF_SI_SI_SO<float3, float3, float> fn{"Distance",
                                                                      float3::distance};
      return fn;
    }
    case NODE_VECTOR_MATH_LENGTH: {
      static blender::fn::CustomMF_SI_SO<float3, float> fn{"Length",
                                                           [](float3 a) { return a.length(); }};
      return fn;
    }
    case NODE_VECTOR_MATH_SCALE: {
      static blender::fn::CustomMF_SI_SI_SO<float3, float, float3> fn{
          "Scale", [](float3 a, float factor) { return a * factor; }};
      return fn;
    }
    case NODE_VECTOR_MATH_NORMALIZE: {
      static blender::fn::CustomMF_SI_SO<float3, float3> fn{
          "Normalize", [](float3 a) { return a.normalized(); }};
      return fn;
    }

    case NODE_VECTOR_MATH_SNAP: {
      return builder.get_not_implemented_fn();
    }
    case NODE_VECTOR_MATH_FLOOR: {
      return builder.get_not_implemented_fn();
    }
    case NODE_VECTOR_MATH_CEIL: {
      return builder.get_not_implemented_fn();
    }
    case NODE_VECTOR_MATH_MODULO: {
      return builder.get_not_implemented_fn();
    }
    case NODE_VECTOR_MATH_FRACTION: {
      return builder.get_not_implemented_fn();
    }
    case NODE_VECTOR_MATH_ABSOLUTE: {
      return builder.get_not_implemented_fn();
    }
    case NODE_VECTOR_MATH_MINIMUM: {
      return builder.get_not_implemented_fn();
    }
    case NODE_VECTOR_MATH_MAXIMUM: {
      return builder.get_not_implemented_fn();
    }
    case NODE_VECTOR_MATH_WRAP: {
      return builder.get_not_implemented_fn();
    }
    case NODE_VECTOR_MATH_SINE: {
      return builder.get_not_implemented_fn();
    }
    case NODE_VECTOR_MATH_COSINE: {
      return builder.get_not_implemented_fn();
    }
    case NODE_VECTOR_MATH_TANGENT: {
      return builder.get_not_implemented_fn();
    }

    default:
      BLI_assert(false);
      return builder.get_not_implemented_fn();
  };
}

static void sh_node_vector_math_expand_in_mf_network(blender::nodes::NodeMFNetworkBuilder &builder)
{
  const blender::fn::MultiFunction &fn = get_multi_function(builder);
  builder.set_matching_fn(fn);
}

void register_node_type_sh_vect_math(void)
{
  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_VECTOR_MATH, "Vector Math", NODE_CLASS_OP_VECTOR, 0);
  node_type_socket_templates(&ntype, sh_node_vector_math_in, sh_node_vector_math_out);
  node_type_label(&ntype, node_vector_math_label);
  node_type_gpu(&ntype, gpu_shader_vector_math);
  node_type_update(&ntype, node_shader_update_vector_math);
  ntype.expand_in_mf_network = sh_node_vector_math_expand_in_mf_network;

  nodeRegisterType(&ntype);
}
