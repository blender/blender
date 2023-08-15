/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "GPU_material.h"

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

}  // namespace blender::nodes::node_composite_separate_xyz_cc

void register_node_type_cmp_separate_xyz()
{
  namespace file_ns = blender::nodes::node_composite_separate_xyz_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_SEPARATE_XYZ, "Separate XYZ", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::cmp_node_separate_xyz_declare;
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;

  nodeRegisterType(&ntype);
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

}  // namespace blender::nodes::node_composite_combine_xyz_cc

void register_node_type_cmp_combine_xyz()
{
  namespace file_ns = blender::nodes::node_composite_combine_xyz_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_COMBINE_XYZ, "Combine XYZ", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::cmp_node_combine_xyz_declare;
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;

  nodeRegisterType(&ntype);
}
