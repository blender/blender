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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_combine_xyz_cc {

static void sh_node_combxyz_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Float>(N_("X")).min(-10000.0f).max(10000.0f);
  b.add_input<decl::Float>(N_("Y")).min(-10000.0f).max(10000.0f);
  b.add_input<decl::Float>(N_("Z")).min(-10000.0f).max(10000.0f);
  b.add_output<decl::Vector>(N_("Vector"));
};

static int gpu_shader_combxyz(GPUMaterial *mat,
                              bNode *node,
                              bNodeExecData *UNUSED(execdata),
                              GPUNodeStack *in,
                              GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "combine_xyz", in, out);
}

static void sh_node_combxyz_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static blender::fn::CustomMF_SI_SI_SI_SO<float, float, float, blender::float3> fn{
      "Combine Vector", [](float x, float y, float z) { return blender::float3(x, y, z); }};
  builder.set_matching_fn(fn);
}

}  // namespace blender::nodes::node_shader_combine_xyz_cc

void register_node_type_sh_combxyz()
{
  namespace file_ns = blender::nodes::node_shader_combine_xyz_cc;

  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_COMBXYZ, "Combine XYZ", NODE_CLASS_CONVERTER, 0);
  ntype.declare = file_ns::sh_node_combxyz_declare;
  node_type_gpu(&ntype, file_ns::gpu_shader_combxyz);
  ntype.build_multi_function = file_ns::sh_node_combxyz_build_multi_function;

  nodeRegisterType(&ntype);
}
