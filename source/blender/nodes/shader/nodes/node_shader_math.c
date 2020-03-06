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

static int gpu_shader_math(GPUMaterial *mat,
                           bNode *node,
                           bNodeExecData *UNUSED(execdata),
                           GPUNodeStack *in,
                           GPUNodeStack *out)
{
  static const char *names[] = {
      [NODE_MATH_ADD] = "math_add",
      [NODE_MATH_SUBTRACT] = "math_subtract",
      [NODE_MATH_MULTIPLY] = "math_multiply",
      [NODE_MATH_DIVIDE] = "math_divide",
      [NODE_MATH_MULTIPLY_ADD] = "math_multiply_add",

      [NODE_MATH_POWER] = "math_power",
      [NODE_MATH_LOGARITHM] = "math_logarithm",
      [NODE_MATH_EXPONENT] = "math_exponent",
      [NODE_MATH_SQRT] = "math_sqrt",
      [NODE_MATH_INV_SQRT] = "math_inversesqrt",
      [NODE_MATH_ABSOLUTE] = "math_absolute",
      [NODE_MATH_RADIANS] = "math_radians",
      [NODE_MATH_DEGREES] = "math_degrees",

      [NODE_MATH_MINIMUM] = "math_minimum",
      [NODE_MATH_MAXIMUM] = "math_maximum",
      [NODE_MATH_LESS_THAN] = "math_less_than",
      [NODE_MATH_GREATER_THAN] = "math_greater_than",
      [NODE_MATH_SIGN] = "math_sign",
      [NODE_MATH_COMPARE] = "math_compare",
      [NODE_MATH_SMOOTH_MIN] = "math_smoothmin",
      [NODE_MATH_SMOOTH_MAX] = "math_smoothmax",

      [NODE_MATH_ROUND] = "math_round",
      [NODE_MATH_FLOOR] = "math_floor",
      [NODE_MATH_CEIL] = "math_ceil",
      [NODE_MATH_FRACTION] = "math_fraction",
      [NODE_MATH_MODULO] = "math_modulo",
      [NODE_MATH_TRUNC] = "math_trunc",
      [NODE_MATH_SNAP] = "math_snap",
      [NODE_MATH_WRAP] = "math_wrap",
      [NODE_MATH_PINGPONG] = "math_pingpong",

      [NODE_MATH_SINE] = "math_sine",
      [NODE_MATH_COSINE] = "math_cosine",
      [NODE_MATH_TANGENT] = "math_tangent",
      [NODE_MATH_SINH] = "math_sinh",
      [NODE_MATH_COSH] = "math_cosh",
      [NODE_MATH_TANH] = "math_tanh",
      [NODE_MATH_ARCSINE] = "math_arcsine",
      [NODE_MATH_ARCCOSINE] = "math_arccosine",
      [NODE_MATH_ARCTANGENT] = "math_arctangent",
      [NODE_MATH_ARCTAN2] = "math_arctan2",
  };

  if (node->custom1 < ARRAY_SIZE(names) && names[node->custom1]) {
    int ret = GPU_stack_link(mat, node, names[node->custom1], in, out);

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

void register_node_type_sh_math(void)
{
  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_MATH, "Math", NODE_CLASS_CONVERTOR, 0);
  node_type_socket_templates(&ntype, sh_node_math_in, sh_node_math_out);
  node_type_label(&ntype, node_math_label);
  node_type_gpu(&ntype, gpu_shader_math);
  node_type_update(&ntype, node_math_update);

  nodeRegisterType(&ntype);
}
