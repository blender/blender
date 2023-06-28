/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_hair_info_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>("Is Strand");
  b.add_output<decl::Float>("Intercept");
  b.add_output<decl::Float>("Length");
  b.add_output<decl::Float>("Thickness");
  b.add_output<decl::Vector>("Tangent Normal");
  b.add_output<decl::Float>("Random");
}

static int node_shader_gpu_hair_info(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData * /*execdata*/,
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  /* Length: don't request length if not needed. */
  static const float zero = 0;
  GPUNodeLink *length_link = out[2].hasoutput ? GPU_attribute_hair_length(mat) :
                                                GPU_constant(&zero);
  return GPU_stack_link(mat, node, "node_hair_info", in, out, length_link);
}

}  // namespace blender::nodes::node_shader_hair_info_cc

/* node type definition */
void register_node_type_sh_hair_info()
{
  namespace file_ns = blender::nodes::node_shader_hair_info_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_HAIR_INFO, "Curves Info", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::node_shader_gpu_hair_info;

  nodeRegisterType(&ntype);
}
