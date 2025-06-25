/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BKE_node.hh"
#include "BLI_math_base.hh"
#include "BLI_math_color.h"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "BKE_node_runtime.hh"

#include "NOD_multi_function.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "GPU_material.hh"

#include "IMB_colormanagement.hh"

#include "BLI_math_color.hh"

#include "node_composite_util.hh"

/* ******************* Color Balance ********************************* */

namespace blender::nodes::node_composite_colorbalance_cc {

static void cmp_node_colorbalance_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.use_custom_socket_order();

  b.add_output<decl::Color>("Image");

  b.add_layout([](uiLayout *layout, bContext * /*C*/, PointerRNA *ptr) {
    layout->prop(ptr, "correction_method", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  });

  b.add_input<decl::Float>("Fac")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .compositor_domain_priority(1);
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);

  b.add_input<decl::Float>("Lift", "Base Lift")
      .default_value(0.0f)
      .min(-1.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description("Correction for shadows");
  b.add_input<decl::Color>("Lift", "Color Lift")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .description("Correction for shadows");
  b.add_input<decl::Float>("Gamma", "Base Gamma")
      .default_value(1.0f)
      .min(0.0f)
      .max(2.0f)
      .subtype(PROP_FACTOR)
      .description("Correction for midtones");
  b.add_input<decl::Color>("Gamma", "Color Gamma")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .description("Correction for midtones");
  b.add_input<decl::Float>("Gain", "Base Gain")
      .default_value(1.0f)
      .min(0.0f)
      .max(2.0f)
      .subtype(PROP_FACTOR)
      .description("Correction for highlights");
  b.add_input<decl::Color>("Gain", "Color Gain")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .description("Correction for highlights");

  b.add_input<decl::Float>("Offset", "Base Offset")
      .default_value(0.0f)
      .min(-1.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description("Correction for shadows");
  b.add_input<decl::Color>("Offset", "Color Offset")
      .default_value({0.0f, 0.0f, 0.0f, 1.0f})
      .description("Correction for shadows");
  b.add_input<decl::Float>("Power", "Base Power")
      .default_value(1.0f)
      .min(0.0f)
      .max(2.0f)
      .subtype(PROP_FACTOR)
      .description("Correction for midtones");
  b.add_input<decl::Color>("Power", "Color Power")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .description("Correction for midtones");
  b.add_input<decl::Float>("Slope", "Base Slope")
      .default_value(1.0f)
      .min(0.0f)
      .max(2.0f)
      .subtype(PROP_FACTOR)
      .description("Correction for highlights");
  b.add_input<decl::Color>("Slope", "Color Slope")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .description("Correction for highlights");

  PanelDeclarationBuilder &input_panel = b.add_panel("Input");
  input_panel.add_input<decl::Float>("Temperature", "Input Temperature")
      .default_value(6500.0f)
      .subtype(PROP_COLOR_TEMPERATURE)
      .min(1800.0f)
      .max(100000.0f)
      .description("Color temperature of the input's white point");
  input_panel.add_input<decl::Float>("Tint", "Input Tint")
      .default_value(10.0f)
      .subtype(PROP_FACTOR)
      .min(-150.0f)
      .max(150.0f)
      .description("Color tint of the input's white point (the default of 10 matches daylight)");
  input_panel.add_layout([](uiLayout *layout, bContext * /*C*/, PointerRNA *ptr) {
    uiLayout *split = &layout->split(0.2f, false);
    uiTemplateCryptoPicker(split, ptr, "input_whitepoint", ICON_EYEDROPPER);
  });

  PanelDeclarationBuilder &output_panel = b.add_panel("Output");
  output_panel.add_input<decl::Float>("Temperature", "Output Temperature")
      .default_value(6500.0f)
      .subtype(PROP_COLOR_TEMPERATURE)
      .min(1800.0f)
      .max(100000.0f)
      .description("Color temperature of the output's white point");
  output_panel.add_input<decl::Float>("Tint", "Output Tint")
      .default_value(10.0f)
      .subtype(PROP_FACTOR)
      .min(-150.0f)
      .max(150.0f)
      .description("Color tint of the output's white point (the default of 10 matches daylight)");
  output_panel.add_layout([](uiLayout *layout, bContext * /*C*/, PointerRNA *ptr) {
    uiLayout *split = &layout->split(0.2f, false);
    uiTemplateCryptoPicker(split, ptr, "output_whitepoint", ICON_EYEDROPPER);
  });
}

static CMPNodeColorBalanceMethod get_color_balance_method(const bNode &node)
{
  return static_cast<CMPNodeColorBalanceMethod>(node.custom1);
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const bool is_lgg = get_color_balance_method(*node) == CMP_NODE_COLOR_BALANCE_LGG;
  bNodeSocket *base_lift_input = bke::node_find_socket(*node, SOCK_IN, "Base Lift");
  bNodeSocket *base_gamma_input = bke::node_find_socket(*node, SOCK_IN, "Base Gamma");
  bNodeSocket *base_gain_input = bke::node_find_socket(*node, SOCK_IN, "Base Gain");
  bNodeSocket *color_lift_input = bke::node_find_socket(*node, SOCK_IN, "Color Lift");
  bNodeSocket *color_gamma_input = bke::node_find_socket(*node, SOCK_IN, "Color Gamma");
  bNodeSocket *color_gain_input = bke::node_find_socket(*node, SOCK_IN, "Color Gain");
  blender::bke::node_set_socket_availability(*ntree, *base_lift_input, is_lgg);
  blender::bke::node_set_socket_availability(*ntree, *base_gamma_input, is_lgg);
  blender::bke::node_set_socket_availability(*ntree, *base_gain_input, is_lgg);
  blender::bke::node_set_socket_availability(*ntree, *color_lift_input, is_lgg);
  blender::bke::node_set_socket_availability(*ntree, *color_gamma_input, is_lgg);
  blender::bke::node_set_socket_availability(*ntree, *color_gain_input, is_lgg);

  const bool is_cdl = get_color_balance_method(*node) == CMP_NODE_COLOR_BALANCE_ASC_CDL;
  bNodeSocket *base_offset_input = bke::node_find_socket(*node, SOCK_IN, "Base Offset");
  bNodeSocket *base_power_input = bke::node_find_socket(*node, SOCK_IN, "Base Power");
  bNodeSocket *base_slope_input = bke::node_find_socket(*node, SOCK_IN, "Base Slope");
  bNodeSocket *color_offset_input = bke::node_find_socket(*node, SOCK_IN, "Color Offset");
  bNodeSocket *color_power_input = bke::node_find_socket(*node, SOCK_IN, "Color Power");
  bNodeSocket *color_slope_input = bke::node_find_socket(*node, SOCK_IN, "Color Slope");
  blender::bke::node_set_socket_availability(*ntree, *base_offset_input, is_cdl);
  blender::bke::node_set_socket_availability(*ntree, *base_power_input, is_cdl);
  blender::bke::node_set_socket_availability(*ntree, *base_slope_input, is_cdl);
  blender::bke::node_set_socket_availability(*ntree, *color_offset_input, is_cdl);
  blender::bke::node_set_socket_availability(*ntree, *color_power_input, is_cdl);
  blender::bke::node_set_socket_availability(*ntree, *color_slope_input, is_cdl);

  const bool is_white_point = get_color_balance_method(*node) == CMP_NODE_COLOR_BALANCE_WHITEPOINT;
  bNodeSocket *input_temperature_input = bke::node_find_socket(
      *node, SOCK_IN, "Input Temperature");
  bNodeSocket *input_tint_input = bke::node_find_socket(*node, SOCK_IN, "Input Tint");
  bNodeSocket *output_temperature_input = bke::node_find_socket(
      *node, SOCK_IN, "Output Temperature");
  bNodeSocket *output_tint_input = bke::node_find_socket(*node, SOCK_IN, "Output Tint");
  blender::bke::node_set_socket_availability(*ntree, *input_temperature_input, is_white_point);
  blender::bke::node_set_socket_availability(*ntree, *input_tint_input, is_white_point);
  blender::bke::node_set_socket_availability(*ntree, *output_temperature_input, is_white_point);
  blender::bke::node_set_socket_availability(*ntree, *output_tint_input, is_white_point);
}

static float3x3 get_white_point_matrix(const float input_temperature,
                                       const float input_tint,
                                       const float output_temperature,
                                       const float output_tint)
{
  const float3x3 scene_to_xyz = IMB_colormanagement_get_scene_linear_to_xyz();
  const float3x3 xyz_to_scene = IMB_colormanagement_get_xyz_to_scene_linear();
  const float3 input = blender::math::whitepoint_from_temp_tint(input_temperature, input_tint);
  const float3 output = blender::math::whitepoint_from_temp_tint(output_temperature, output_tint);
  const float3x3 adaption = blender::math::chromatic_adaption_matrix(input, output);
  return xyz_to_scene * adaption * scene_to_xyz;
}

static int node_gpu_material(GPUMaterial *material,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *inputs,
                             GPUNodeStack *outputs)
{
  switch (get_color_balance_method(*node)) {
    case CMP_NODE_COLOR_BALANCE_LGG: {
      return GPU_stack_link(material, node, "node_composite_color_balance_lgg", inputs, outputs);
    }
    case CMP_NODE_COLOR_BALANCE_ASC_CDL: {
      return GPU_stack_link(
          material, node, "node_composite_color_balance_asc_cdl", inputs, outputs);
    }
    case CMP_NODE_COLOR_BALANCE_WHITEPOINT: {
      const bNodeSocket &input_temperature = node->input_by_identifier("Input Temperature");
      const bNodeSocket &input_tint = node->input_by_identifier("Input Tint");
      const bNodeSocket &output_temperature = node->input_by_identifier("Output Temperature");
      const bNodeSocket &output_tint = node->input_by_identifier("Output Tint");

      /* If all inputs are not linked, compute the white point matrix on the host and pass it to
       * the shader. */
      if (input_temperature.is_directly_linked() && input_tint.is_directly_linked() &&
          output_temperature.is_directly_linked() && output_tint.is_directly_linked())
      {
        const float3x3 white_point_matrix = get_white_point_matrix(
            input_temperature.default_value_typed<bNodeSocketValueFloat>()->value,
            input_tint.default_value_typed<bNodeSocketValueFloat>()->value,
            output_temperature.default_value_typed<bNodeSocketValueFloat>()->value,
            output_tint.default_value_typed<bNodeSocketValueFloat>()->value);
        return GPU_stack_link(material,
                              node,
                              "node_composite_color_balance_white_point_constant",
                              inputs,
                              outputs,
                              GPU_uniform(blender::float4x4(white_point_matrix).base_ptr()));
      }

      const float3x3 scene_to_xyz = IMB_colormanagement_get_scene_linear_to_xyz();
      const float3x3 xyz_to_scene = IMB_colormanagement_get_xyz_to_scene_linear();
      return GPU_stack_link(material,
                            node,
                            "node_composite_color_balance_white_point_variable",
                            inputs,
                            outputs,
                            GPU_uniform(blender::float4x4(scene_to_xyz).base_ptr()),
                            GPU_uniform(blender::float4x4(xyz_to_scene).base_ptr()));
    }
  }

  return false;
}

static float4 color_balance_lgg(const float factor,
                                const float4 &color,
                                const float &base_lift,
                                const float4 &color_lift,
                                const float &base_gamma,
                                const float4 &color_gamma,
                                const float &base_gain,
                                const float4 &color_gain)
{
  float3 srgb_color;
  linearrgb_to_srgb_v3_v3(srgb_color, color);

  const float3 lift = base_lift + color_lift.xyz();
  const float3 lift_balanced = ((srgb_color - 1.0f) * (2.0f - lift)) + 1.0f;

  const float3 gain = base_gain * color_gain.xyz();
  float3 gain_balanced = lift_balanced * gain;
  gain_balanced = math::max(gain_balanced, float3(0.0f));

  float3 linear_color;
  srgb_to_linearrgb_v3_v3(linear_color, gain_balanced);

  const float3 gamma = base_gamma * color_gamma.xyz();
  float3 gamma_balanced = math::pow(linear_color, 1.0f / math::max(gamma, float3(1e-6f)));

  return float4(math::interpolate(color.xyz(), gamma_balanced, math::min(factor, 1.0f)), color.w);
}

static float4 color_balance_asc_cdl(const float factor,
                                    const float4 &color,
                                    const float &base_offset,
                                    const float4 &color_offset,
                                    const float &base_power,
                                    const float4 &color_power,
                                    const float &base_slope,
                                    const float4 &color_slope)
{
  const float3 slope = base_slope * color_slope.xyz();
  const float3 slope_balanced = color.xyz() * slope;

  const float3 offset = base_offset + color_offset.xyz();
  const float3 offset_balanced = slope_balanced + offset;

  const float3 power = base_power * color_power.xyz();
  const float3 power_balanced = math::pow(math::max(offset_balanced, float3(0.0f)), power);

  return float4(math::interpolate(color.xyz(), power_balanced, math::min(factor, 1.0f)), color.w);
}

static float4 color_balance_white_point_constant(const float factor,
                                                 const float4 &color,
                                                 const float3x3 &white_point_matrix)
{
  const float3 balanced = white_point_matrix * color.xyz();
  return float4(math::interpolate(color.xyz(), balanced, math::min(factor, 1.0f)), color.w);
}

static float4 color_balance_white_point_variable(const float factor,
                                                 const float4 &color,
                                                 const float input_temperature,
                                                 const float input_tint,
                                                 const float output_temperature,
                                                 const float output_tint,
                                                 const float3x3 &scene_to_xyz,
                                                 const float3x3 &xyz_to_scene)
{
  const float3 input = blender::math::whitepoint_from_temp_tint(input_temperature, input_tint);
  const float3 output = blender::math::whitepoint_from_temp_tint(output_temperature, output_tint);
  const float3x3 adaption = blender::math::chromatic_adaption_matrix(input, output);
  const float3x3 white_point_matrix = xyz_to_scene * adaption * scene_to_xyz;

  const float3 balanced = white_point_matrix * color.xyz();
  return float4(math::interpolate(color.xyz(), balanced, math::min(factor, 1.0f)), color.w);
}

class ColorBalanceWhitePointFunction : public mf::MultiFunction {
 public:
  ColorBalanceWhitePointFunction()
  {
    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"Color Balance White Point", signature};
      builder.single_input<float>("Factor");
      builder.single_input<float4>("Color");
      builder.single_input<float>("Input Temperature");
      builder.single_input<float>("Input Tint");
      builder.single_input<float>("Output Temperature");
      builder.single_input<float>("Output Tint");
      builder.single_output<float4>("Result");
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<float> factor_array = params.readonly_single_input<float>(0, "Factor");
    const VArray<float4> color_array = params.readonly_single_input<float4>(1, "Color");
    const VArray<float> input_temperature_array = params.readonly_single_input<float>(
        2, "Input Temperature");
    const VArray<float> input_tint_array = params.readonly_single_input<float>(3, "Input Tint");
    const VArray<float> output_temperature_array = params.readonly_single_input<float>(
        4, "Output Temperature");
    const VArray<float> output_tint_array = params.readonly_single_input<float>(5, "Output Tint");

    MutableSpan<float4> result = params.uninitialized_single_output<float4>(6, "Result");

    const std::optional<float> input_temperature_single = input_temperature_array.get_if_single();
    const std::optional<float> input_tint_single = input_tint_array.get_if_single();
    const std::optional<float> output_temperature_single =
        output_temperature_array.get_if_single();
    const std::optional<float> output_tint_single = output_tint_array.get_if_single();

    if (input_temperature_single.has_value() && input_tint_single.has_value() &&
        output_temperature_single.has_value() && output_tint_single.has_value())
    {
      const float3x3 white_point_matrix = get_white_point_matrix(input_temperature_single.value(),
                                                                 input_tint_single.value(),
                                                                 output_temperature_single.value(),
                                                                 output_tint_single.value());
      mask.foreach_index([&](const int64_t i) {
        result[i] = color_balance_white_point_constant(
            factor_array[i], color_array[i], white_point_matrix);
      });
    }
    else {
      const float3x3 scene_to_xyz = IMB_colormanagement_get_scene_linear_to_xyz();
      const float3x3 xyz_to_scene = IMB_colormanagement_get_xyz_to_scene_linear();

      mask.foreach_index([&](const int64_t i) {
        result[i] = color_balance_white_point_variable(factor_array[i],
                                                       color_array[i],
                                                       input_temperature_array[i],
                                                       input_tint_array[i],
                                                       output_temperature_array[i],
                                                       output_tint_array[i],
                                                       scene_to_xyz,
                                                       xyz_to_scene);
      });
    }
  }
};

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  switch (get_color_balance_method(builder.node())) {
    case CMP_NODE_COLOR_BALANCE_LGG: {
      builder.construct_and_set_matching_fn_cb([=]() {
        return mf::build::
            SI8_SO<float, float4, float, float4, float, float4, float, float4, float4>(
                "Color Balance LGG",
                [=](const float factor,
                    const float4 &color,
                    const float base_lift,
                    const float4 &color_lift,
                    const float base_gamma,
                    const float4 &color_gamma,
                    const float base_gain,
                    const float4 &color_gain) -> float4 {
                  return color_balance_lgg(factor,
                                           color,
                                           base_lift,
                                           color_lift,
                                           base_gamma,
                                           color_gamma,
                                           base_gain,
                                           color_gain);
                },
                mf::build::exec_presets::SomeSpanOrSingle<1>());
      });
      break;
    }
    case CMP_NODE_COLOR_BALANCE_ASC_CDL: {
      builder.construct_and_set_matching_fn_cb([=]() {
        return mf::build::
            SI8_SO<float, float4, float, float4, float, float4, float, float4, float4>(
                "Color Balance ASC CDL",
                [=](const float factor,
                    const float4 &color,
                    const float base_offset,
                    const float4 &color_offset,
                    const float base_power,
                    const float4 &color_power,
                    const float base_slope,
                    const float4 &color_slope) -> float4 {
                  return color_balance_asc_cdl(factor,
                                               color,
                                               base_offset,
                                               color_offset,
                                               base_power,
                                               color_power,
                                               base_slope,
                                               color_slope);
                },
                mf::build::exec_presets::SomeSpanOrSingle<1>());
      });
      break;
    }
    case CMP_NODE_COLOR_BALANCE_WHITEPOINT: {
      const static ColorBalanceWhitePointFunction function;
      builder.set_matching_fn(function);
      break;
    }
  }
}

}  // namespace blender::nodes::node_composite_colorbalance_cc

static void register_node_type_cmp_colorbalance()
{
  namespace file_ns = blender::nodes::node_composite_colorbalance_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeColorBalance", CMP_NODE_COLORBALANCE);
  ntype.ui_name = "Color Balance";
  ntype.ui_description = "Adjust color and values";
  ntype.enum_name_legacy = "COLORBALANCE";
  ntype.nclass = NODE_CLASS_OP_COLOR;
  ntype.declare = file_ns::cmp_node_colorbalance_declare;
  ntype.updatefunc = file_ns::node_update;
  ntype.gpu_fn = file_ns::node_gpu_material;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_colorbalance)
