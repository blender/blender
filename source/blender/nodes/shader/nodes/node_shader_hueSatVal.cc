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

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_hueSatVal_cc {

/* **************** Hue Saturation ******************** */
static bNodeSocketTemplate sh_node_hue_sat_in[] = {
    {SOCK_FLOAT, N_("Hue"), 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE},
    {SOCK_FLOAT, N_("Saturation"), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 2.0f, PROP_NONE},
    {SOCK_FLOAT, N_("Value"), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 2.0f, PROP_NONE},
    {SOCK_FLOAT, N_("Fac"), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
    {SOCK_RGBA, N_("Color"), 0.8f, 0.8f, 0.8f, 1.0f},
    {-1, ""},
};
static bNodeSocketTemplate sh_node_hue_sat_out[] = {
    {SOCK_RGBA, N_("Color")},
    {-1, ""},
};

static int gpu_shader_hue_sat(GPUMaterial *mat,
                              bNode *node,
                              bNodeExecData *UNUSED(execdata),
                              GPUNodeStack *in,
                              GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "hue_sat", in, out);
}

}  // namespace blender::nodes::node_shader_hueSatVal_cc

void register_node_type_sh_hue_sat()
{
  namespace file_ns = blender::nodes::node_shader_hueSatVal_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_HUE_SAT, "Hue Saturation Value", NODE_CLASS_OP_COLOR);
  node_type_socket_templates(&ntype, file_ns::sh_node_hue_sat_in, file_ns::sh_node_hue_sat_out);
  node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
  node_type_gpu(&ntype, file_ns::gpu_shader_hue_sat);

  nodeRegisterType(&ntype);
}
