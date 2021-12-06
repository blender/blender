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
 * The Original Code is Copyright (C) 2021 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_separate_hsv_cc {

static bNodeSocketTemplate sh_node_sephsv_in[] = {
    {SOCK_RGBA, N_("Color"), 0.8f, 0.8f, 0.8f, 1.0f},
    {-1, ""},
};
static bNodeSocketTemplate sh_node_sephsv_out[] = {
    {SOCK_FLOAT, N_("H")},
    {SOCK_FLOAT, N_("S")},
    {SOCK_FLOAT, N_("V")},
    {-1, ""},
};

static void node_shader_exec_sephsv(void *UNUSED(data),
                                    int UNUSED(thread),
                                    bNode *UNUSED(node),
                                    bNodeExecData *UNUSED(execdata),
                                    bNodeStack **in,
                                    bNodeStack **out)
{
  float col[3];
  nodestack_get_vec(col, SOCK_VECTOR, in[0]);

  rgb_to_hsv(col[0], col[1], col[2], &out[0]->vec[0], &out[1]->vec[0], &out[2]->vec[0]);
}

static int gpu_shader_sephsv(GPUMaterial *mat,
                             bNode *node,
                             bNodeExecData *UNUSED(execdata),
                             GPUNodeStack *in,
                             GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "separate_hsv", in, out);
}

}  // namespace blender::nodes::node_shader_separate_hsv_cc

void register_node_type_sh_sephsv()
{
  namespace file_ns = blender::nodes::node_shader_separate_hsv_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_SEPHSV, "Separate HSV", NODE_CLASS_CONVERTER, 0);
  node_type_socket_templates(&ntype, file_ns::sh_node_sephsv_in, file_ns::sh_node_sephsv_out);
  node_type_exec(&ntype, nullptr, nullptr, file_ns::node_shader_exec_sephsv);
  node_type_gpu(&ntype, file_ns::gpu_shader_sephsv);

  nodeRegisterType(&ntype);
}
