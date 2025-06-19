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
  b.add_input<decl::Bool>("Straight Alpha")
      .default_value(false)
      .description(
          "Defines whether the foreground is in straight alpha form, which is necessary to know "
          "for proper alpha compositing. Images in the compositor are in premultiplied alpha form "
          "by default, so this should be false in most cases. But if, and only if, the foreground "
          "was converted to straight alpha form for some reason, this should be set to true");
  b.add_output<decl::Color>("Image");
}

using namespace blender::compositor;

static int node_gpu_material(GPUMaterial *material,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *inputs,
                             GPUNodeStack *outputs)
{
  return GPU_stack_link(material, node, "node_composite_alpha_over", inputs, outputs);
}

/* Computes the Porter and Duff Over compositing operation. If straight_alpha is true, then the
 * foreground is in straight alpha form and would need to be premultiplied. */
static float4 alpha_over(const float factor,
                         const float4 &background,
                         const float4 &foreground,
                         const bool straight_alpha)
{
  /* Premultiply the alpha of the foreground if it is straight. */
  const float alpha = math::clamp(foreground.w, 0.0f, 1.0f);
  const float4 premultiplied_foreground = float4(foreground.xyz() * alpha, alpha);
  const float4 foreground_color = straight_alpha ? premultiplied_foreground : foreground;

  const float4 mix_result = background * (1.0f - alpha) + foreground_color;
  return math::interpolate(background, mix_result, factor);
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static auto function = mf::build::SI4_SO<float, float4, float4, bool, float4>(
      "Alpha Over",
      [=](const float factor,
          const float4 &background,
          const float4 &foreground,
          const bool straight_alpha) -> float4 {
        return alpha_over(factor, background, foreground, straight_alpha);
      },
      mf::build::exec_presets::SomeSpanOrSingle<1, 2>());
  builder.set_matching_fn(function);
}

}  // namespace blender::nodes::node_composite_alpha_over_cc

static void register_node_type_cmp_alphaover()
{
  namespace file_ns = blender::nodes::node_composite_alpha_over_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeAlphaOver", CMP_NODE_ALPHAOVER);
  ntype.ui_name = "Alpha Over";
  ntype.ui_description = "Overlay a foreground image onto a background image";
  ntype.enum_name_legacy = "ALPHAOVER";
  ntype.nclass = NODE_CLASS_OP_COLOR;
  ntype.declare = file_ns::cmp_node_alphaover_declare;
  ntype.gpu_fn = file_ns::node_gpu_material;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_alphaover)
