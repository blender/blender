/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.hh"
#include "BLI_math_color.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

#include "NOD_multi_function.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "GPU_material.hh"

#include "IMB_colormanagement.hh"

#include "COM_result.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_colorbalance_cc {

static const EnumPropertyItem type_items[] = {
    {CMP_NODE_COLOR_BALANCE_LGG, "LIFT_GAMMA_GAIN", 0, N_("Lift/Gamma/Gain"), ""},
    {CMP_NODE_COLOR_BALANCE_ASC_CDL,
     "OFFSET_POWER_SLOPE",
     0,
     N_("Offset/Power/Slope (ASC-CDL)"),
     N_("ASC-CDL standard color correction")},
    {CMP_NODE_COLOR_BALANCE_WHITEPOINT,
     "WHITEPOINT",
     0,
     N_("White Point"),
     N_("Chromatic adaption from a different white point")},
    {0, nullptr, 0, nullptr, nullptr},
};

static void cmp_node_colorbalance_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_input<decl::Color>("Image").default_value({1.0f, 1.0f, 1.0f, 1.0f}).hide_value();
  b.add_output<decl::Color>("Image").align_with_previous();

  b.add_input<decl::Float>("Factor", "Fac")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);

  b.add_input<decl::Menu>("Type")
      .default_value(CMP_NODE_COLOR_BALANCE_LGG)
      .static_items(type_items)
      .optional_label();

  b.add_input<decl::Float>("Lift", "Base Lift")
      .default_value(0.0f)
      .min(-1.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .usage_by_menu("Type", CMP_NODE_COLOR_BALANCE_LGG)
      .description("Correction for shadows");
  b.add_input<decl::Color>("Lift", "Color Lift")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .usage_by_menu("Type", CMP_NODE_COLOR_BALANCE_LGG)
      .description("Correction for shadows");
  b.add_input<decl::Float>("Gamma", "Base Gamma")
      .default_value(1.0f)
      .min(0.0f)
      .max(2.0f)
      .subtype(PROP_FACTOR)
      .usage_by_menu("Type", CMP_NODE_COLOR_BALANCE_LGG)
      .description("Correction for midtones");
  b.add_input<decl::Color>("Gamma", "Color Gamma")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .usage_by_menu("Type", CMP_NODE_COLOR_BALANCE_LGG)
      .description("Correction for midtones");
  b.add_input<decl::Float>("Gain", "Base Gain")
      .default_value(1.0f)
      .min(0.0f)
      .max(2.0f)
      .subtype(PROP_FACTOR)
      .usage_by_menu("Type", CMP_NODE_COLOR_BALANCE_LGG)
      .description("Correction for highlights");
  b.add_input<decl::Color>("Gain", "Color Gain")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .usage_by_menu("Type", CMP_NODE_COLOR_BALANCE_LGG)
      .description("Correction for highlights");

  b.add_input<decl::Float>("Offset", "Base Offset")
      .default_value(0.0f)
      .min(-1.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .usage_by_menu("Type", CMP_NODE_COLOR_BALANCE_ASC_CDL)
      .description("Correction for shadows");
  b.add_input<decl::Color>("Offset", "Color Offset")
      .default_value({0.0f, 0.0f, 0.0f, 1.0f})
      .usage_by_menu("Type", CMP_NODE_COLOR_BALANCE_ASC_CDL)
      .description("Correction for shadows");
  b.add_input<decl::Float>("Power", "Base Power")
      .default_value(1.0f)
      .min(0.0f)
      .max(2.0f)
      .subtype(PROP_FACTOR)
      .usage_by_menu("Type", CMP_NODE_COLOR_BALANCE_ASC_CDL)
      .description("Correction for midtones");
  b.add_input<decl::Color>("Power", "Color Power")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .usage_by_menu("Type", CMP_NODE_COLOR_BALANCE_ASC_CDL)
      .description("Correction for midtones");
  b.add_input<decl::Float>("Slope", "Base Slope")
      .default_value(1.0f)
      .min(0.0f)
      .max(2.0f)
      .subtype(PROP_FACTOR)
      .usage_by_menu("Type", CMP_NODE_COLOR_BALANCE_ASC_CDL)
      .description("Correction for highlights");
  b.add_input<decl::Color>("Slope", "Color Slope")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .usage_by_menu("Type", CMP_NODE_COLOR_BALANCE_ASC_CDL)
      .description("Correction for highlights");

  PanelDeclarationBuilder &input_panel = b.add_panel("Input");
  input_panel.add_input<decl::Float>("Temperature", "Input Temperature")
      .default_value(6500.0f)
      .subtype(PROP_COLOR_TEMPERATURE)
      .min(1800.0f)
      .max(100000.0f)
      .usage_by_menu("Type", CMP_NODE_COLOR_BALANCE_WHITEPOINT)
      .description("Color temperature of the input's white point");
  input_panel.add_input<decl::Float>("Tint", "Input Tint")
      .default_value(10.0f)
      .subtype(PROP_FACTOR)
      .min(-150.0f)
      .max(150.0f)
      .usage_by_menu("Type", CMP_NODE_COLOR_BALANCE_WHITEPOINT)
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
      .usage_by_menu("Type", CMP_NODE_COLOR_BALANCE_WHITEPOINT)
      .description("Color temperature of the output's white point");
  output_panel.add_input<decl::Float>("Tint", "Output Tint")
      .default_value(10.0f)
      .subtype(PROP_FACTOR)
      .min(-150.0f)
      .max(150.0f)
      .usage_by_menu("Type", CMP_NODE_COLOR_BALANCE_WHITEPOINT)
      .description("Color tint of the output's white point (the default of 10 matches daylight)");
  output_panel.add_layout([](uiLayout *layout, bContext * /*C*/, PointerRNA *ptr) {
    uiLayout *split = &layout->split(0.2f, false);
    uiTemplateCryptoPicker(split, ptr, "output_whitepoint", ICON_EYEDROPPER);
  });
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
  const bNodeSocket &type = *node->input_by_identifier("Type");
  const bool is_white_point = !type.is_directly_linked() &&
                              type.default_value_typed<bNodeSocketValueMenu>()->value ==
                                  CMP_NODE_COLOR_BALANCE_WHITEPOINT;

  const bNodeSocket &input_temperature = *node->input_by_identifier("Input Temperature");
  const bNodeSocket &input_tint = *node->input_by_identifier("Input Tint");
  const bNodeSocket &output_temperature = *node->input_by_identifier("Output Temperature");
  const bNodeSocket &output_tint = *node->input_by_identifier("Output Tint");

  /* As an optimization for white point balancing, if all inputs are constant, compute the white
   * point matrix on the host and pass it to the shader. */
  if (is_white_point && !input_temperature.is_directly_linked() &&
      !input_tint.is_directly_linked() && !output_temperature.is_directly_linked() &&
      !output_tint.is_directly_linked())
  {
    const float4x4 white_point_matrix = float4x4(get_white_point_matrix(
        input_temperature.default_value_typed<bNodeSocketValueFloat>()->value,
        input_tint.default_value_typed<bNodeSocketValueFloat>()->value,
        output_temperature.default_value_typed<bNodeSocketValueFloat>()->value,
        output_tint.default_value_typed<bNodeSocketValueFloat>()->value));

    return GPU_stack_link(material,
                          node,
                          "node_composite_color_balance_white_point_constant",
                          inputs,
                          outputs,
                          GPU_uniform(white_point_matrix.base_ptr()));
  }

  const float4x4 scene_to_xyz = float4x4(IMB_colormanagement_get_scene_linear_to_xyz());
  const float4x4 xyz_to_scene = float4x4(IMB_colormanagement_get_xyz_to_scene_linear());
  return GPU_stack_link(material,
                        node,
                        "node_composite_color_balance",
                        inputs,
                        outputs,
                        GPU_uniform(scene_to_xyz.base_ptr()),
                        GPU_uniform(xyz_to_scene.base_ptr()));
}

static float4 lift_gamma_gain(const float4 color,
                              const float base_lift,
                              const float4 color_lift,
                              const float base_gamma,
                              const float4 color_gamma,
                              const float base_gain,
                              const float4 color_gain)
{
  const float3 lift = base_lift + color_lift.xyz();
  const float3 lift_balanced = ((color.xyz() - 1.0f) * (2.0f - lift)) + 1.0f;

  const float3 gain = base_gain * color_gain.xyz();
  const float3 gain_balanced = math::max(float3(0.0f), lift_balanced * gain);

  const float3 gamma = base_gamma * color_gamma.xyz();
  const float3 gamma_balanced = math::pow(gain_balanced, 1.0f / math::max(gamma, float3(1e-6f)));

  return float4(gamma_balanced, color.w);
}

static float4 offset_power_slope(const float4 color,
                                 const float base_offset,
                                 const float4 color_offset,
                                 const float base_power,
                                 const float4 color_power,
                                 const float base_slope,
                                 const float4 color_slope)
{
  const float3 slope = base_slope * color_slope.xyz();
  const float3 slope_balanced = color.xyz() * slope;

  const float3 offset = base_offset + color_offset.xyz();
  const float3 offset_balanced = slope_balanced + offset;

  const float3 power = base_power * color_power.xyz();
  const float3 power_balanced = math::pow(math::max(offset_balanced, float3(0.0f)), power);

  return float4(power_balanced, color.w);
}

static float4 white_point_constant(const float4 color,
                                   const float factor,
                                   const float3x3 white_point_matrix)
{
  const float3 balanced = white_point_matrix * color.xyz();
  return float4(math::interpolate(color.xyz(), balanced, math::min(factor, 1.0f)), color.w);
}

static float4 white_point_variable(const float4 color,
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
  return float4(balanced, color.w);
}

static float4 color_balance(const float4 color,
                            const float factor,
                            const CMPNodeColorBalanceMethod type,
                            const float base_lift,
                            const float4 color_lift,
                            const float base_gamma,
                            const float4 color_gamma,
                            const float base_gain,
                            const float4 color_gain,
                            const float base_offset,
                            const float4 color_offset,
                            const float base_power,
                            const float4 color_power,
                            const float base_slope,
                            const float4 color_slope,
                            const float input_temperature,
                            const float input_tint,
                            const float output_temperature,
                            const float output_tint,
                            const float3x3 scene_to_xyz,
                            const float3x3 xyz_to_scene)
{
  float4 result = float4(0.0f);
  switch (type) {
    case CMP_NODE_COLOR_BALANCE_LGG:
      result = lift_gamma_gain(
          color, base_lift, color_lift, base_gamma, color_gamma, base_gain, color_gain);
      break;
    case CMP_NODE_COLOR_BALANCE_ASC_CDL:
      result = offset_power_slope(
          color, base_offset, color_offset, base_power, color_power, base_slope, color_slope);
      break;
    case CMP_NODE_COLOR_BALANCE_WHITEPOINT:
      result = white_point_variable(color,
                                    input_temperature,
                                    input_tint,
                                    output_temperature,
                                    output_tint,
                                    scene_to_xyz,
                                    xyz_to_scene);
      break;
  }

  return float4(math::interpolate(color.xyz(), result.xyz(), math::min(factor, 1.0f)), color.w);
}

using blender::compositor::Color;

class ColorBalanceFunction : public mf::MultiFunction {
 public:
  ColorBalanceFunction()
  {
    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"Color Balance", signature};
      builder.single_input<Color>("Color");
      builder.single_input<float>("Factor");
      builder.single_input<MenuValue>("Type");

      builder.single_input<float>("Base Lift");
      builder.single_input<Color>("Color Lift");
      builder.single_input<float>("Base Gamma");
      builder.single_input<Color>("Color Gamma");
      builder.single_input<float>("Base Gain");
      builder.single_input<Color>("Color Gain");

      builder.single_input<float>("Base Offset");
      builder.single_input<Color>("Color Offset");
      builder.single_input<float>("Base Power");
      builder.single_input<Color>("Color Power");
      builder.single_input<float>("Base Slope");
      builder.single_input<Color>("Color Slope");

      builder.single_input<float>("Input Temperature");
      builder.single_input<float>("Input Tint");
      builder.single_input<float>("Output Temperature");
      builder.single_input<float>("Output Tint");

      builder.single_output<Color>("Result");
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<Color> color_array = params.readonly_single_input<Color>(0, "Color");
    const VArray<float> factor_array = params.readonly_single_input<float>(1, "Factor");
    const VArray<MenuValue> type_array = params.readonly_single_input<MenuValue>(2, "Type");

    const VArray<float> base_lift_array = params.readonly_single_input<float>(3, "Base Lift");
    const VArray<Color> color_lift_array = params.readonly_single_input<Color>(4, "Color Lift");
    const VArray<float> base_gamma_array = params.readonly_single_input<float>(5, "Base Gamma");
    const VArray<Color> color_gamma_array = params.readonly_single_input<Color>(6, "Color Gamma");
    const VArray<float> base_gain_array = params.readonly_single_input<float>(7, "Base Gain");
    const VArray<Color> color_gain_array = params.readonly_single_input<Color>(8, "Color Gain");

    const VArray<float> base_offset_array = params.readonly_single_input<float>(9, "Base Offset");
    const VArray<Color> color_offset_array = params.readonly_single_input<Color>(10,
                                                                                 "Color Offset");
    const VArray<float> base_power_array = params.readonly_single_input<float>(11, "Base Power");
    const VArray<Color> color_power_array = params.readonly_single_input<Color>(12, "Color Power");
    const VArray<float> base_slope_array = params.readonly_single_input<float>(13, "Base Slope");
    const VArray<Color> color_slope_array = params.readonly_single_input<Color>(14, "Color Slope");

    const VArray<float> input_temperature_array = params.readonly_single_input<float>(
        15, "Input Temperature");
    const VArray<float> input_tint_array = params.readonly_single_input<float>(16, "Input Tint");
    const VArray<float> output_temperature_array = params.readonly_single_input<float>(
        17, "Output Temperature");
    const VArray<float> output_tint_array = params.readonly_single_input<float>(18, "Output Tint");

    MutableSpan<Color> result = params.uninitialized_single_output<Color>(19, "Result");

    const std::optional<MenuValue> type_single = type_array.get_if_single();
    const std::optional<float> input_temperature_single = input_temperature_array.get_if_single();
    const std::optional<float> input_tint_single = input_tint_array.get_if_single();
    const std::optional<float> output_temperature_single =
        output_temperature_array.get_if_single();
    const std::optional<float> output_tint_single = output_tint_array.get_if_single();

    const bool is_white_point = type_single.has_value() &&
                                type_single.value().value == CMP_NODE_COLOR_BALANCE_WHITEPOINT;

    /* As an optimization for white point balancing, if all inputs are single, compute the white
     * point matrix outside of the loop. */
    if (is_white_point && input_temperature_single.has_value() && input_tint_single.has_value() &&
        output_temperature_single.has_value() && output_tint_single.has_value())
    {
      const float3x3 white_point_matrix = get_white_point_matrix(input_temperature_single.value(),
                                                                 input_tint_single.value(),
                                                                 output_temperature_single.value(),
                                                                 output_tint_single.value());
      mask.foreach_index([&](const int64_t i) {
        result[i] = Color(
            white_point_constant(float4(color_array[i]), factor_array[i], white_point_matrix));
      });
    }
    else {
      const float3x3 scene_to_xyz = IMB_colormanagement_get_scene_linear_to_xyz();
      const float3x3 xyz_to_scene = IMB_colormanagement_get_xyz_to_scene_linear();

      bool all_but_color_single_value = true;
      for (int i = 0; i < 19; i++) {
        if (i == 1) {
          continue;
        }
        if (!params.readonly_single_input(i).is_single()) {
          all_but_color_single_value = false;
          break;
        }
      }

      if (all_but_color_single_value) {
        const float factor = factor_array.get_internal_single();
        const float type = CMPNodeColorBalanceMethod(type_array.get_internal_single().value);
        const float base_lift = base_lift_array.get_internal_single();
        const float4 color_lift = float4(color_lift_array.get_internal_single());
        const float base_gamma = base_gamma_array.get_internal_single();
        const float4 color_gamma = float4(color_gamma_array.get_internal_single());
        const float base_gain = base_gain_array.get_internal_single();
        const float4 color_gain = float4(color_gain_array.get_internal_single());
        const float base_offset = base_offset_array.get_internal_single();
        const float4 color_offset = float4(color_offset_array.get_internal_single());
        const float base_power = base_power_array.get_internal_single();
        const float4 color_power = float4(color_power_array.get_internal_single());
        const float base_slope = base_slope_array.get_internal_single();
        const float4 color_slope = float4(color_slope_array.get_internal_single());
        const float input_temperature = input_temperature_array.get_internal_single();
        const float input_tint = input_tint_array.get_internal_single();
        const float output_temperature = output_temperature_array.get_internal_single();
        const float output_tint = output_tint_array.get_internal_single();

        mask.foreach_index([&](const int64_t i) {
          result[i] = Color(color_balance(float4(color_array[i]),
                                          factor,
                                          CMPNodeColorBalanceMethod(type),
                                          base_lift,
                                          color_lift,
                                          base_gamma,
                                          color_gamma,
                                          base_gain,
                                          color_gain,
                                          base_offset,
                                          color_offset,
                                          base_power,
                                          color_power,
                                          base_slope,
                                          color_slope,
                                          input_temperature,
                                          input_tint,
                                          output_temperature,
                                          output_tint,
                                          scene_to_xyz,
                                          xyz_to_scene));
        });
      }
      else {
        mask.foreach_index([&](const int64_t i) {
          result[i] = Color(color_balance(float4(color_array[i]),
                                          factor_array[i],
                                          CMPNodeColorBalanceMethod(type_array[i].value),
                                          base_lift_array[i],
                                          float4(color_lift_array[i]),
                                          base_gamma_array[i],
                                          float4(color_gamma_array[i]),
                                          base_gain_array[i],
                                          float4(color_gain_array[i]),
                                          base_offset_array[i],
                                          float4(color_offset_array[i]),
                                          base_power_array[i],
                                          float4(color_power_array[i]),
                                          base_slope_array[i],
                                          float4(color_slope_array[i]),
                                          input_temperature_array[i],
                                          input_tint_array[i],
                                          output_temperature_array[i],
                                          output_tint_array[i],
                                          scene_to_xyz,
                                          xyz_to_scene));
        });
      }
    }
  }
};

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  const static ColorBalanceFunction function;
  builder.set_matching_fn(function);
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
  ntype.gpu_fn = file_ns::node_gpu_material;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_colorbalance)
