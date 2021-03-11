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

/* **************** VALUE ******************** */
static bNodeSocketTemplate sh_node_value_out[] = {
    {SOCK_FLOAT, N_("Value"), 0.5f, 0, 0, 0, -FLT_MAX, FLT_MAX, PROP_NONE},
    {-1, ""},
};

static int gpu_shader_value(GPUMaterial *mat,
                            bNode *node,
                            bNodeExecData *UNUSED(execdata),
                            GPUNodeStack *in,
                            GPUNodeStack *out)
{
  GPUNodeLink *link = GPU_uniformbuf_link_out(mat, node, out, 0);
  return GPU_stack_link(mat, node, "set_value", in, out, link);
}

static void sh_node_value_expand_in_mf_network(blender::nodes::NodeMFNetworkBuilder &builder)
{
  const bNodeSocket *bsocket = builder.dnode()->output(0).bsocket();
  const bNodeSocketValueFloat *value = (const bNodeSocketValueFloat *)bsocket->default_value;
  builder.construct_and_set_matching_fn<blender::fn::CustomMF_Constant<float>>(value->value);
}

void register_node_type_sh_value(void)
{
  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_VALUE, "Value", NODE_CLASS_INPUT, 0);
  node_type_socket_templates(&ntype, nullptr, sh_node_value_out);
  node_type_gpu(&ntype, gpu_shader_value);
  ntype.expand_in_mf_network = sh_node_value_expand_in_mf_network;

  nodeRegisterType(&ntype);
}
