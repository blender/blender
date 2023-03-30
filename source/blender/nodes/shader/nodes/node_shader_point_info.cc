/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_point_info_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Vector>(N_("Position"));
  b.add_output<decl::Float>(N_("Radius"));
  b.add_output<decl::Float>(N_("Random"));
}

static int node_shader_gpu_point_info(GPUMaterial *mat,
                                      bNode *node,
                                      bNodeExecData * /*execdata*/,
                                      GPUNodeStack *in,
                                      GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "node_point_info", in, out);
}

}  // namespace blender::nodes::node_shader_point_info_cc

/* node type definition */
void register_node_type_sh_point_info()
{
  namespace file_ns = blender::nodes::node_shader_point_info_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_POINT_INFO, "Point Info", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::node_shader_gpu_point_info;

  nodeRegisterType(&ntype);
}
