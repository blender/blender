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

#include "COM_shader_node.hh"

#include "node_composite_util.hh"

/* **************** INVERT ******************** */

namespace blender::nodes::node_composite_invert_cc {

static void cmp_node_invert_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Fac")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .compositor_domain_priority(1);
  b.add_input<decl::Color>("Color")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_output<decl::Color>("Color");
}

static void node_composit_init_invert(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom1 |= CMP_CHAN_RGB;
}

static void node_composit_buts_invert(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "invert_rgb", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(col, ptr, "invert_alpha", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

static bool should_invert_rgb(const bNode &node)
{
  return node.custom1 & CMP_CHAN_RGB;
}

static bool should_invert_alpha(const bNode &node)
{
  return node.custom1 & CMP_CHAN_A;
}

class InvertShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    const float do_rgb = should_invert_rgb(bnode());
    const float do_alpha = should_invert_alpha(bnode());

    GPU_stack_link(material,
                   &bnode(),
                   "node_composite_invert",
                   inputs,
                   outputs,
                   GPU_constant(&do_rgb),
                   GPU_constant(&do_alpha));
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new InvertShaderNode(node);
}

template<bool ShouldInvertRGB, bool ShouldInvertAlpha>
static float4 invert(const float factor, const float4 &color)
{
  float4 result = color;
  if constexpr (ShouldInvertRGB) {
    result = float4(1.0f - result.xyz(), result.w);
  }
  if constexpr (ShouldInvertAlpha) {
    result = float4(result.xyz(), 1.0f - result.w);
  }
  return math::interpolate(color, result, factor);
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static auto rgb_alpha_function = mf::build::SI2_SO<float, float4, float4>(
      "Invert RGB Alpha",
      [](const float factor, const float4 &color) -> float4 {
        return invert<true, true>(factor, color);
      },
      mf::build::exec_presets::SomeSpanOrSingle<1>());

  static auto rgb_function = mf::build::SI2_SO<float, float4, float4>(
      "Invert RGB",
      [](const float factor, const float4 &color) -> float4 {
        return invert<true, false>(factor, color);
      },
      mf::build::exec_presets::SomeSpanOrSingle<1>());

  static auto alpha_function = mf::build::SI2_SO<float, float4, float4>(
      "Invert Alpha",
      [](const float factor, const float4 &color) -> float4 {
        return invert<false, true>(factor, color);
      },
      mf::build::exec_presets::SomeSpanOrSingle<1>());

  static auto identity_function = mf::build::SI2_SO<float, float4, float4>(
      "Identity",
      [](const float /*factor*/, const float4 &color) -> float4 { return color; },
      mf::build::exec_presets::SomeSpanOrSingle<1>());

  if (should_invert_rgb(builder.node())) {
    if (should_invert_alpha(builder.node())) {
      builder.set_matching_fn(rgb_alpha_function);
    }
    else {
      builder.set_matching_fn(rgb_function);
    }
  }
  else {
    if (should_invert_alpha(builder.node())) {
      builder.set_matching_fn(alpha_function);
    }
    else {
      builder.set_matching_fn(identity_function);
    }
  }
}

}  // namespace blender::nodes::node_composite_invert_cc

void register_node_type_cmp_invert()
{
  namespace file_ns = blender::nodes::node_composite_invert_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_INVERT, "Invert Color", NODE_CLASS_OP_COLOR);
  ntype.declare = file_ns::cmp_node_invert_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_invert;
  ntype.initfunc = file_ns::node_composit_init_invert;
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(&ntype);
}
