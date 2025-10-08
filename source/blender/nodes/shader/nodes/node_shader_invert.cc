/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_invert_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Factor", "Fac")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description("Amount of influence the node exerts on the image");
  b.add_input<decl::Color>("Color")
      .default_value({0.0f, 0.0f, 0.0f, 1.0f})
      .description("Color input on which inversion will be applied");
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

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  NodeItem fac = get_input_value("Fac", NodeItem::Type::Float);
  NodeItem color = get_input_value("Color", NodeItem::Type::Color3);
  return fac.mix(color, val(1.0f) - color);
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_invert_cc

void register_node_type_sh_invert()
{
  namespace file_ns = blender::nodes::node_shader_invert_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeInvert", SH_NODE_INVERT);
  ntype.ui_name = "Invert Color";
  ntype.ui_description = "Invert a color, producing a negative";
  ntype.enum_name_legacy = "INVERT";
  ntype.nclass = NODE_CLASS_OP_COLOR;
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::gpu_shader_invert;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::node_register_type(ntype);
}
