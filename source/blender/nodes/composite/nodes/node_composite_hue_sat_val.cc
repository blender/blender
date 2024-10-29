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

#include "COM_shader_node.hh"

#include "node_composite_util.hh"

/* **************** Hue/Saturation/Value ******************** */

namespace blender::nodes::node_composite_hue_sat_val_cc {

static void cmp_node_huesatval_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Float>("Hue")
      .default_value(0.5f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .compositor_domain_priority(1);
  b.add_input<decl::Float>("Saturation")
      .default_value(1.0f)
      .min(0.0f)
      .max(2.0f)
      .subtype(PROP_FACTOR)
      .compositor_domain_priority(2);
  b.add_input<decl::Float>("Value")
      .default_value(1.0f)
      .min(0.0f)
      .max(2.0f)
      .subtype(PROP_FACTOR)
      .translation_context(BLT_I18NCONTEXT_COLOR)
      .compositor_domain_priority(3);
  b.add_input<decl::Float>("Fac")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .compositor_domain_priority(4);
  b.add_output<decl::Color>("Image");
}

using namespace blender::realtime_compositor;

class HueSaturationValueShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    GPU_stack_link(material, &bnode(), "node_composite_hue_saturation_value", inputs, outputs);
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new HueSaturationValueShaderNode(node);
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static auto function = mf::build::SI5_SO<float4, float, float, float, float, float4>(
      "Hue Saturation Value",
      [](const float4 &color,
         const float hue,
         const float saturation,
         const float value,
         const float factor) -> float4 {
        float3 hsv;
        rgb_to_hsv_v(color, hsv);

        hsv.x = math::fract(hsv.x + hue + 0.5f);
        hsv.y = hsv.y * saturation;
        hsv.z = hsv.z * value;

        float3 rgb_result;
        hsv_to_rgb_v(hsv, rgb_result);
        rgb_result = math::max(rgb_result, float3(0.0f));

        return float4(math::interpolate(color.xyz(), rgb_result, factor), color.w);
      },
      mf::build::exec_presets::SomeSpanOrSingle<0>());
  builder.set_matching_fn(function);
}

}  // namespace blender::nodes::node_composite_hue_sat_val_cc

void register_node_type_cmp_hue_sat()
{
  namespace file_ns = blender::nodes::node_composite_hue_sat_val_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_HUE_SAT, "Hue/Saturation/Value", NODE_CLASS_OP_COLOR);
  ntype.declare = file_ns::cmp_node_huesatval_declare;
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(&ntype);
}
