/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.hh"
#include "BLI_math_color.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "GPU_material.hh"

#include "COM_result.hh"

#include "node_composite_util.hh"

/* **************** Hue/Saturation/Value ******************** */

namespace blender::nodes::node_composite_hue_sat_val_cc {

static void cmp_node_huesatval_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.is_function_node();
  b.add_input<decl::Color>("Image").default_value({1.0f, 1.0f, 1.0f, 1.0f}).hide_value();
  b.add_output<decl::Color>("Image").align_with_previous();

  b.add_input<decl::Float>("Hue").default_value(0.5f).min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_input<decl::Float>("Saturation")
      .default_value(1.0f)
      .min(0.0f)
      .max(2.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>("Value")
      .default_value(1.0f)
      .min(0.0f)
      .max(2.0f)
      .subtype(PROP_FACTOR)
      .translation_context(BLT_I18NCONTEXT_COLOR);
  b.add_input<decl::Float>("Factor", "Fac")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
}

using namespace blender::compositor;

static int node_gpu_material(GPUMaterial *material,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *inputs,
                             GPUNodeStack *outputs)
{
  return GPU_stack_link(material, node, "node_composite_hue_saturation_value", inputs, outputs);
}

static float4 hue_saturation_value(const float4 &color,
                                   const float hue,
                                   const float saturation,
                                   const float value,
                                   const float factor)
{
  float3 hsv;
  rgb_to_hsv_v(color, hsv);

  hsv.x = math::fract(hsv.x + hue + 0.5f);
  hsv.y = hsv.y * saturation;
  hsv.z = hsv.z * value;

  float3 rgb_result;
  hsv_to_rgb_v(hsv, rgb_result);
  rgb_result = math::max(rgb_result, float3(0.0f));

  return float4(math::interpolate(color.xyz(), rgb_result, factor), color.w);
}

using blender::compositor::Color;

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static auto function = mf::build::SI5_SO<Color, float, float, float, float, Color>(
      "Hue Saturation Value",
      [](const Color &color,
         const float hue,
         const float saturation,
         const float value,
         const float factor) -> Color {
        return Color(hue_saturation_value(float4(color), hue, saturation, value, factor));
      },
      mf::build::exec_presets::SomeSpanOrSingle<0>());
  builder.set_matching_fn(function);
}

}  // namespace blender::nodes::node_composite_hue_sat_val_cc

static void register_node_type_cmp_hue_sat()
{
  namespace file_ns = blender::nodes::node_composite_hue_sat_val_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeHueSat", CMP_NODE_HUE_SAT);
  ntype.ui_name = "Hue/Saturation/Value";
  ntype.ui_description = "Apply a color transformation in the HSV color model";
  ntype.enum_name_legacy = "HUE_SAT";
  ntype.nclass = NODE_CLASS_OP_COLOR;
  ntype.declare = file_ns::cmp_node_huesatval_declare;
  ntype.gpu_fn = file_ns::node_gpu_material;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_hue_sat)
