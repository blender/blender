/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_brightness_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Color")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .description("Color input on which correction will be applied");
  b.add_input<decl::Float>("Brightness", "Bright")
      .default_value(0.0f)
      .min(-100.0f)
      .max(100.0f)
      .description(
          "Brightness correction value.\n"
          "An additive-type factor by which to increase the overall brightness of the image. "
          "Use a negative number to darken an image, and a positive number to brighten it");
  b.add_input<decl::Float>("Contrast")
      .default_value(0.0f)
      .min(-100.0f)
      .max(100.0f)
      .description(
          "Contrast correction value.\n"
          "A scaling type factor by which to make brighter pixels brighter, but keeping the "
          "darker pixels dark. "
          "Use a negative number to decrease contrast, and a positive number to increase it");
  b.add_output<decl::Color>("Color");
}

static int gpu_shader_brightcontrast(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData * /*execdata*/,
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "brightness_contrast", in, out);
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  NodeItem color = get_input_value("Color", NodeItem::Type::Color3);
  NodeItem bright = get_input_value("Bright", NodeItem::Type::Float);
  NodeItem contrast = get_input_value("Contrast", NodeItem::Type::Float);

  /* This formula was given from OSL shader code in Cycles. */
  return (bright + color * (contrast + val(1.0f)) - contrast * val(0.5f)).max(val(0.0f));
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_brightness_cc

void register_node_type_sh_brightcontrast()
{
  namespace file_ns = blender::nodes::node_shader_brightness_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeBrightContrast", SH_NODE_BRIGHTCONTRAST);
  ntype.ui_name = "Brightness/Contrast";
  ntype.ui_description = "Control the brightness and contrast of the input color";
  ntype.enum_name_legacy = "BRIGHTCONTRAST";
  ntype.nclass = NODE_CLASS_OP_COLOR;
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::gpu_shader_brightcontrast;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::node_register_type(ntype);
}
