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

#include "COM_result.hh"

#include "node_composite_util.hh"

/* **************** INVERT ******************** */

namespace blender::nodes::node_composite_invert_cc {

static void cmp_node_invert_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.is_function_node();
  b.add_input<decl::Color>("Color").default_value({1.0f, 1.0f, 1.0f, 1.0f}).hide_value();
  b.add_output<decl::Color>("Color").align_with_previous();

  b.add_input<decl::Float>("Factor", "Fac")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Bool>("Invert Color").default_value(true);
  b.add_input<decl::Bool>("Invert Alpha").default_value(false);
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

static float4 invert(const float4 &color,
                     const float factor,
                     const bool invert_color,
                     const bool invert_alpha)
{
  float4 result = color;
  if (invert_color) {
    result = float4(1.0f - result.xyz(), result.w);
  }
  if (invert_alpha) {
    result = float4(result.xyz(), 1.0f - result.w);
  }
  return math::interpolate(color, result, factor);
}

using blender::compositor::Color;

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static auto function = mf::build::SI4_SO<Color, float, bool, bool, Color>(
      "Invert Color",
      [](const Color &color, const float factor, const bool invert_color, const bool invert_alpha)
          -> Color { return Color(invert(float4(color), factor, invert_color, invert_alpha)); },
      mf::build::exec_presets::SomeSpanOrSingle<0>());
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
