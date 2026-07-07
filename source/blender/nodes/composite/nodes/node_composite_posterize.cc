/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "GPU_material.hh"

#include "COM_result.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_posterize_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.is_function_node();
  b.add_input<decl::Color>("Image").default_value({1.0f, 1.0f, 1.0f, 1.0f}).hide_value();
  b.add_output<decl::Color>("Image").align_with_previous();

  b.add_input<decl::Float>("Steps").default_value(8.0f).min(2.0f).max(1024.0f);
}

using namespace blender::compositor;

static int node_gpu_material(GPUMaterial *material,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *inputs,
                             GPUNodeStack *outputs)
{
  return GPU_stack_link(material, node, "node_composite_posterize", inputs, outputs);
}

using compositor::Color;

static float4 posterize(const float4 &color, const float steps)
{
  const float sanitized_steps = math::clamp(steps, 2.0f, 1024.0f);
  return float4(math::floor(color.xyz() * sanitized_steps) / sanitized_steps, color.w);
}

static void node_build_multi_function(nodes::NodeMultiFunctionBuilder &builder)
{
  static auto function = mf::build::SI2_SO<Color, float, Color>(
      "Posterize",
      [](const Color &color, const float steps) -> Color {
        return Color(posterize(float4(color), steps));
      },
      mf::build::exec_presets::SomeSpanOrSingle<0>());
  builder.set_matching_fn(function);
}

static void node_register()
{
  static bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodePosterize", CMP_NODE_POSTERIZE);
  ntype.ui_name = "Posterize";
  ntype.ui_description =
      "Reduce number of colors in an image, converting smooth gradients into sharp transitions";
  ntype.enum_name_legacy = "POSTERIZE";
  ntype.nclass = NODE_CLASS_OP_COLOR;
  ntype.declare = node_declare;
  ntype.gpu_fn = node_gpu_material;
  ntype.build_multi_function = node_build_multi_function;

  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_composite_posterize_cc
