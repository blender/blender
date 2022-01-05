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

namespace blender::nodes::node_shader_object_info_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Vector>(N_("Location"));
  b.add_output<decl::Color>(N_("Color"));
  b.add_output<decl::Float>(N_("Object Index"));
  b.add_output<decl::Float>(N_("Material Index"));
  b.add_output<decl::Float>(N_("Random"));
}

static int node_shader_gpu_object_info(GPUMaterial *mat,
                                       bNode *node,
                                       bNodeExecData *UNUSED(execdata),
                                       GPUNodeStack *in,
                                       GPUNodeStack *out)
{
  Material *ma = GPU_material_get_material(mat);
  float index = ma ? ma->index : 0.0f;
  return GPU_stack_link(mat,
                        node,
                        "node_object_info",
                        in,
                        out,
                        GPU_builtin(GPU_OBJECT_MATRIX),
                        GPU_builtin(GPU_OBJECT_COLOR),
                        GPU_builtin(GPU_OBJECT_INFO),
                        GPU_constant(&index));
}

}  // namespace blender::nodes::node_shader_object_info_cc

void register_node_type_sh_object_info()
{
  namespace file_ns = blender::nodes::node_shader_object_info_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_OBJECT_INFO, "Object Info", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  node_type_gpu(&ntype, file_ns::node_shader_gpu_object_info);

  nodeRegisterType(&ntype);
}
