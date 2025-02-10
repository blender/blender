/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "GPU_material.hh"

#include "IMB_colormanagement.hh"

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_rgb_to_bw_cc {

static void sh_node_rgbtobw_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Color").default_value({0.5f, 0.5f, 0.5f, 1.0f});
  b.add_output<decl::Float>("Val");
}

static int gpu_shader_rgbtobw(GPUMaterial *mat,
                              bNode *node,
                              bNodeExecData * /*execdata*/,
                              GPUNodeStack *in,
                              GPUNodeStack *out)
{
  float coefficients[3];
  IMB_colormanagement_get_luminance_coefficients(coefficients);
  return GPU_stack_link(mat, node, "rgbtobw", in, out, GPU_constant(coefficients));
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  NodeItem color = get_input_value("Color", NodeItem::Type::Color3);
  return create_node("luminance", NodeItem::Type::Color3, {{"in", color}});
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_rgb_to_bw_cc

void register_node_type_sh_rgbtobw()
{
  namespace file_ns = blender::nodes::node_shader_rgb_to_bw_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeRGBToBW", SH_NODE_RGBTOBW);
  ntype.ui_name = "RGB to BW";
  ntype.ui_description = "Convert a color's luminance to a grayscale value";
  ntype.enum_name_legacy = "RGBTOBW";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = file_ns::sh_node_rgbtobw_declare;
  ntype.gpu_fn = file_ns::gpu_shader_rgbtobw;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::node_register_type(&ntype);
}
