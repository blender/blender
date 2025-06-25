/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "GPU_material.hh"

#include "node_composite_util.hh"

/* **************** Pre-multiply and Key Alpha Convert ******************** */

namespace blender::nodes::node_composite_premulkey_cc {

static void cmp_node_premulkey_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_output<decl::Color>("Image");
}

static void node_composit_buts_premulkey(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "mapping", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

using namespace blender::compositor;

static CMPNodeAlphaConvertMode get_mode(const bNode &node)
{
  return static_cast<CMPNodeAlphaConvertMode>(node.custom1);
}

static int node_gpu_material(GPUMaterial *material,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *inputs,
                             GPUNodeStack *outputs)
{
  switch (get_mode(*node)) {
    case CMP_NODE_ALPHA_CONVERT_PREMULTIPLY:
      return GPU_stack_link(material, node, "color_alpha_premultiply", inputs, outputs);
    case CMP_NODE_ALPHA_CONVERT_UNPREMULTIPLY:
      return GPU_stack_link(material, node, "color_alpha_unpremultiply", inputs, outputs);
  }

  return false;
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static auto premultiply_function = mf::build::SI1_SO<float4, float4>(
      "Alpha Convert Premultiply",
      [](const float4 &color) -> float4 { return float4(color.xyz() * color.w, color.w); },
      mf::build::exec_presets::AllSpanOrSingle());

  static auto unpremultiply_function = mf::build::SI1_SO<float4, float4>(
      "Alpha Convert Unpremultiply",
      [](const float4 &color) -> float4 {
        if (ELEM(color.w, 0.0f, 1.0f)) {
          return color;
        }
        return float4(color.xyz() / color.w, color.w);
      },
      mf::build::exec_presets::AllSpanOrSingle());

  switch (get_mode(builder.node())) {
    case CMP_NODE_ALPHA_CONVERT_PREMULTIPLY:
      builder.set_matching_fn(premultiply_function);
      break;
    case CMP_NODE_ALPHA_CONVERT_UNPREMULTIPLY:
      builder.set_matching_fn(unpremultiply_function);
      break;
  }
}

}  // namespace blender::nodes::node_composite_premulkey_cc

static void register_node_type_cmp_premulkey()
{
  namespace file_ns = blender::nodes::node_composite_premulkey_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodePremulKey", CMP_NODE_PREMULKEY);
  ntype.ui_name = "Alpha Convert";
  ntype.ui_description = "Convert to and from premultiplied (associated) alpha";
  ntype.enum_name_legacy = "PREMULKEY";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = file_ns::cmp_node_premulkey_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_premulkey;
  ntype.gpu_fn = file_ns::node_gpu_material;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_premulkey)
