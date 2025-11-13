/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.hh"
#include "BLI_math_color.h"
#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "BKE_node.hh"

#include "UI_resources.hh"

#include "GPU_material.hh"

#include "COM_result.hh"

#include "node_composite_util.hh"

/* ******************* Color Matte ********************************************************** */

namespace blender::nodes::node_composite_color_matte_cc {

static void cmp_node_color_matte_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.is_function_node();
  b.add_input<decl::Color>("Image").default_value({1.0f, 1.0f, 1.0f, 1.0f}).hide_value();
  b.add_output<decl::Color>("Image").align_with_previous();
  b.add_output<decl::Float>("Matte");

  b.add_input<decl::Color>("Key Color").default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>("Hue")
      .default_value(0.01f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(1.0f)
      .description(
          "If the difference in hue between the color and key color is less than this threshold, "
          "it is keyed");
  b.add_input<decl::Float>("Saturation")
      .default_value(0.1f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(1.0f)
      .description(
          "If the difference in saturation between the color and key color is less than this "
          "threshold, it is keyed");
  b.add_input<decl::Float>("Value")
      .default_value(0.1f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(1.0f)
      .description(
          "If the difference in value between the color and key color is less than this "
          "threshold, it is keyed");
}

using namespace blender::compositor;

static int node_gpu_material(GPUMaterial *material,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *inputs,
                             GPUNodeStack *outputs)
{
  return GPU_stack_link(material, node, "node_composite_color_matte", inputs, outputs);
}

static void color_matte(const float4 color,
                        const float4 key,
                        const float hue_threshold,
                        const float saturation_epsilon,
                        const float value_epsilon,
                        float4 &result,
                        float &matte)
{
  float3 color_hsva;
  rgb_to_hsv_v(color, color_hsva);
  float3 key_hsva;
  rgb_to_hsv_v(key, key_hsva);

  /* Divide by 2 because the hue wraps around. */
  float hue_epsilon = hue_threshold / 2.0f;

  bool is_within_saturation = math::distance(color_hsva.y, key_hsva.y) < saturation_epsilon;
  bool is_within_value = math::distance(color_hsva.z, key_hsva.z) < value_epsilon;
  bool is_within_hue = math::distance(color_hsva.x, key_hsva.x) < hue_epsilon;
  /* Hue wraps around, so check the distance around the boundary. */
  float min_hue = math::min(color_hsva.x, key_hsva.x);
  float max_hue = math::max(color_hsva.x, key_hsva.x);
  is_within_hue = is_within_hue || ((min_hue + (1.0f - max_hue)) < hue_epsilon);

  matte = (is_within_hue && is_within_saturation && is_within_value) ? 0.0f : color.w;
  result = color * matte;
}

using blender::compositor::Color;

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  builder.construct_and_set_matching_fn_cb([=]() {
    return mf::build::SI5_SO2<Color, Color, float, float, float, Color, float>(
        "Color Key",
        [=](const Color &color,
            const Color &key_color,
            const float &hue,
            const float &saturation,
            const float &value,
            Color &output_color,
            float &matte) -> void {
          float4 out_color;
          color_matte(float4(color), float4(key_color), hue, saturation, value, out_color, matte);
          output_color = Color(out_color);
        },
        mf::build::exec_presets::SomeSpanOrSingle<0, 1>());
  });
}

}  // namespace blender::nodes::node_composite_color_matte_cc

static void register_node_type_cmp_color_matte()
{
  namespace file_ns = blender::nodes::node_composite_color_matte_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeColorMatte", CMP_NODE_COLOR_MATTE);
  ntype.ui_name = "Color Key";
  ntype.ui_description = "Create matte using a given color, for green or blue screen footage";
  ntype.enum_name_legacy = "COLOR_MATTE";
  ntype.nclass = NODE_CLASS_MATTE;
  ntype.declare = file_ns::cmp_node_color_matte_declare;
  ntype.flag |= NODE_PREVIEW;
  ntype.gpu_fn = file_ns::node_gpu_material;
  ntype.build_multi_function = file_ns::node_build_multi_function;
  blender::bke::node_type_size(ntype, 155, 140, NODE_DEFAULT_MAX_WIDTH);

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_color_matte)
