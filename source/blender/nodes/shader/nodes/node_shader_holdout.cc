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

#include "node_shader_util.hh"

/* **************** OUTPUT ******************** */

namespace blender::nodes::node_shader_holdout_cc {

static bNodeSocketTemplate sh_node_holdout_in[] = {
    {-1, ""},
};

static bNodeSocketTemplate sh_node_holdout_out[] = {
    {SOCK_SHADER, N_("Holdout")},
    {-1, ""},
};

static int gpu_shader_rgb(GPUMaterial *mat,
                          bNode *node,
                          bNodeExecData *UNUSED(execdata),
                          GPUNodeStack *in,
                          GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "node_holdout", in, out);
}

}  // namespace blender::nodes::node_shader_holdout_cc

/* node type definition */
void register_node_type_sh_holdout()
{
  namespace file_ns = blender::nodes::node_shader_holdout_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_HOLDOUT, "Holdout", NODE_CLASS_SHADER, 0);
  node_type_socket_templates(&ntype, file_ns::sh_node_holdout_in, file_ns::sh_node_holdout_out);
  node_type_init(&ntype, nullptr);
  node_type_storage(&ntype, "", nullptr, nullptr);
  node_type_gpu(&ntype, file_ns::gpu_shader_rgb);

  nodeRegisterType(&ntype);
}
