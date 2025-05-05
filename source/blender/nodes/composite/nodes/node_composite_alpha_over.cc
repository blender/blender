/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_material.hh"

#include "node_composite_util.hh"

/* **************** ALPHAOVER ******************** */

namespace blender::nodes::node_composite_alpha_over_cc {

static void cmp_node_alphaover_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Fac")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .compositor_domain_priority(2);
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Color>("Image", "Image_001")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(1);
  b.add_output<decl::Color>("Image");
}

static void node_alphaover_init(bNodeTree * /*ntree*/, bNode *node)
{
  /* Not used, but the data is still allocated for forward compatibility. */
  node->storage = MEM_callocN<NodeTwoFloats>(__func__);
}

static void node_composit_buts_alphaover(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "use_premultiply", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
}

using namespace blender::compositor;

static bool get_use_premultiply(const bNode &node)
{
  return node.custom1;
}

static int node_gpu_material(GPUMaterial *material,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *inputs,
                             GPUNodeStack *outputs)
{
  if (get_use_premultiply(*node)) {
    return GPU_stack_link(material, node, "node_composite_alpha_over_key", inputs, outputs);
  }

  return GPU_stack_link(material, node, "node_composite_alpha_over_premultiply", inputs, outputs);
}

static float4 alpha_over_key(const float factor, const float4 &color, const float4 &over_color)
{
  if (over_color.w <= 0.0f) {
    return color;
  }

  if (factor == 1.0f && over_color.w >= 1.0f) {
    return over_color;
  }

  return math::interpolate(color, float4(over_color.xyz(), 1.0f), factor * over_color.w);
}

static float4 alpha_over_premultiply(const float factor,
                                     const float4 &color,
                                     const float4 &over_color)
{
  if (over_color.w < 0.0f) {
    return color;
  }

  if (factor == 1.0f && over_color.w >= 1.0f) {
    return over_color;
  }

  float multiplier = 1.0f - factor * over_color.w;
  return multiplier * color + factor * over_color;
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static auto key_function = mf::build::SI3_SO<float, float4, float4, float4>(
      "Alpha Over Key",
      [=](const float factor, const float4 &color, const float4 &over_color) -> float4 {
        return alpha_over_key(factor, color, over_color);
      },
      mf::build::exec_presets::SomeSpanOrSingle<1, 2>());

  static auto premultiply_function = mf::build::SI3_SO<float, float4, float4, float4>(
      "Alpha Over Premultiply",
      [=](const float factor, const float4 &color, const float4 &over_color) -> float4 {
        return alpha_over_premultiply(factor, color, over_color);
      },
      mf::build::exec_presets::SomeSpanOrSingle<1, 2>());

  if (get_use_premultiply(builder.node())) {
    builder.set_matching_fn(key_function);
  }
  else {
    builder.set_matching_fn(premultiply_function);
  }
}

}  // namespace blender::nodes::node_composite_alpha_over_cc

void register_node_type_cmp_alphaover()
{
  namespace file_ns = blender::nodes::node_composite_alpha_over_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeAlphaOver", CMP_NODE_ALPHAOVER);
  ntype.ui_name = "Alpha Over";
  ntype.ui_description = "Overlay a foreground image onto a background image";
  ntype.enum_name_legacy = "ALPHAOVER";
  ntype.nclass = NODE_CLASS_OP_COLOR;
  ntype.declare = file_ns::cmp_node_alphaover_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_alphaover;
  ntype.initfunc = file_ns::node_alphaover_init;
  blender::bke::node_type_storage(
      ntype, "NodeTwoFloats", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_gpu_material;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(ntype);
}
