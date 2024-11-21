/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "GPU_material.hh"

#include "COM_shader_node.hh"

#include "node_composite_util.hh"

/* **************** SEPARATE XYZ ******************** */

namespace blender::nodes::node_composite_separate_xyz_cc {

static void cmp_node_separate_xyz_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>("Vector").min(-10000.0f).max(10000.0f);
  b.add_output<decl::Float>("X");
  b.add_output<decl::Float>("Y");
  b.add_output<decl::Float>("Z");
}

using namespace blender::realtime_compositor;

class SeparateXYZShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    GPU_stack_link(material, &bnode(), "node_composite_separate_xyz", inputs, outputs);
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new SeparateXYZShaderNode(node);
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static auto function = mf::build::SI1_SO3<float4, float, float, float>(
      "Separate XYZ",
      [](const float4 &vector, float &x, float &y, float &z) -> void {
        x = vector.x;
        y = vector.y;
        z = vector.z;
      },
      mf::build::exec_presets::AllSpanOrSingle());
  builder.set_matching_fn(function);
}

}  // namespace blender::nodes::node_composite_separate_xyz_cc

void register_node_type_cmp_separate_xyz()
{
  namespace file_ns = blender::nodes::node_composite_separate_xyz_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_SEPARATE_XYZ, "Separate XYZ", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::cmp_node_separate_xyz_declare;
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(&ntype);
}

/* **************** COMBINE XYZ ******************** */

namespace blender::nodes::node_composite_combine_xyz_cc {

static void cmp_node_combine_xyz_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("X").min(-10000.0f).max(10000.0f);
  b.add_input<decl::Float>("Y").min(-10000.0f).max(10000.0f);
  b.add_input<decl::Float>("Z").min(-10000.0f).max(10000.0f);
  b.add_output<decl::Vector>("Vector");
}

using namespace blender::realtime_compositor;

class CombineXYZShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    GPU_stack_link(material, &bnode(), "node_composite_combine_xyz", inputs, outputs);
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new CombineXYZShaderNode(node);
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static auto function = mf::build::SI3_SO<float, float, float, float4>(
      "Combine XYZ",
      [](const float x, const float y, const float z) -> float4 { return float4(x, y, z, 0.0f); },
      mf::build::exec_presets::Materialized());
  builder.set_matching_fn(function);
}

}  // namespace blender::nodes::node_composite_combine_xyz_cc

void register_node_type_cmp_combine_xyz()
{
  namespace file_ns = blender::nodes::node_composite_combine_xyz_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_COMBINE_XYZ, "Combine XYZ", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::cmp_node_combine_xyz_declare;
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(&ntype);
}
