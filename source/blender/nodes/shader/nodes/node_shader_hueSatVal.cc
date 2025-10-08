/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_hueSatVal_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Hue").default_value(0.5f).min(0.0f).max(1.0f).description(
      "Hue rotation offset, from 0 (-180°) to 1 (+180°). Note that 0 and 1 have the same result");
  b.add_input<decl::Float>("Saturation")
      .default_value(1.0f)
      .min(0.0f)
      .max(2.0f)
      .description(
          "Value of 0 removes color from the image, making it black-and-white. "
          "A value greater than 1.0 increases saturation");
  b.add_input<decl::Float>("Value")
      .default_value(1.0f)
      .min(0.0f)
      .max(2.0f)
      .translation_context(BLT_I18NCONTEXT_COLOR)
      .description(
          "Value shift. 0 makes the color black, 1 keeps it the same, and higher values make it "
          "brighter");
  b.add_input<decl::Float>("Factor", "Fac")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description("Amount of influence the node exerts on the image");
  b.add_input<decl::Color>("Color")
      .default_value({0.8f, 0.8f, 0.8f, 1.0f})
      .description("Color input on which HSV color transformation will be applied");
  b.add_output<decl::Color>("Color");
}

static int gpu_shader_hue_sat(GPUMaterial *mat,
                              bNode *node,
                              bNodeExecData * /*execdata*/,
                              GPUNodeStack *in,
                              GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "hue_sat", in, out);
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  NodeItem hue = get_input_value("Hue", NodeItem::Type::Float);
  NodeItem saturation = get_input_value("Saturation", NodeItem::Type::Float);
  NodeItem value = get_input_value("Value", NodeItem::Type::Float);
  NodeItem fac = get_input_value("Fac", NodeItem::Type::Float);
  NodeItem color = get_input_value("Color", NodeItem::Type::Color3);

  /* Modifier to follow Cycles result */
  hue = hue - val(0.5f);

  NodeItem combine = create_node(
      "combine3", NodeItem::Type::Vector3, {{"in1", hue}, {"in2", saturation}, {"in3", value}});

  NodeItem hsv = create_node(
      "hsvadjust", NodeItem::Type::Color3, {{"in", color}, {"amount", combine}});

  return fac.mix(color, hsv);
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_hueSatVal_cc

void register_node_type_sh_hue_sat()
{
  namespace file_ns = blender::nodes::node_shader_hueSatVal_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeHueSaturation", SH_NODE_HUE_SAT);
  ntype.ui_name = "Hue/Saturation/Value";
  ntype.ui_description = "Apply a color transformation in the HSV color model";
  ntype.enum_name_legacy = "HUE_SAT";
  ntype.nclass = NODE_CLASS_OP_COLOR;
  ntype.declare = file_ns::node_declare;
  blender::bke::node_type_size_preset(ntype, blender::bke::eNodeSizePreset::Middle);
  ntype.gpu_fn = file_ns::gpu_shader_hue_sat;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::node_register_type(ntype);
}
