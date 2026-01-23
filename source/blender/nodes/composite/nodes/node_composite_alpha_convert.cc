/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "GPU_material.hh"

#include "COM_result.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_alpha_convert_cc {

static const EnumPropertyItem type_items[] = {
    {CMP_NODE_ALPHA_CONVERT_PREMULTIPLY,
     "STRAIGHT_TO_PREMULTIPLIED",
     0,
     N_("To Premultiplied"),
     N_("Convert straight to premultiplied")},
    {CMP_NODE_ALPHA_CONVERT_UNPREMULTIPLY,
     "PREMULTIPLIED_TO_STRAIGHT",
     0,
     N_("To Straight"),
     N_("Convert premultiplied to straight")},
    {0, nullptr, 0, nullptr, nullptr},
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.is_function_node();
  b.add_input<decl::Color>("Image").default_value({1.0f, 1.0f, 1.0f, 1.0f}).hide_value();
  b.add_output<decl::Color>("Image").align_with_previous();

  b.add_input<decl::Menu>("Type")
      .default_value(CMP_NODE_ALPHA_CONVERT_PREMULTIPLY)
      .static_items(type_items)
      .optional_label();
}

using namespace blender::compositor;

static int node_gpu_material(GPUMaterial *material,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *inputs,
                             GPUNodeStack *outputs)
{
  return GPU_stack_link(material, node, "node_composite_convert_alpha", inputs, outputs);
}

static float4 convert_alpha(const float4 &color, const MenuValue &type)
{
  switch (CMPNodeAlphaConvertMode(type.value)) {
    case CMP_NODE_ALPHA_CONVERT_PREMULTIPLY:
      return float4(color.xyz() * color.w, color.w);
    case CMP_NODE_ALPHA_CONVERT_UNPREMULTIPLY:
      return color.w == 0.0f ? color : float4(color.xyz() / color.w, color.w);
  }
  return color;
}

using compositor::Color;

static void node_build_multi_function(nodes::NodeMultiFunctionBuilder &builder)
{
  static auto function = mf::build::SI2_SO<Color, MenuValue, Color>(
      "Alpha Convert",
      [](const Color &color, const MenuValue &type) -> Color {
        return Color(convert_alpha(float4(color), type));
      },
      mf::build::exec_presets::AllSpanOrSingle());
  builder.set_matching_fn(function);
}

static void node_register()
{
  static bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodePremulKey", CMP_NODE_PREMULKEY);
  ntype.ui_name = "Alpha Convert";
  ntype.ui_description = "Convert to and from premultiplied (associated) alpha";
  ntype.enum_name_legacy = "PREMULKEY";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.gpu_fn = node_gpu_material;
  ntype.build_multi_function = node_build_multi_function;

  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_composite_alpha_convert_cc
