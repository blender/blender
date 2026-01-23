/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cmath>

#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "GPU_material.hh"

#include "COM_result.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_exposure_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.is_function_node();
  b.add_input<decl::Color>("Image").default_value({1.0f, 1.0f, 1.0f, 1.0f}).hide_value();
  b.add_output<decl::Color>("Image").align_with_previous();

  b.add_input<decl::Float>("Exposure").min(-10.0f).max(10.0f);
}

using namespace blender::compositor;

static int node_gpu_material(GPUMaterial *material,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *inputs,
                             GPUNodeStack *outputs)
{
  return GPU_stack_link(material, node, "node_composite_exposure", inputs, outputs);
}

static float4 adjust_exposure(const float4 &color, const float exposure)
{
  return float4(color.xyz() * std::exp2(exposure), color.w);
}

using compositor::Color;

static void node_build_multi_function(nodes::NodeMultiFunctionBuilder &builder)
{
  static auto function = mf::build::SI2_SO<Color, float, Color>(
      "Exposure",
      [](const Color &color, const float exposure) -> Color {
        return Color(adjust_exposure(float4(color), exposure));
      },
      mf::build::exec_presets::SomeSpanOrSingle<0>());
  builder.set_matching_fn(function);
}

static void node_register()
{
  static bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeExposure", CMP_NODE_EXPOSURE);
  ntype.ui_name = "Exposure";
  ntype.ui_description = "Adjust brightness using a camera exposure parameter";
  ntype.enum_name_legacy = "EXPOSURE";
  ntype.nclass = NODE_CLASS_OP_COLOR;
  ntype.declare = node_declare;
  ntype.gpu_fn = node_gpu_material;
  ntype.build_multi_function = node_build_multi_function;

  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_composite_exposure_cc
