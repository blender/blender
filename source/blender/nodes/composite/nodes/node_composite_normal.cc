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

#include "GPU_material.hh"

#include "COM_shader_node.hh"

#include "node_composite_util.hh"

/* **************** NORMAL  ******************** */

namespace blender::nodes::node_composite_normal_cc {

static void cmp_node_normal_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>("Normal")
      .default_value({0.0f, 0.0f, 1.0f})
      .min(-1.0f)
      .max(1.0f)
      .subtype(PROP_DIRECTION)
      .compositor_domain_priority(0);
  b.add_output<decl::Vector>("Normal")
      .default_value({0.0f, 0.0f, 1.0f})
      .min(-1.0f)
      .max(1.0f)
      .subtype(PROP_DIRECTION);
  b.add_output<decl::Float>("Dot");
}

using namespace blender::realtime_compositor;

/* The vector value is stored in the default value of the output socket. */
static float3 get_vector_value(const bNode &node)
{
  const bNodeSocket &normal_output = node.output_by_identifier("Normal");
  const float3 node_normal = normal_output.default_value_typed<bNodeSocketValueVector>()->value;
  return math::normalize(node_normal);
}

class NormalShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    GPU_stack_link(material,
                   &bnode(),
                   "node_composite_normal",
                   inputs,
                   outputs,
                   GPU_uniform(get_vector_value(bnode())));
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new NormalShaderNode(node);
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  const float3 normalized_node_normal = get_vector_value(builder.node());

  builder.construct_and_set_matching_fn_cb([=]() {
    return mf::build::SI1_SO2<float4, float4, float>(
        "Normal And Dot",
        [=](const float4 &normal, float4 &output_normal, float &dot) -> void {
          output_normal = float4(normalized_node_normal, 0.0f);
          dot = -math::dot(normal.xyz(), normalized_node_normal);
        },
        mf::build::exec_presets::AllSpanOrSingle());
  });
}

}  // namespace blender::nodes::node_composite_normal_cc

void register_node_type_cmp_normal()
{
  namespace file_ns = blender::nodes::node_composite_normal_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_NORMAL, "Normal", NODE_CLASS_OP_VECTOR);
  ntype.declare = file_ns::cmp_node_normal_declare;
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(&ntype);
}
