/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "GPU_material.hh"

#include "COM_shader_node.hh"

#include "node_composite_util.hh"

/* **************** Exposure ******************** */

namespace blender::nodes::node_composite_exposure_cc {

static void cmp_node_exposure_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Float>("Exposure").min(-10.0f).max(10.0f).compositor_domain_priority(1);
  b.add_output<decl::Color>("Image");
}

using namespace blender::realtime_compositor;

class ExposureShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    GPU_stack_link(material, &bnode(), "node_composite_exposure", inputs, outputs);
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new ExposureShaderNode(node);
}

}  // namespace blender::nodes::node_composite_exposure_cc

void register_node_type_cmp_exposure()
{
  namespace file_ns = blender::nodes::node_composite_exposure_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_EXPOSURE, "Exposure", NODE_CLASS_OP_COLOR);
  ntype.declare = file_ns::cmp_node_exposure_declare;
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;

  nodeRegisterType(&ntype);
}
