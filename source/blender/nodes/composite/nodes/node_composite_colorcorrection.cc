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

#include "UI_resources.hh"

#include "GPU_material.hh"

#include "COM_result.hh"

#include "node_composite_util.hh"

/* ******************* Color Correction ********************************* */

namespace blender::nodes::node_composite_colorcorrection_cc {

static void cmp_node_colorcorrection_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_input<decl::Color>("Image").default_value({1.0f, 1.0f, 1.0f, 1.0f}).hide_value();
  b.add_output<decl::Color>("Image").align_with_previous();

  b.add_input<decl::Float>("Mask").default_value(1.0f).min(0.0f).max(1.0f);

  PanelDeclarationBuilder &master_panel = b.add_panel("Master").default_closed(true);
  master_panel.add_input<decl::Float>("Saturation", "Master Saturation")
      .default_value(1.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(4.0f)
      .description("Controls the saturation of the entire image");
  master_panel.add_input<decl::Float>("Contrast", "Master Contrast")
      .default_value(1.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(4.0f)
      .description("Controls the contrast of the entire image");
  master_panel.add_input<decl::Float>("Gamma", "Master Gamma")
      .default_value(1.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(4.0f)
      .description("Controls the gamma of the entire image");
  master_panel.add_input<decl::Float>("Gain", "Master Gain")
      .default_value(1.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(4.0f)
      .description("Controls the gain of the entire image");
  master_panel.add_input<decl::Float>("Offset", "Master Offset")
      .default_value(0.0f)
      .subtype(PROP_FACTOR)
      .min(-1.0f)
      .max(1.0f)
      .description("Controls the offset of the entire image");

  PanelDeclarationBuilder &highlights_panel = b.add_panel("Highlights").default_closed(true);
  highlights_panel.add_input<decl::Float>("Saturation", "Highlights Saturation")
      .default_value(1.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(4.0f)
      .description("Controls the saturation of the highlights");
  highlights_panel.add_input<decl::Float>("Contrast", "Highlights Contrast")
      .default_value(1.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(4.0f)
      .description("Controls the contrast of the highlights");
  highlights_panel.add_input<decl::Float>("Gamma", "Highlights Gamma")
      .default_value(1.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(4.0f)
      .description("Controls the gamma of the highlights");
  highlights_panel.add_input<decl::Float>("Gain", "Highlights Gain")
      .default_value(1.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(4.0f)
      .description("Controls the gain of the highlights");
  highlights_panel.add_input<decl::Float>("Offset", "Highlights Offset")
      .default_value(0.0f)
      .subtype(PROP_FACTOR)
      .min(-1.0f)
      .max(1.0f)
      .description("Controls the offset of the highlights");

  PanelDeclarationBuilder &midtones_panel = b.add_panel("Midtones").default_closed(true);
  midtones_panel.add_input<decl::Float>("Saturation", "Midtones Saturation")
      .default_value(1.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(4.0f)
      .description("Controls the saturation of the midtones");
  midtones_panel.add_input<decl::Float>("Contrast", "Midtones Contrast")
      .default_value(1.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(4.0f)
      .description("Controls the contrast of the midtones");
  midtones_panel.add_input<decl::Float>("Gamma", "Midtones Gamma")
      .default_value(1.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(4.0f)
      .description("Controls the gamma of the midtones");
  midtones_panel.add_input<decl::Float>("Gain", "Midtones Gain")
      .default_value(1.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(4.0f)
      .description("Controls the gain of the midtones");
  midtones_panel.add_input<decl::Float>("Offset", "Midtones Offset")
      .default_value(0.0f)
      .subtype(PROP_FACTOR)
      .min(-1.0f)
      .max(1.0f)
      .description("Controls the offset of the midtones");

  PanelDeclarationBuilder &shadows_panel = b.add_panel("Shadows").default_closed(true);
  shadows_panel.add_input<decl::Float>("Saturation", "Shadows Saturation")
      .default_value(1.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(4.0f)
      .description("Controls the saturation of the shadows");
  shadows_panel.add_input<decl::Float>("Contrast", "Shadows Contrast")
      .default_value(1.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(4.0f)
      .description("Controls the contrast of the shadows");
  shadows_panel.add_input<decl::Float>("Gamma", "Shadows Gamma")
      .default_value(1.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(4.0f)
      .description("Controls the gamma of the shadows");
  shadows_panel.add_input<decl::Float>("Gain", "Shadows Gain")
      .default_value(1.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(4.0f)
      .description("Controls the gain of the shadows");
  shadows_panel.add_input<decl::Float>("Offset", "Shadows Offset")
      .default_value(0.0f)
      .subtype(PROP_FACTOR)
      .min(-1.0f)
      .max(1.0f)
      .description("Controls the offset of the shadows");

  PanelDeclarationBuilder &tonal_range_panel = b.add_panel("Tonal Range").default_closed(true);
  tonal_range_panel.add_input<decl::Float>("Midtones Start")
      .default_value(0.2f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(1.0f)
      .description(
          "Specifies the luminance at which the midtones of the image start and the shadows end");
  tonal_range_panel.add_input<decl::Float>("Midtones End")
      .default_value(0.7f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(1.0f)
      .description(
          "Specifies the luminance at which the midtones of the image end and the highlights "
          "start");

  PanelDeclarationBuilder &tone_range_panel = b.add_panel("Channels").default_closed(true);
  tone_range_panel.add_input<decl::Bool>("Red", "Apply On Red")
      .default_value(true)
      .description("If true, the correction will be applied on the red channel");
  tone_range_panel.add_input<decl::Bool>("Green", "Apply On Green")
      .default_value(true)
      .description("If true, the correction will be applied on the green channel");
  tone_range_panel.add_input<decl::Bool>("Blue", "Apply On Blue")
      .default_value(true)
      .description("If true, the correction will be applied on the blue channel");
}

using namespace blender::compositor;

static int node_gpu_material(GPUMaterial *material,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *inputs,
                             GPUNodeStack *outputs)
{
  float luminance_coefficients[3];
  IMB_colormanagement_get_luminance_coefficients(luminance_coefficients);

  return GPU_stack_link(material,
                        node,
                        "node_composite_color_correction",
                        inputs,
                        outputs,
                        GPU_constant(luminance_coefficients));
}

static float4 color_correction(const float4 &color,
                               const float &mask,
                               const float &master_saturation,
                               const float &master_contrast,
                               const float &master_gamma,
                               const float &master_gain,
                               const float &master_offset,
                               const float &highlights_saturation,
                               const float &highlights_contrast,
                               const float &highlights_gamma,
                               const float &highlights_gain,
                               const float &highlights_offset,
                               const float &midtones_saturation,
                               const float &midtones_contrast,
                               const float &midtones_gamma,
                               const float &midtones_gain,
                               const float &midtones_offset,
                               const float &shadows_saturation,
                               const float &shadows_contrast,
                               const float &shadows_gamma,
                               const float &shadows_gain,
                               const float &shadows_offset,
                               const float &start_midtones,
                               const float &end_midtones,
                               const bool &apply_on_red,
                               const bool &apply_on_green,
                               const bool &apply_on_blue,
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
  float offset = level_shadows * shadows_offset;
  offset += level_midtones * midtones_offset;
  offset += level_highlights * highlights_offset;
  offset += master_offset;

  float inverse_gamma = 1.0f / gamma;
  float luma = math::dot(color.xyz(), luminance_coefficients);

  float3 corrected = luma + saturation * (color.xyz() - luma);
  corrected = 0.5f + (corrected - 0.5f) * contrast;
  corrected = math::fallback_pow(corrected * gain + offset, inverse_gamma, corrected);
  corrected = math::interpolate(color.xyz(), corrected, math::min(mask, 1.0f));

  return float4(apply_on_red ? corrected.x : color.x,
                apply_on_green ? corrected.y : color.y,
                apply_on_blue ? corrected.z : color.z,
                color.w);
}

using blender::compositor::Color;

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  float3 luminance_coefficients;
  IMB_colormanagement_get_luminance_coefficients(luminance_coefficients);

  builder.construct_and_set_matching_fn_cb([=]() {
    return mf::build::detail::build_multi_function_with_n_inputs_one_output<Color>(
        "Color Correction",
        [=](const Color &color,
            const float &mask,
            const float &master_saturation,
            const float &master_contrast,
            const float &master_gamma,
            const float &master_gain,
            const float &master_offset,
            const float &highlights_saturation,
            const float &highlights_contrast,
            const float &highlights_gamma,
            const float &highlights_gain,
            const float &highlights_offset,
            const float &midtones_saturation,
            const float &midtones_contrast,
            const float &midtones_gamma,
            const float &midtones_gain,
            const float &midtones_offset,
            const float &shadows_saturation,
            const float &shadows_contrast,
            const float &shadows_gamma,
            const float &shadows_gain,
            const float &shadows_offset,
            const float &start_midtones,
            const float &end_midtones,
            const bool &apply_on_red,
            const bool &apply_on_green,
            const bool &apply_on_blue) -> Color {
          return Color(color_correction(float4(color),
                                        mask,
                                        master_saturation,
                                        master_contrast,
                                        master_gamma,
                                        master_gain,
                                        master_offset,
                                        highlights_saturation,
                                        highlights_contrast,
                                        highlights_gamma,
                                        highlights_gain,
                                        highlights_offset,
                                        midtones_saturation,
                                        midtones_contrast,
                                        midtones_gamma,
                                        midtones_gain,
                                        midtones_offset,
                                        shadows_saturation,
                                        shadows_contrast,
                                        shadows_gamma,
                                        shadows_gain,
                                        shadows_offset,
                                        start_midtones,
                                        end_midtones,
                                        apply_on_red,
                                        apply_on_green,
                                        apply_on_blue,
                                        luminance_coefficients));
        },
        mf::build::exec_presets::SomeSpanOrSingle<0>(),
        TypeSequence<Color,
                     float,
                     float,
                     float,
                     float,
                     float,
                     float,
                     float,
                     float,
                     float,
                     float,
                     float,
                     float,
                     float,
                     float,
                     float,
                     float,
                     float,
                     float,
                     float,
                     float,
                     float,
                     float,
                     float,
                     bool,
                     bool,
                     bool>());
  });
}

}  // namespace blender::nodes::node_composite_colorcorrection_cc

static void register_node_type_cmp_colorcorrection()
{
  namespace file_ns = blender::nodes::node_composite_colorcorrection_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeColorCorrection", CMP_NODE_COLORCORRECTION);
  ntype.ui_name = "Color Correction";
  ntype.ui_description =
      "Adjust the color of an image, separately in several tonal ranges (highlights, midtones and "
      "shadows)";
  ntype.enum_name_legacy = "COLORCORRECTION";
  ntype.nclass = NODE_CLASS_OP_COLOR;
  ntype.declare = file_ns::cmp_node_colorcorrection_declare;
  ntype.gpu_fn = file_ns::node_gpu_material;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_colorcorrection)
