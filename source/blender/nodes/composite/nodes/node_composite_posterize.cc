/* SPDX-FileCopyrightText: 2021 Blender Authors
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

#include "GPU_material.hh"

#include "COM_shader_node.hh"

#include "node_composite_util.hh"

/* **************** Posterize ******************** */

namespace blender::nodes::node_composite_posterize_cc {

static void cmp_node_posterize_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Float>("Steps")
      .default_value(8.0f)
      .min(2.0f)
      .max(1024.0f)
      .compositor_domain_priority(1);
  b.add_output<decl::Color>("Image");
}

using namespace blender::compositor;

class PosterizeShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    GPU_stack_link(material, &bnode(), "node_composite_posterize", inputs, outputs);
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new PosterizeShaderNode(node);
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static auto function = mf::build::SI2_SO<float4, float, float4>(
      "Posterize",
      [](const float4 &color, const float steps) -> float4 {
        const float sanitized_steps = math::clamp(steps, 2.0f, 1024.0f);
        return float4(math::floor(color.xyz() * sanitized_steps) / sanitized_steps, color.w);
      },
      mf::build::exec_presets::SomeSpanOrSingle<0>());
  builder.set_matching_fn(function);
}

}  // namespace blender::nodes::node_composite_posterize_cc

void register_node_type_cmp_posterize()
{
  namespace file_ns = blender::nodes::node_composite_posterize_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodePosterize", CMP_NODE_POSTERIZE);
  ntype.ui_name = "Posterize";
  ntype.ui_description =
      "Reduce number of colors in an image, converting smooth gradients into sharp transitions";
  ntype.enum_name_legacy = "POSTERIZE";
  ntype.nclass = NODE_CLASS_OP_COLOR;
  ntype.declare = file_ns::cmp_node_posterize_declare;
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(&ntype);
}
