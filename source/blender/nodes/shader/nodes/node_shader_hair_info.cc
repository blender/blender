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

namespace blender::nodes::node_shader_hair_info_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>(N_("Is Strand"));
  b.add_output<decl::Float>(N_("Intercept"));
  b.add_output<decl::Float>(N_("Length"));
  b.add_output<decl::Float>(N_("Thickness"));
  b.add_output<decl::Vector>(N_("Tangent Normal"));
  // b.add_output<decl::Float>(N_("Fade"));
  b.add_output<decl::Float>(N_("Random"));
}

static int node_shader_gpu_hair_info(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData *UNUSED(execdata),
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  /* Length: don't request length if not needed. */
  static const float zero = 0;
  GPUNodeLink *length_link = (!out[2].hasoutput) ? GPU_constant(&zero) :
                                                   GPU_attribute(mat, CD_HAIRLENGTH, "");
  return GPU_stack_link(mat, node, "node_hair_info", in, out, length_link);
}

}  // namespace blender::nodes::node_shader_hair_info_cc

/* node type definition */
void register_node_type_sh_hair_info()
{
  namespace file_ns = blender::nodes::node_shader_hair_info_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_HAIR_INFO, "Hair Info", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  node_type_gpu(&ntype, file_ns::node_shader_gpu_hair_info);

  nodeRegisterType(&ntype);
}
