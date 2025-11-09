/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "UI_resources.hh"

#include "GPU_material.hh"

#include "COM_result.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_alpha_over_cc {

static const EnumPropertyItem type_items[] = {
    {CMP_NODE_ALPHA_OVER_OPERATION_TYPE_OVER,
     "OVER",
     ICON_NONE,
     N_("Over"),
     N_("The foreground goes over the background according to the alpha of the foreground")},
    {CMP_NODE_ALPHA_OVER_OPERATION_TYPE_DISJOINT_OVER,
     "DISJOINT_OVER",
     ICON_NONE,
     N_("Disjoint Over"),
     N_("The foreground goes over the background according to the alpha of the foreground while "
        "assuming the background is being held out by the foreground")},
    {CMP_NODE_ALPHA_OVER_OPERATION_TYPE_CONJOINT_OVER,
     "CONJOINT_OVER",
     ICON_NONE,
     N_("Conjoint Over"),
     N_("The foreground goes over the background according to the alpha of the foreground but the "
        "foreground completely covers the background if it is more opaque")},
    {0, nullptr, 0, nullptr, nullptr},
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Color>("Background").default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Color>("Foreground").default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>("Factor", "Fac")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Menu>("Type")
      .default_value(CMP_NODE_ALPHA_OVER_OPERATION_TYPE_OVER)
      .static_items(type_items)
      .optional_label();
  b.add_input<decl::Bool>("Straight Alpha")
      .default_value(false)
      .description(
          "Defines whether the foreground is in straight alpha form, which is necessary to know "
          "for proper alpha compositing. Images in the compositor are in premultiplied alpha form "
          "by default, so this should be false in most cases. But if, and only if, the foreground "
          "was converted to straight alpha form for some reason, this should be set to true");
  b.add_output<decl::Color>("Image");
}

static int node_gpu_material(GPUMaterial *material,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *inputs,
                             GPUNodeStack *outputs)
{
  return GPU_stack_link(material, node, "node_composite_alpha_over", inputs, outputs);
}

/* If straight_alpha is true, then the foreground is in straight alpha form and would need to be
 * premultiplied. */
static float4 preprocess_foreground(const float4 &foreground, const bool straight_alpha)
{
  const float alpha = math::clamp(foreground.w, 0.0f, 1.0f);
  const float4 premultiplied_foreground = float4(foreground.xyz() * alpha, alpha);
  return straight_alpha ? premultiplied_foreground : foreground;
}

/* Computes the Porter and Duff Over compositing operation. */
static float4 alpha_over(const float4 &background,
                         const float4 &foreground,
                         const float factor,
                         const bool straight_alpha)
{
  const float4 foreground_color = preprocess_foreground(foreground, straight_alpha);

  const float foreground_alpha = math::clamp(foreground.w, 0.0f, 1.0f);
  const float4 mix_result = foreground_color + background * (1.0f - foreground_alpha);

  return math::interpolate(background, mix_result, factor);
}

/* Computes the Porter and Duff Over compositing operation while assuming the background is being
 * held out by the foreground. See for reference:
 *
 *   https://benmcewan.com/blog/disjoint-over-and-conjoint-over-explained */
static float4 alpha_over_disjoint(const float4 &background,
                                  const float4 &foreground,
                                  const float factor,
                                  const bool straight_alpha)
{
  const float4 foreground_color = preprocess_foreground(foreground, straight_alpha);

  const float foreground_alpha = math::clamp(foreground.w, 0.0f, 1.0f);
  const float background_alpha = math::clamp(background.w, 0.0f, 1.0f);

  if (foreground_alpha + background_alpha < 1.0f) {
    const float4 mix_result = foreground_color + background;
    return math::interpolate(background, mix_result, factor);
  }

  const float4 straight_background = math::safe_divide(background, background_alpha);
  const float4 mix_result = foreground_color + straight_background * (1.0f - foreground_alpha);

  return math::interpolate(background, mix_result, factor);
}

/* Computes the Porter and Duff Over compositing operation but the foreground completely covers the
 * background if it is more opaque but not necessary completely opaque. See for reference:
 *
 *   https://benmcewan.com/blog/disjoint-over-and-conjoint-over-explained */
static float4 alpha_over_conjoint(const float4 &background,
                                  const float4 &foreground,
                                  const float factor,
                                  const bool straight_alpha)
{
  const float4 foreground_color = preprocess_foreground(foreground, straight_alpha);

  const float foreground_alpha = math::clamp(foreground.w, 0.0f, 1.0f);
  const float background_alpha = math::clamp(background.w, 0.0f, 1.0f);

  if (foreground_alpha > background_alpha) {
    const float4 mix_result = foreground_color;
    return math::interpolate(background, mix_result, factor);
  }

  const float alpha_ratio = math::safe_divide(foreground_alpha, background_alpha);
  const float4 mix_result = foreground_color + background * (1.0f - alpha_ratio);

  return math::interpolate(background, mix_result, factor);
}

using blender::compositor::Color;

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static auto function = mf::build::SI5_SO<Color, Color, float, MenuValue, bool, Color>(
      "Alpha Over",
      [=](const Color &background,
          const Color &foreground,
          const float factor,
          const MenuValue type,
          const bool straight_alpha) -> Color {
        switch (CMPNodeAlphaOverOperationType(type.value)) {
          case CMP_NODE_ALPHA_OVER_OPERATION_TYPE_OVER:
            return Color(
                alpha_over(float4(background), float4(foreground), factor, straight_alpha));
          case CMP_NODE_ALPHA_OVER_OPERATION_TYPE_DISJOINT_OVER:
            return Color(alpha_over_disjoint(
                float4(background), float4(foreground), factor, straight_alpha));
          case CMP_NODE_ALPHA_OVER_OPERATION_TYPE_CONJOINT_OVER:
            return Color(alpha_over_conjoint(
                float4(background), float4(foreground), factor, straight_alpha));
        }
        return background;
      },
      mf::build::exec_presets::SomeSpanOrSingle<0, 1>());

  builder.set_matching_fn(function);
}

static void register_node_type_cmp_alphaover()
{
  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeAlphaOver", CMP_NODE_ALPHAOVER);
  ntype.ui_name = "Alpha Over";
  ntype.ui_description = "Overlay a foreground image onto a background image";
  ntype.enum_name_legacy = "ALPHAOVER";
  ntype.nclass = NODE_CLASS_OP_COLOR;
  ntype.declare = node_declare;
  ntype.gpu_fn = node_gpu_material;
  ntype.build_multi_function = node_build_multi_function;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_alphaover)

}  // namespace blender::nodes::node_composite_alpha_over_cc
