/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include <limits>

#include "BLI_math_color.h"
#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_material.hh"

#include "COM_shader_node.hh"

#include "node_composite_util.hh"

/* **************** Brightness and Contrast  ******************** */

namespace blender::nodes::node_composite_brightness_cc {

static void cmp_node_brightcontrast_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Float>("Bright").min(-100.0f).max(100.0f).compositor_domain_priority(1);
  b.add_input<decl::Float>("Contrast").min(-100.0f).max(100.0f).compositor_domain_priority(2);
  b.add_output<decl::Color>("Image");
}

static void node_composit_init_brightcontrast(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom1 = 1;
}

static void node_composit_buts_brightcontrast(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "use_premultiply", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class BrightContrastShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    const float use_premultiply = get_use_premultiply();

    GPU_stack_link(material,
                   &bnode(),
                   "node_composite_bright_contrast",
                   inputs,
                   outputs,
                   GPU_constant(&use_premultiply));
  }

  bool get_use_premultiply()
  {
    return bnode().custom1;
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new BrightContrastShaderNode(node);
}

/* The algorithm is by Werner D. Streidt, extracted of OpenCV `demhist.c`:
 *   http://visca.com/ffactory/archives/5-99/msg00021.html */
template<bool UsePremultiply>
static float4 brightness_and_contrast(const float4 &color,
                                      const float brightness,
                                      const float contrast)
{
  float scaled_brightness = brightness / 100.0f;
  float delta = contrast / 200.0f;

  float multiplier, offset;
  if (contrast > 0.0f) {
    multiplier = 1.0f - delta * 2.0f;
    multiplier = 1.0f / math::max(multiplier, std::numeric_limits<float>::epsilon());
    offset = multiplier * (scaled_brightness - delta);
  }
  else {
    delta *= -1.0f;
    multiplier = math::max(1.0f - delta * 2.0f, 0.0f);
    offset = multiplier * scaled_brightness + delta;
  }

  float4 input_color = color;
  if constexpr (UsePremultiply) {
    premul_to_straight_v4(input_color);
  }

  float4 result = float4(input_color.xyz() * multiplier + offset, input_color.w);

  if constexpr (UsePremultiply) {
    straight_to_premul_v4(result);
  }
  return result;
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static auto premultiply_used_function = mf::build::SI3_SO<float4, float, float, float4>(
      "Bright And Contrast Use Premultiply",
      [](const float4 &color, const float brightness, const float contrast) -> float4 {
        return brightness_and_contrast<true>(color, brightness, contrast);
      },
      mf::build::exec_presets::SomeSpanOrSingle<0>());

  static auto no_premultiply_function = mf::build::SI3_SO<float4, float, float, float4>(
      "Bright And Contrast No Premultiply",
      [](const float4 &color, const float brightness, const float contrast) -> float4 {
        return brightness_and_contrast<false>(color, brightness, contrast);
      },
      mf::build::exec_presets::SomeSpanOrSingle<0>());

  const bool use_premultiply = builder.node().custom1;
  if (use_premultiply) {
    builder.set_matching_fn(premultiply_used_function);
  }
  else {
    builder.set_matching_fn(no_premultiply_function);
  }
}

}  // namespace blender::nodes::node_composite_brightness_cc

void register_node_type_cmp_brightcontrast()
{
  namespace file_ns = blender::nodes::node_composite_brightness_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_BRIGHTCONTRAST, "Brightness/Contrast", NODE_CLASS_OP_COLOR);
  ntype.declare = file_ns::cmp_node_brightcontrast_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_brightcontrast;
  ntype.initfunc = file_ns::node_composit_init_brightcontrast;
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(&ntype);
}
