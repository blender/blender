/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "IMB_colormanagement.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_material.hh"

#include "COM_shader_node.hh"

#include "node_composite_util.hh"

/* ******************* Color Correction ********************************* */

namespace blender::nodes::node_composite_colorcorrection_cc {

NODE_STORAGE_FUNCS(NodeColorCorrection)

static void cmp_node_colorcorrection_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Float>("Mask")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .compositor_domain_priority(1);
  b.add_output<decl::Color>("Image");
}

static void node_composit_init_colorcorrection(bNodeTree * /*ntree*/, bNode *node)
{
  NodeColorCorrection *n = MEM_cnew<NodeColorCorrection>(__func__);
  n->startmidtones = 0.2f;
  n->endmidtones = 0.7f;
  n->master.contrast = 1.0f;
  n->master.gain = 1.0f;
  n->master.gamma = 1.0f;
  n->master.lift = 0.0f;
  n->master.saturation = 1.0f;
  n->midtones.contrast = 1.0f;
  n->midtones.gain = 1.0f;
  n->midtones.gamma = 1.0f;
  n->midtones.lift = 0.0f;
  n->midtones.saturation = 1.0f;
  n->shadows.contrast = 1.0f;
  n->shadows.gain = 1.0f;
  n->shadows.gamma = 1.0f;
  n->shadows.lift = 0.0f;
  n->shadows.saturation = 1.0f;
  n->highlights.contrast = 1.0f;
  n->highlights.gain = 1.0f;
  n->highlights.gamma = 1.0f;
  n->highlights.lift = 0.0f;
  n->highlights.saturation = 1.0f;
  node->custom1 = 7;  // red + green + blue enabled
  node->storage = n;
}

static void node_composit_buts_colorcorrection(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *row;

  row = uiLayoutRow(layout, false);
  uiItemR(row, ptr, "red", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(row, ptr, "green", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(row, ptr, "blue", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

  row = uiLayoutRow(layout, false);
  uiItemL(row, "", ICON_NONE);
  uiItemL(row, IFACE_("Saturation"), ICON_NONE);
  uiItemL(row, IFACE_("Contrast"), ICON_NONE);
  uiItemL(row, IFACE_("Gamma"), ICON_NONE);
  uiItemL(row, IFACE_("Gain"), ICON_NONE);
  uiItemL(row, IFACE_("Lift"), ICON_NONE);

  row = uiLayoutRow(layout, false);
  uiItemL(row, IFACE_("Master"), ICON_NONE);
  uiItemR(
      row, ptr, "master_saturation", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, "", ICON_NONE);
  uiItemR(
      row, ptr, "master_contrast", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, "", ICON_NONE);
  uiItemR(row, ptr, "master_gamma", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, "", ICON_NONE);
  uiItemR(row, ptr, "master_gain", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, "", ICON_NONE);
  uiItemR(row, ptr, "master_lift", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, "", ICON_NONE);

  row = uiLayoutRow(layout, false);
  uiItemL(row, IFACE_("Highlights"), ICON_NONE);
  uiItemR(row,
          ptr,
          "highlights_saturation",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
          "",
          ICON_NONE);
  uiItemR(row,
          ptr,
          "highlights_contrast",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
          "",
          ICON_NONE);
  uiItemR(
      row, ptr, "highlights_gamma", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, "", ICON_NONE);
  uiItemR(
      row, ptr, "highlights_gain", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, "", ICON_NONE);
  uiItemR(
      row, ptr, "highlights_lift", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, "", ICON_NONE);

  row = uiLayoutRow(layout, false);
  uiItemL(row, IFACE_("Midtones"), ICON_NONE);
  uiItemR(row,
          ptr,
          "midtones_saturation",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
          "",
          ICON_NONE);
  uiItemR(
      row, ptr, "midtones_contrast", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, "", ICON_NONE);
  uiItemR(
      row, ptr, "midtones_gamma", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, "", ICON_NONE);
  uiItemR(row, ptr, "midtones_gain", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, "", ICON_NONE);
  uiItemR(row, ptr, "midtones_lift", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, "", ICON_NONE);

  row = uiLayoutRow(layout, false);
  uiItemL(row, IFACE_("Shadows"), ICON_NONE);
  uiItemR(row,
          ptr,
          "shadows_saturation",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
          "",
          ICON_NONE);
  uiItemR(
      row, ptr, "shadows_contrast", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, "", ICON_NONE);
  uiItemR(row, ptr, "shadows_gamma", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, "", ICON_NONE);
  uiItemR(row, ptr, "shadows_gain", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, "", ICON_NONE);
  uiItemR(row, ptr, "shadows_lift", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, "", ICON_NONE);

  row = uiLayoutRow(layout, false);
  uiItemR(row,
          ptr,
          "midtones_start",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
          nullptr,
          ICON_NONE);
  uiItemR(
      row, ptr, "midtones_end", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
}

static void node_composit_buts_colorcorrection_ex(uiLayout *layout,
                                                  bContext * /*C*/,
                                                  PointerRNA *ptr)
{
  uiLayout *row;

  row = uiLayoutRow(layout, false);
  uiItemR(row, ptr, "red", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(row, ptr, "green", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(row, ptr, "blue", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  row = layout;
  uiItemL(row, IFACE_("Saturation"), ICON_NONE);
  uiItemR(row,
          ptr,
          "master_saturation",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
          nullptr,
          ICON_NONE);
  uiItemR(row,
          ptr,
          "highlights_saturation",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
          nullptr,
          ICON_NONE);
  uiItemR(row,
          ptr,
          "midtones_saturation",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
          nullptr,
          ICON_NONE);
  uiItemR(row,
          ptr,
          "shadows_saturation",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
          nullptr,
          ICON_NONE);

  uiItemL(row, IFACE_("Contrast"), ICON_NONE);
  uiItemR(row,
          ptr,
          "master_contrast",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
          nullptr,
          ICON_NONE);
  uiItemR(row,
          ptr,
          "highlights_contrast",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
          nullptr,
          ICON_NONE);
  uiItemR(row,
          ptr,
          "midtones_contrast",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
          nullptr,
          ICON_NONE);
  uiItemR(row,
          ptr,
          "shadows_contrast",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
          nullptr,
          ICON_NONE);

  uiItemL(row, IFACE_("Gamma"), ICON_NONE);
  uiItemR(
      row, ptr, "master_gamma", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
  uiItemR(row,
          ptr,
          "highlights_gamma",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
          nullptr,
          ICON_NONE);
  uiItemR(row,
          ptr,
          "midtones_gamma",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
          nullptr,
          ICON_NONE);
  uiItemR(row,
          ptr,
          "shadows_gamma",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
          nullptr,
          ICON_NONE);

  uiItemL(row, IFACE_("Gain"), ICON_NONE);
  uiItemR(
      row, ptr, "master_gain", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
  uiItemR(row,
          ptr,
          "highlights_gain",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
          nullptr,
          ICON_NONE);
  uiItemR(row,
          ptr,
          "midtones_gain",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
          nullptr,
          ICON_NONE);
  uiItemR(
      row, ptr, "shadows_gain", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);

  uiItemL(row, IFACE_("Lift"), ICON_NONE);
  uiItemR(
      row, ptr, "master_lift", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
  uiItemR(row,
          ptr,
          "highlights_lift",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
          nullptr,
          ICON_NONE);
  uiItemR(row,
          ptr,
          "midtones_lift",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
          nullptr,
          ICON_NONE);
  uiItemR(
      row, ptr, "shadows_lift", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);

  row = uiLayoutRow(layout, false);
  uiItemR(row, ptr, "midtones_start", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(row, ptr, "midtones_end", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class ColorCorrectionShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    float enabled_channels[3];
    get_enabled_channels(enabled_channels);
    float luminance_coefficients[3];
    IMB_colormanagement_get_luminance_coefficients(luminance_coefficients);

    const NodeColorCorrection &node_color_correction = node_storage(bnode());

    GPU_stack_link(material,
                   &bnode(),
                   "node_composite_color_correction",
                   inputs,
                   outputs,
                   GPU_constant(enabled_channels),
                   GPU_uniform(&node_color_correction.startmidtones),
                   GPU_uniform(&node_color_correction.endmidtones),
                   GPU_uniform(&node_color_correction.master.saturation),
                   GPU_uniform(&node_color_correction.master.contrast),
                   GPU_uniform(&node_color_correction.master.gamma),
                   GPU_uniform(&node_color_correction.master.gain),
                   GPU_uniform(&node_color_correction.master.lift),
                   GPU_uniform(&node_color_correction.shadows.saturation),
                   GPU_uniform(&node_color_correction.shadows.contrast),
                   GPU_uniform(&node_color_correction.shadows.gamma),
                   GPU_uniform(&node_color_correction.shadows.gain),
                   GPU_uniform(&node_color_correction.shadows.lift),
                   GPU_uniform(&node_color_correction.midtones.saturation),
                   GPU_uniform(&node_color_correction.midtones.contrast),
                   GPU_uniform(&node_color_correction.midtones.gamma),
                   GPU_uniform(&node_color_correction.midtones.gain),
                   GPU_uniform(&node_color_correction.midtones.lift),
                   GPU_uniform(&node_color_correction.highlights.saturation),
                   GPU_uniform(&node_color_correction.highlights.contrast),
                   GPU_uniform(&node_color_correction.highlights.gamma),
                   GPU_uniform(&node_color_correction.highlights.gain),
                   GPU_uniform(&node_color_correction.highlights.lift),
                   GPU_constant(luminance_coefficients));
  }

  void get_enabled_channels(float enabled_channels[3])
  {
    for (int i = 0; i < 3; i++) {
      enabled_channels[i] = (bnode().custom1 & (1 << i)) ? 1.0f : 0.0f;
    }
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new ColorCorrectionShaderNode(node);
}

static float4 color_correction(const float4 &color,
                               const float mask,
                               const int16_t enabled_channels,
                               const float start_midtones,
                               const float end_midtones,
                               const float master_saturation,
                               const float master_contrast,
                               const float master_gamma,
                               const float master_gain,
                               const float master_lift,
                               const float shadows_saturation,
                               const float shadows_contrast,
                               const float shadows_gamma,
                               const float shadows_gain,
                               const float shadows_lift,
                               const float midtones_saturation,
                               const float midtones_contrast,
                               const float midtones_gamma,
                               const float midtones_gain,
                               const float midtones_lift,
                               const float highlights_saturation,
                               const float highlights_contrast,
                               const float highlights_gamma,
                               const float highlights_gain,
                               const float highlights_lift,
                               const float3 &luminance_coefficients)
{
  const float margin = 0.10f;
  const float margin_divider = 0.5f / margin;
  float level = (color.x + color.y + color.z) / 3.0f;
  float level_shadows = 0.0f;
  float level_midtones = 0.0f;
  float level_highlights = 0.0f;
  if (level < (start_midtones - margin)) {
    level_shadows = 1.0f;
  }
  else if (level < (start_midtones + margin)) {
    level_midtones = ((level - start_midtones) * margin_divider) + 0.5f;
    level_shadows = 1.0f - level_midtones;
  }
  else if (level < (end_midtones - margin)) {
    level_midtones = 1.0f;
  }
  else if (level < (end_midtones + margin)) {
    level_highlights = ((level - end_midtones) * margin_divider) + 0.5f;
    level_midtones = 1.0f - level_highlights;
  }
  else {
    level_highlights = 1.0f;
  }

  float contrast = level_shadows * shadows_contrast;
  contrast += level_midtones * midtones_contrast;
  contrast += level_highlights * highlights_contrast;
  contrast *= master_contrast;
  float saturation = level_shadows * shadows_saturation;
  saturation += level_midtones * midtones_saturation;
  saturation += level_highlights * highlights_saturation;
  saturation *= master_saturation;
  float gamma = level_shadows * shadows_gamma;
  gamma += level_midtones * midtones_gamma;
  gamma += level_highlights * highlights_gamma;
  gamma *= master_gamma;
  float gain = level_shadows * shadows_gain;
  gain += level_midtones * midtones_gain;
  gain += level_highlights * highlights_gain;
  gain *= master_gain;
  float lift = level_shadows * shadows_lift;
  lift += level_midtones * midtones_lift;
  lift += level_highlights * highlights_lift;
  lift += master_lift;

  float inverse_gamma = 1.0f / gamma;
  float luma = math::dot(color.xyz(), luminance_coefficients);

  float3 corrected = luma + saturation * (color.xyz() - luma);
  corrected = 0.5f + (corrected - 0.5f) * contrast;
  corrected = math::fallback_pow(corrected * gain + lift, inverse_gamma, corrected);
  corrected = math::interpolate(color.xyz(), corrected, math::min(mask, 1.0f));

  return float4(enabled_channels & (1 << 0) ? corrected.x : color.x,
                enabled_channels & (1 << 1) ? corrected.y : color.y,
                enabled_channels & (1 << 2) ? corrected.z : color.z,
                color.w);
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  const NodeColorCorrection &node_color_correction = node_storage(builder.node());

  const int16_t enabled_channels = builder.node().custom1;
  float3 luminance_coefficients;
  IMB_colormanagement_get_luminance_coefficients(luminance_coefficients);

  builder.construct_and_set_matching_fn_cb([=]() {
    return mf::build::SI2_SO<float4, float, float4>(
        "Color Correction",
        [=](const float4 &color, const float mask) -> float4 {
          return color_correction(color,
                                  mask,
                                  enabled_channels,
                                  node_color_correction.startmidtones,
                                  node_color_correction.endmidtones,
                                  node_color_correction.master.saturation,
                                  node_color_correction.master.contrast,
                                  node_color_correction.master.gamma,
                                  node_color_correction.master.gain,
                                  node_color_correction.master.lift,
                                  node_color_correction.shadows.saturation,
                                  node_color_correction.shadows.contrast,
                                  node_color_correction.shadows.gamma,
                                  node_color_correction.shadows.gain,
                                  node_color_correction.shadows.lift,
                                  node_color_correction.midtones.saturation,
                                  node_color_correction.midtones.contrast,
                                  node_color_correction.midtones.gamma,
                                  node_color_correction.midtones.gain,
                                  node_color_correction.midtones.lift,
                                  node_color_correction.highlights.saturation,
                                  node_color_correction.highlights.contrast,
                                  node_color_correction.highlights.gamma,
                                  node_color_correction.highlights.gain,
                                  node_color_correction.highlights.lift,
                                  luminance_coefficients);
        },
        mf::build::exec_presets::SomeSpanOrSingle<0>());
  });
}

}  // namespace blender::nodes::node_composite_colorcorrection_cc

void register_node_type_cmp_colorcorrection()
{
  namespace file_ns = blender::nodes::node_composite_colorcorrection_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_COLORCORRECTION, "Color Correction", NODE_CLASS_OP_COLOR);
  ntype.declare = file_ns::cmp_node_colorcorrection_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_colorcorrection;
  ntype.draw_buttons_ex = file_ns::node_composit_buts_colorcorrection_ex;
  blender::bke::node_type_size(&ntype, 400, 200, 600);
  ntype.initfunc = file_ns::node_composit_init_colorcorrection;
  blender::bke::node_type_storage(
      &ntype, "NodeColorCorrection", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(&ntype);
}
