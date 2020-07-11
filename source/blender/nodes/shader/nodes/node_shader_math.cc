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

/* **************** SCALAR MATH ******************** */
static bNodeSocketTemplate sh_node_math_in[] = {
    {SOCK_FLOAT, N_("Value"), 0.5f, 0.5f, 0.5f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
    {SOCK_FLOAT, N_("Value"), 0.5f, 0.5f, 0.5f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
    {SOCK_FLOAT, N_("Value"), 0.0f, 0.5f, 0.5f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
    {-1, ""}};

static bNodeSocketTemplate sh_node_math_out[] = {{SOCK_FLOAT, N_("Value")}, {-1, ""}};

static const char *gpu_shader_get_name(int mode)
{
  switch (mode) {
    case NODE_MATH_ADD:
      return "math_add";
    case NODE_MATH_SUBTRACT:
      return "math_subtract";
    case NODE_MATH_MULTIPLY:
      return "math_multiply";
    case NODE_MATH_DIVIDE:
      return "math_divide";
    case NODE_MATH_MULTIPLY_ADD:
      return "math_multiply_add";

    case NODE_MATH_POWER:
      return "math_power";
    case NODE_MATH_LOGARITHM:
      return "math_logarithm";
    case NODE_MATH_EXPONENT:
      return "math_exponent";
    case NODE_MATH_SQRT:
      return "math_sqrt";
    case NODE_MATH_INV_SQRT:
      return "math_inversesqrt";
    case NODE_MATH_ABSOLUTE:
      return "math_absolute";
    case NODE_MATH_RADIANS:
      return "math_radians";
    case NODE_MATH_DEGREES:
      return "math_degrees";

    case NODE_MATH_MINIMUM:
      return "math_minimum";
    case NODE_MATH_MAXIMUM:
      return "math_maximum";
    case NODE_MATH_LESS_THAN:
      return "math_less_than";
    case NODE_MATH_GREATER_THAN:
      return "math_greater_than";
    case NODE_MATH_SIGN:
      return "math_sign";
    case NODE_MATH_COMPARE:
      return "math_compare";
    case NODE_MATH_SMOOTH_MIN:
      return "math_smoothmin";
    case NODE_MATH_SMOOTH_MAX:
      return "math_smoothmax";

    case NODE_MATH_ROUND:
      return "math_round";
    case NODE_MATH_FLOOR:
      return "math_floor";
    case NODE_MATH_CEIL:
      return "math_ceil";
    case NODE_MATH_FRACTION:
      return "math_fraction";
    case NODE_MATH_MODULO:
      return "math_modulo";
    case NODE_MATH_TRUNC:
      return "math_trunc";
    case NODE_MATH_SNAP:
      return "math_snap";
    case NODE_MATH_WRAP:
      return "math_wrap";
    case NODE_MATH_PINGPONG:
      return "math_pingpong";

    case NODE_MATH_SINE:
      return "math_sine";
    case NODE_MATH_COSINE:
      return "math_cosine";
    case NODE_MATH_TANGENT:
      return "math_tangent";
    case NODE_MATH_SINH:
      return "math_sinh";
    case NODE_MATH_COSH:
      return "math_cosh";
    case NODE_MATH_TANH:
      return "math_tanh";
    case NODE_MATH_ARCSINE:
      return "math_arcsine";
    case NODE_MATH_ARCCOSINE:
      return "math_arccosine";
    case NODE_MATH_ARCTANGENT:
      return "math_arctangent";
    case NODE_MATH_ARCTAN2:
      return "math_arctan2";
  }
  return nullptr;
}

static int gpu_shader_math(GPUMaterial *mat,
                           bNode *node,
                           bNodeExecData *UNUSED(execdata),
                           GPUNodeStack *in,
                           GPUNodeStack *out)
{
  const char *name = gpu_shader_get_name(node->custom1);
  if (name != nullptr) {
    int ret = GPU_stack_link(mat, node, name, in, out);

    if (ret && node->custom2 & SHD_MATH_CLAMP) {
      float min[3] = {0.0f, 0.0f, 0.0f};
      float max[3] = {1.0f, 1.0f, 1.0f};
      GPU_link(
          mat, "clamp_value", out[0].link, GPU_constant(min), GPU_constant(max), &out[0].link);
    }
    return ret;
  }
  else {
    return 0;
  }
}

static void sh_node_math_expand_in_mf_network(blender::bke::NodeMFNetworkBuilder &builder)
{
  /* TODO: Implement clamp and other operations. */
  const int mode = builder.bnode().custom1;
  switch (mode) {
    case NODE_MATH_ADD: {
      static blender::fn::CustomMF_SI_SI_SO<float, float, float> fn{
          "Add", [](float a, float b) { return a + b; }};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_SUBTRACT: {
      static blender::fn::CustomMF_SI_SI_SO<float, float, float> fn{
          "Subtract", [](float a, float b) { return a - b; }};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_MULTIPLY: {
      static blender::fn::CustomMF_SI_SI_SO<float, float, float> fn{
          "Multiply", [](float a, float b) { return a * b; }};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_DIVIDE: {
      static blender::fn::CustomMF_SI_SI_SO<float, float, float> fn{
          "Divide", [](float a, float b) { return (b != 0.0f) ? a / b : 0.0f; }};
      builder.set_matching_fn(fn);
      break;
    }
    default:
      BLI_assert(false);
      break;
  }
}

void register_node_type_sh_math(void)
{
  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_MATH, "Math", NODE_CLASS_CONVERTOR, 0);
  node_type_socket_templates(&ntype, sh_node_math_in, sh_node_math_out);
  node_type_label(&ntype, node_math_label);
  node_type_gpu(&ntype, gpu_shader_math);
  node_type_update(&ntype, node_math_update);
  ntype.expand_in_mf_network = sh_node_math_expand_in_mf_network;

  nodeRegisterType(&ntype);
}
