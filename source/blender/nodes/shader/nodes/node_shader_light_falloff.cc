/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_light_falloff_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Strength").default_value(100.0f).min(0.0f).max(1000000.0f);
  b.add_input<decl::Float>("Smooth").default_value(0.0f).min(0.0f).max(1000.0f);
  b.add_output<decl::Float>("Quadratic");
  b.add_output<decl::Float>("Linear");
  b.add_output<decl::Float>("Constant");
}

static int node_shader_gpu_light_falloff(GPUMaterial *mat,
                                         bNode *node,
                                         bNodeExecData * /*execdata*/,
                                         GPUNodeStack *in,
                                         GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "node_light_falloff", in, out);
}

}  // namespace blender::nodes::node_shader_light_falloff_cc

/* node type definition */
void register_node_type_sh_light_falloff()
{
  namespace file_ns = blender::nodes::node_shader_light_falloff_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_LIGHT_FALLOFF, "Light Falloff", NODE_CLASS_OP_COLOR);
  ntype.declare = file_ns::node_declare;
  blender::bke::node_type_size_preset(&ntype, blender::bke::eNodeSizePreset::MIDDLE);
  ntype.gpu_fn = file_ns::node_shader_gpu_light_falloff;

  nodeRegisterType(&ntype);
}
