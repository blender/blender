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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_gamma_cc {

/* **************** Gamma Tools  ******************** */

static bNodeSocketTemplate sh_node_gamma_in[] = {
    {SOCK_RGBA, N_("Color"), 1.0f, 1.0f, 1.0f, 1.0f},
    {SOCK_FLOAT, N_("Gamma"), 1.0f, 0.0f, 0.0f, 0.0f, 0.001f, 10.0f, PROP_UNSIGNED},
    {-1, ""},
};

static bNodeSocketTemplate sh_node_gamma_out[] = {
    {SOCK_RGBA, N_("Color")},
    {-1, ""},
};

static int node_shader_gpu_gamma(GPUMaterial *mat,
                                 bNode *node,
                                 bNodeExecData *UNUSED(execdata),
                                 GPUNodeStack *in,
                                 GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "node_gamma", in, out);
}

}  // namespace blender::nodes::node_shader_gamma_cc

void register_node_type_sh_gamma()
{
  namespace file_ns = blender::nodes::node_shader_gamma_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_GAMMA, "Gamma", NODE_CLASS_OP_COLOR);
  node_type_socket_templates(&ntype, file_ns::sh_node_gamma_in, file_ns::sh_node_gamma_out);
  node_type_gpu(&ntype, file_ns::node_shader_gpu_gamma);

  nodeRegisterType(&ntype);
}
