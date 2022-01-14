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

namespace blender::nodes::node_shader_volume_info_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Color>(N_("Color"));
  b.add_output<decl::Float>(N_("Density"));
  b.add_output<decl::Float>(N_("Flame"));
  b.add_output<decl::Float>(N_("Temperature"));
}

static int node_shader_gpu_volume_info(GPUMaterial *mat,
                                       bNode *UNUSED(node),
                                       bNodeExecData *UNUSED(execdata),
                                       GPUNodeStack *UNUSED(in),
                                       GPUNodeStack *out)
{
  if (out[0].hasoutput) {
    out[0].link = GPU_volume_grid(mat, "color", GPU_VOLUME_DEFAULT_0);
  }
  if (out[1].hasoutput) {
    out[1].link = GPU_volume_grid(mat, "density", GPU_VOLUME_DEFAULT_0);
  }
  if (out[2].hasoutput) {
    out[2].link = GPU_volume_grid(mat, "flame", GPU_VOLUME_DEFAULT_0);
  }
  if (out[3].hasoutput) {
    out[3].link = GPU_volume_grid(mat, "temperature", GPU_VOLUME_DEFAULT_0);
  }

  return true;
}

}  // namespace blender::nodes::node_shader_volume_info_cc

void register_node_type_sh_volume_info()
{
  namespace file_ns = blender::nodes::node_shader_volume_info_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_VOLUME_INFO, "Volume Info", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  node_type_gpu(&ntype, file_ns::node_shader_gpu_volume_info);

  nodeRegisterType(&ntype);
}
