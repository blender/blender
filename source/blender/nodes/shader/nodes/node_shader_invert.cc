/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_invert_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Fac").default_value(1.0f).min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_input<decl::Color>("Color").default_value({0.0f, 0.0f, 0.0f, 1.0f});
  b.add_output<decl::Color>("Color");
}

static int gpu_shader_invert(GPUMaterial *mat,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *in,
                             GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "invert", in, out);
}

}  // namespace blender::nodes::node_shader_invert_cc

void register_node_type_sh_invert()
{
  namespace file_ns = blender::nodes::node_shader_invert_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_INVERT, "Invert Color", NODE_CLASS_OP_COLOR);
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::gpu_shader_invert;

  nodeRegisterType(&ntype);
}
