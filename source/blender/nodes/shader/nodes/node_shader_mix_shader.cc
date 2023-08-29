/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_mix_shader_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Fac").default_value(0.5f).min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_input<decl::Shader>("Shader");
  b.add_input<decl::Shader>("Shader", "Shader_001");
  b.add_output<decl::Shader>("Shader");
}

static int node_shader_gpu_mix_shader(GPUMaterial *mat,
                                      bNode *node,
                                      bNodeExecData * /*execdata*/,
                                      GPUNodeStack *in,
                                      GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "node_mix_shader", in, out);
}

}  // namespace blender::nodes::node_shader_mix_shader_cc

/* node type definition */
void register_node_type_sh_mix_shader()
{
  namespace file_ns = blender::nodes::node_shader_mix_shader_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_MIX_SHADER, "Mix Shader", NODE_CLASS_SHADER);
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::node_shader_gpu_mix_shader;

  nodeRegisterType(&ntype);
}
