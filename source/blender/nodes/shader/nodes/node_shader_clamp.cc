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

/* **************** Clamp ******************** */
static bNodeSocketTemplate sh_node_clamp_in[] = {
    {SOCK_FLOAT, N_("Value"), 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, PROP_NONE},
    {SOCK_FLOAT, N_("Min"), 0.0f, 1.0f, 1.0f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
    {SOCK_FLOAT, N_("Max"), 1.0f, 1.0f, 1.0f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
    {-1, ""},
};
static bNodeSocketTemplate sh_node_clamp_out[] = {
    {SOCK_FLOAT, N_("Result")},
    {-1, ""},
};

static void node_shader_init_clamp(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->custom1 = NODE_CLAMP_MINMAX; /* clamp type */
}

static int gpu_shader_clamp(GPUMaterial *mat,
                            bNode *node,
                            bNodeExecData *UNUSED(execdata),
                            GPUNodeStack *in,
                            GPUNodeStack *out)
{
  return (node->custom1 == NODE_CLAMP_MINMAX) ?
             GPU_stack_link(mat, node, "clamp_minmax", in, out) :
             GPU_stack_link(mat, node, "clamp_range", in, out);
}

static void sh_node_clamp_expand_in_mf_network(blender::nodes::NodeMFNetworkBuilder &builder)
{
  static blender::fn::CustomMF_SI_SI_SI_SO<float, float, float, float> minmax_fn{
      "Clamp (Min Max)",
      [](float value, float min, float max) { return std::min(std::max(value, min), max); }};
  static blender::fn::CustomMF_SI_SI_SI_SO<float, float, float, float> range_fn{
      "Clamp (Range)", [](float value, float a, float b) {
        if (a < b) {
          return clamp_f(value, a, b);
        }

        return clamp_f(value, b, a);
      }};

  int clamp_type = builder.bnode().custom1;
  if (clamp_type == NODE_CLAMP_MINMAX) {
    builder.set_matching_fn(minmax_fn);
  }
  else {
    builder.set_matching_fn(range_fn);
  }
}

void register_node_type_sh_clamp(void)
{
  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_CLAMP, "Clamp", NODE_CLASS_CONVERTOR, 0);
  node_type_socket_templates(&ntype, sh_node_clamp_in, sh_node_clamp_out);
  node_type_init(&ntype, node_shader_init_clamp);
  node_type_gpu(&ntype, gpu_shader_clamp);
  ntype.expand_in_mf_network = sh_node_clamp_expand_in_mf_network;

  nodeRegisterType(&ntype);
}
