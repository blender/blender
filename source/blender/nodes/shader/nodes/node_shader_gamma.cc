/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_gamma_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Color")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .description("Color input on which correction will be applied");
  b.add_input<decl::Float>("Gamma")
      .default_value(1.0f)
      .min(0.001f)
      .max(10.0f)
      .subtype(PROP_UNSIGNED)
      .description(
          "Gamma correction value\n"
          "Gamma controls the relative intensity of the mid-tones compared to the full black and "
          "full white.");
  b.add_output<decl::Color>("Color");
}

static int node_shader_gpu_gamma(GPUMaterial *mat,
                                 bNode *node,
                                 bNodeExecData * /*execdata*/,
                                 GPUNodeStack *in,
                                 GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "node_gamma", in, out);
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  NodeItem color = get_input_value("Color", NodeItem::Type::Color4);
  NodeItem gamma = get_input_value("Gamma", NodeItem::Type::Float);
  return color ^ gamma;
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_gamma_cc

void register_node_type_sh_gamma()
{
  namespace file_ns = blender::nodes::node_shader_gamma_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_GAMMA, "Gamma", NODE_CLASS_OP_COLOR);
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::node_shader_gpu_gamma;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  nodeRegisterType(&ntype);
}
