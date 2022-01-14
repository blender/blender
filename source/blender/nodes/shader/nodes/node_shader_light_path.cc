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

namespace blender::nodes::node_shader_light_path_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>(N_("Is Camera Ray"));
  b.add_output<decl::Float>(N_("Is Shadow Ray"));
  b.add_output<decl::Float>(N_("Is Diffuse Ray"));
  b.add_output<decl::Float>(N_("Is Glossy Ray"));
  b.add_output<decl::Float>(N_("Is Singular Ray"));
  b.add_output<decl::Float>(N_("Is Reflection Ray"));
  b.add_output<decl::Float>(N_("Is Transmission Ray"));
  b.add_output<decl::Float>(N_("Ray Length"));
  b.add_output<decl::Float>(N_("Ray Depth"));
  b.add_output<decl::Float>(N_("Diffuse Depth"));
  b.add_output<decl::Float>(N_("Glossy Depth"));
  b.add_output<decl::Float>(N_("Transparent Depth"));
  b.add_output<decl::Float>(N_("Transmission Depth"));
}

static int node_shader_gpu_light_path(GPUMaterial *mat,
                                      bNode *node,
                                      bNodeExecData *UNUSED(execdata),
                                      GPUNodeStack *in,
                                      GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "node_light_path", in, out);
}

}  // namespace blender::nodes::node_shader_light_path_cc

/* node type definition */
void register_node_type_sh_light_path()
{
  namespace file_ns = blender::nodes::node_shader_light_path_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_LIGHT_PATH, "Light Path", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  node_type_gpu(&ntype, file_ns::node_shader_gpu_light_path);

  nodeRegisterType(&ntype);
}
