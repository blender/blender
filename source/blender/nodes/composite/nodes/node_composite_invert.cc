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

#include "UI_resources.hh"

#include "GPU_material.hh"

#include "node_composite_util.hh"

/* **************** INVERT ******************** */

namespace blender::nodes::node_composite_invert_cc {

static void cmp_node_invert_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Float>("Fac")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .compositor_domain_priority(1);
  b.add_input<decl::Color>("Color")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Bool>("Invert Color").default_value(true).compositor_domain_priority(2);
  b.add_input<decl::Bool>("Invert Alpha").default_value(false).compositor_domain_priority(3);

  b.add_output<decl::Color>("Color");
}

using namespace blender::compositor;

static int node_gpu_material(GPUMaterial *material,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *inputs,
                             GPUNodeStack *outputs)
{
  return GPU_stack_link(material, node, "node_composite_invert", inputs, outputs);
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static auto function = mf::build::SI4_SO<float, float4, bool, bool, float4>(
      "Invert Color",
      [](const float factor, const float4 &color, const bool invert_color, const bool invert_alpha)
          -> float4 {
        float4 result = color;
        if (invert_color) {
          result = float4(1.0f - result.xyz(), result.w);
        }
        if (invert_alpha) {
          result = float4(result.xyz(), 1.0f - result.w);
        }
        return math::interpolate(color, result, factor);
      },
      mf::build::exec_presets::SomeSpanOrSingle<1>());
  builder.set_matching_fn(function);
}

}  // namespace blender::nodes::node_composite_invert_cc

static void register_node_type_cmp_invert()
{
  namespace file_ns = blender::nodes::node_composite_invert_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeInvert", CMP_NODE_INVERT);
  ntype.ui_name = "Invert Color";
  ntype.ui_description = "Invert colors, producing a negative";
  ntype.enum_name_legacy = "INVERT";
  ntype.nclass = NODE_CLASS_OP_COLOR;
  ntype.declare = file_ns::cmp_node_invert_declare;
  ntype.gpu_fn = file_ns::node_gpu_material;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_invert)
