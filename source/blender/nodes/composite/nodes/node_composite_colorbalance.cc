/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.hh"
#include "BLI_math_color.h"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_material.hh"

#include "COM_shader_node.hh"

#include "IMB_colormanagement.hh"

#include "BLI_math_color.hh"

#include "node_composite_util.hh"

/* ******************* Color Balance ********************************* */

/* Sync functions update formula parameters for other modes, such that the result is comparable.
 * Note that the results are not exactly the same due to differences in color handling
 * (sRGB conversion happens for LGG),
 * but this keeps settings comparable. */

void ntreeCompositColorBalanceSyncFromLGG(bNodeTree * /*ntree*/, bNode *node)
{
  NodeColorBalance *n = (NodeColorBalance *)node->storage;

  for (int c = 0; c < 3; c++) {
    n->slope[c] = (2.0f - n->lift[c]) * n->gain[c];
    n->offset[c] = (n->lift[c] - 1.0f) * n->gain[c];
    n->power[c] = (n->gamma[c] != 0.0f) ? 1.0f / n->gamma[c] : 1000000.0f;
  }
}

void ntreeCompositColorBalanceSyncFromCDL(bNodeTree * /*ntree*/, bNode *node)
{
  NodeColorBalance *n = (NodeColorBalance *)node->storage;

  for (int c = 0; c < 3; c++) {
    float d = n->slope[c] + n->offset[c];
    n->lift[c] = (d != 0.0f ? n->slope[c] + 2.0f * n->offset[c] / d : 0.0f);
    n->gain[c] = d;
    n->gamma[c] = (n->power[c] != 0.0f) ? 1.0f / n->power[c] : 1000000.0f;
  }
}

namespace blender::nodes::node_composite_colorbalance_cc {

NODE_STORAGE_FUNCS(NodeColorBalance)

static void cmp_node_colorbalance_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Fac")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .compositor_domain_priority(1);
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_output<decl::Color>("Image");
}

static void node_composit_init_colorbalance(bNodeTree * /*ntree*/, bNode *node)
{
  NodeColorBalance *n = MEM_cnew<NodeColorBalance>(__func__);

  n->lift[0] = n->lift[1] = n->lift[2] = 1.0f;
  n->gamma[0] = n->gamma[1] = n->gamma[2] = 1.0f;
  n->gain[0] = n->gain[1] = n->gain[2] = 1.0f;

  n->slope[0] = n->slope[1] = n->slope[2] = 1.0f;
  n->offset[0] = n->offset[1] = n->offset[2] = 0.0f;
  n->power[0] = n->power[1] = n->power[2] = 1.0f;

  n->input_temperature = n->output_temperature = 6500.0f;
  n->input_tint = n->output_tint = 10.0f;
  node->storage = n;
}

static void node_composit_buts_colorbalance(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *split, *col, *row;

  uiItemR(layout, ptr, "correction_method", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

  const int method = RNA_enum_get(ptr, "correction_method");

  if (method == CMP_NODE_COLOR_BALANCE_LGG) {
    split = uiLayoutSplit(layout, 0.0f, false);
    col = uiLayoutColumn(split, false);
    uiTemplateColorPicker(col, ptr, "lift", true, true, false, true);
    row = uiLayoutRow(col, false);
    uiItemR(row, ptr, "lift", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

    col = uiLayoutColumn(split, false);
    uiTemplateColorPicker(col, ptr, "gamma", true, true, true, true);
    row = uiLayoutRow(col, false);
    uiItemR(row, ptr, "gamma", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

    col = uiLayoutColumn(split, false);
    uiTemplateColorPicker(col, ptr, "gain", true, true, true, true);
    row = uiLayoutRow(col, false);
    uiItemR(row, ptr, "gain", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  }
  else if (method == CMP_NODE_COLOR_BALANCE_ASC_CDL) {
    split = uiLayoutSplit(layout, 0.0f, false);
    col = uiLayoutColumn(split, false);
    uiTemplateColorPicker(col, ptr, "offset", true, true, false, true);
    row = uiLayoutRow(col, false);
    uiItemR(row, ptr, "offset", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
    uiItemR(col, ptr, "offset_basis", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

    col = uiLayoutColumn(split, false);
    uiTemplateColorPicker(col, ptr, "power", true, true, false, true);
    row = uiLayoutRow(col, false);
    uiItemR(row, ptr, "power", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

    col = uiLayoutColumn(split, false);
    uiTemplateColorPicker(col, ptr, "slope", true, true, false, true);
    row = uiLayoutRow(col, false);
    uiItemR(row, ptr, "slope", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  }
  else if (method == CMP_NODE_COLOR_BALANCE_WHITEPOINT) {
    split = uiLayoutSplit(layout, 0.0f, false);

    col = uiLayoutColumn(split, false);
    row = uiLayoutRow(col, true);
    uiItemL(row, IFACE_("Input"), ICON_NONE);
    uiTemplateCryptoPicker(row, ptr, "input_whitepoint", ICON_EYEDROPPER);
    uiItemR(col,
            ptr,
            "input_temperature",
            UI_ITEM_R_SPLIT_EMPTY_NAME,
            IFACE_("Temperature"),
            ICON_NONE);
    uiItemR(col, ptr, "input_tint", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Tint"), ICON_NONE);

    col = uiLayoutColumn(split, false);
    row = uiLayoutRow(col, true);
    uiItemL(row, IFACE_("Output"), ICON_NONE);
    uiTemplateCryptoPicker(row, ptr, "output_whitepoint", ICON_EYEDROPPER);
    uiItemR(col,
            ptr,
            "output_temperature",
            UI_ITEM_R_SPLIT_EMPTY_NAME,
            IFACE_("Temperature"),
            ICON_NONE);
    uiItemR(col, ptr, "output_tint", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Tint"), ICON_NONE);
  }
  else {
    BLI_assert(false);
  }
}

static void node_composit_buts_colorbalance_ex(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "correction_method", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

  const int method = RNA_enum_get(ptr, "correction_method");

  if (method == CMP_NODE_COLOR_BALANCE_LGG) {
    uiTemplateColorPicker(layout, ptr, "lift", true, true, false, true);
    uiItemR(layout, ptr, "lift", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

    uiTemplateColorPicker(layout, ptr, "gamma", true, true, true, true);
    uiItemR(layout, ptr, "gamma", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

    uiTemplateColorPicker(layout, ptr, "gain", true, true, true, true);
    uiItemR(layout, ptr, "gain", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  }
  else if (method == CMP_NODE_COLOR_BALANCE_ASC_CDL) {
    uiTemplateColorPicker(layout, ptr, "offset", true, true, false, true);
    uiItemR(layout, ptr, "offset", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

    uiTemplateColorPicker(layout, ptr, "power", true, true, false, true);
    uiItemR(layout, ptr, "power", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

    uiTemplateColorPicker(layout, ptr, "slope", true, true, false, true);
    uiItemR(layout, ptr, "slope", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  }
  else if (method == CMP_NODE_COLOR_BALANCE_WHITEPOINT) {
    uiItemR(layout, ptr, "input_temperature", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
    uiItemR(layout, ptr, "input_tint", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
    uiItemR(layout, ptr, "output_temperature", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
    uiItemR(layout, ptr, "output_tint", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  }
  else {
    BLI_assert(false);
  }
}

using namespace blender::realtime_compositor;

static CMPNodeColorBalanceMethod get_color_balance_method(const bNode &node)
{
  return static_cast<CMPNodeColorBalanceMethod>(node.custom1);
}

static float3 get_sanitized_gamma(const float3 gamma)
{
  return float3(gamma.x == 0.0f ? 1e-6f : gamma.x,
                gamma.y == 0.0f ? 1e-6f : gamma.y,
                gamma.z == 0.0f ? 1e-6f : gamma.z);
}

static float3x3 get_white_point_matrix(const bNode &node)
{
  const NodeColorBalance &node_color_balance = node_storage(node);
  const float3x3 scene_to_xyz = IMB_colormanagement_get_scene_linear_to_xyz();
  const float3x3 xyz_to_scene = IMB_colormanagement_get_xyz_to_scene_linear();
  const float3 input = blender::math::whitepoint_from_temp_tint(
      node_color_balance.input_temperature, node_color_balance.input_tint);
  const float3 output = blender::math::whitepoint_from_temp_tint(
      node_color_balance.output_temperature, node_color_balance.output_tint);
  const float3x3 adaption = blender::math::chromatic_adaption_matrix(input, output);
  return xyz_to_scene * adaption * scene_to_xyz;
}

class ColorBalanceShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    const NodeColorBalance &node_color_balance = node_storage(bnode());

    if (get_color_balance_method(bnode()) == CMP_NODE_COLOR_BALANCE_LGG) {
      const float3 lift = node_color_balance.lift;
      const float3 gamma = node_color_balance.gamma;
      const float3 gain = node_color_balance.gain;
      const float3 sanitized_gamma = get_sanitized_gamma(gamma);

      GPU_stack_link(material,
                     &bnode(),
                     "node_composite_color_balance_lgg",
                     inputs,
                     outputs,
                     GPU_uniform(lift),
                     GPU_uniform(sanitized_gamma),
                     GPU_uniform(gain));
    }
    else if (get_color_balance_method(bnode()) == CMP_NODE_COLOR_BALANCE_ASC_CDL) {
      const float3 offset = node_color_balance.offset;
      const float3 power = node_color_balance.power;
      const float3 slope = node_color_balance.slope;
      const float3 full_offset = node_color_balance.offset_basis + offset;

      GPU_stack_link(material,
                     &bnode(),
                     "node_composite_color_balance_asc_cdl",
                     inputs,
                     outputs,
                     GPU_uniform(full_offset),
                     GPU_uniform(power),
                     GPU_uniform(slope));
    }
    else if (get_color_balance_method(bnode()) == CMP_NODE_COLOR_BALANCE_WHITEPOINT) {
      const float3x3 matrix = get_white_point_matrix(bnode());
      GPU_stack_link(material,
                     &bnode(),
                     "node_composite_color_balance_whitepoint",
                     inputs,
                     outputs,
                     GPU_uniform(blender::float4x4(matrix).base_ptr()));
    }
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new ColorBalanceShaderNode(node);
}

static float4 color_balance_lgg(const float factor,
                                const float4 &color,
                                const float3 &lift,
                                const float3 &gamma,
                                const float3 &gain)
{
  float3 inverse_lift = 2.0f - lift;
  float3 srgb_color;
  linearrgb_to_srgb_v3_v3(srgb_color, color);
  float3 lift_balanced = ((srgb_color - 1.0f) * inverse_lift) + 1.0f;

  float3 gain_balanced = lift_balanced * gain;
  gain_balanced = math::max(gain_balanced, float3(0.0f));

  float3 linear_color;
  srgb_to_linearrgb_v3_v3(linear_color, gain_balanced);
  float3 gamma_balanced = math::pow(linear_color, 1.0f / gamma);

  return float4(math::interpolate(color.xyz(), gamma_balanced, math::min(factor, 1.0f)), color.w);
}

static float4 color_balance_asc_cdl(const float factor,
                                    const float4 &color,
                                    const float3 &offset,
                                    const float3 &power,
                                    const float3 &slope)
{
  float3 balanced = color.xyz() * slope + offset;
  balanced = math::pow(math::max(balanced, float3(0.0f)), power);
  return float4(math::interpolate(color.xyz(), balanced, math::min(factor, 1.0f)), color.w);
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  const NodeColorBalance &node_color_balance = node_storage(builder.node());

  switch (get_color_balance_method(builder.node())) {
    case CMP_NODE_COLOR_BALANCE_LGG: {
      const float3 lift = node_color_balance.lift;
      const float3 gamma = node_color_balance.gamma;
      const float3 gain = node_color_balance.gain;
      const float3 sanitized_gamma = get_sanitized_gamma(gamma);

      builder.construct_and_set_matching_fn_cb([=]() {
        return mf::build::SI2_SO<float, float4, float4>(
            "Color Balance LGG",
            [=](const float factor, const float4 &color) -> float4 {
              return color_balance_lgg(factor, color, lift, sanitized_gamma, gain);
            },
            mf::build::exec_presets::SomeSpanOrSingle<1>());
      });
      break;
    }
    case CMP_NODE_COLOR_BALANCE_ASC_CDL: {
      const float3 offset = node_color_balance.offset;
      const float3 power = node_color_balance.power;
      const float3 slope = node_color_balance.slope;
      const float3 full_offset = node_color_balance.offset_basis + offset;

      builder.construct_and_set_matching_fn_cb([=]() {
        return mf::build::SI2_SO<float, float4, float4>(
            "Color Balance ASC CDL",
            [=](const float factor, const float4 &color) -> float4 {
              return color_balance_asc_cdl(factor, color, full_offset, power, slope);
            },
            mf::build::exec_presets::SomeSpanOrSingle<1>());
      });
      break;
    }
    case CMP_NODE_COLOR_BALANCE_WHITEPOINT: {
      const float4x4 matrix = float4x4(get_white_point_matrix(builder.node()));
      builder.construct_and_set_matching_fn_cb([=]() {
        return mf::build::SI2_SO<float, float4, float4>(
            "Color Balance White Point",
            [=](const float factor, const float4 &color) -> float4 {
              return math::interpolate(color, matrix * color, math::min(factor, 1.0f));
            },
            mf::build::exec_presets::SomeSpanOrSingle<1>());
      });
      break;
    }
  }
}

}  // namespace blender::nodes::node_composite_colorbalance_cc

void register_node_type_cmp_colorbalance()
{
  namespace file_ns = blender::nodes::node_composite_colorbalance_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_COLORBALANCE, "Color Balance", NODE_CLASS_OP_COLOR);
  ntype.declare = file_ns::cmp_node_colorbalance_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_colorbalance;
  ntype.draw_buttons_ex = file_ns::node_composit_buts_colorbalance_ex;
  blender::bke::node_type_size(&ntype, 400, 200, 400);
  ntype.initfunc = file_ns::node_composit_init_colorbalance;
  blender::bke::node_type_storage(
      &ntype, "NodeColorBalance", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(&ntype);
}
