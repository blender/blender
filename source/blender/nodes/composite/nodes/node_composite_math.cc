/* SPDX-FileCopyrightText: 2006 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "GPU_material.h"

#include "COM_shader_node.hh"

#include "NOD_math_functions.hh"
#include "NOD_socket_search_link.hh"

#include "RNA_enum_types.h"

#include "node_composite_util.hh"

/* **************** SCALAR MATH ******************** */

namespace blender::nodes::node_composite_math_cc {

static void cmp_node_math_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Value")
      .default_value(0.5f)
      .min(-10000.0f)
      .max(10000.0f)
      .compositor_domain_priority(0);
  b.add_input<decl::Float>("Value", "Value_001")
      .default_value(0.5f)
      .min(-10000.0f)
      .max(10000.0f)
      .compositor_domain_priority(1);
  b.add_input<decl::Float>("Value", "Value_002")
      .default_value(0.5f)
      .min(-10000.0f)
      .max(10000.0f)
      .compositor_domain_priority(2);
  b.add_output<decl::Float>("Value");
}

class SocketSearchOp {
 public:
  std::string socket_name;
  NodeMathOperation mode = NODE_MATH_ADD;
  void operator()(LinkSearchOpParams &params)
  {
    bNode &node = params.add_node("CompositorNodeMath");
    node.custom1 = mode;
    params.update_and_connect_available_socket(node, socket_name);
  }
};

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const int weight = ELEM(params.other_socket().type, SOCK_FLOAT) ? 0 : -1;

  for (const EnumPropertyItem *item = rna_enum_node_math_items; item->identifier != nullptr;
       item++) {
    if (item->name != nullptr && item->identifier[0] != '\0') {
      params.add_item(CTX_IFACE_(BLT_I18NCONTEXT_ID_NODETREE, item->name),
                      SocketSearchOp{"Value", (NodeMathOperation)item->value},
                      weight);
    }
  }
}

using namespace blender::realtime_compositor;

class MathShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    GPU_stack_link(material, &bnode(), get_shader_function_name(), inputs, outputs);

    if (!get_should_clamp()) {
      return;
    }

    const float min = 0.0f;
    const float max = 1.0f;
    GPU_link(material,
             "clamp_value",
             get_output("Value").link,
             GPU_constant(&min),
             GPU_constant(&max),
             &get_output("Value").link);
  }

  NodeMathOperation get_operation()
  {
    return (NodeMathOperation)bnode().custom1;
  }

  const char *get_shader_function_name()
  {
    return get_float_math_operation_info(get_operation())->shader_name.c_str();
  }

  bool get_should_clamp()
  {
    return bnode().custom2 & SHD_MATH_CLAMP;
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new MathShaderNode(node);
}

}  // namespace blender::nodes::node_composite_math_cc

void register_node_type_cmp_math()
{
  namespace file_ns = blender::nodes::node_composite_math_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_MATH, "Math", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::cmp_node_math_declare;
  ntype.labelfunc = node_math_label;
  ntype.updatefunc = node_math_update;
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;
  ntype.gather_link_search_ops = file_ns::node_gather_link_searches;

  nodeRegisterType(&ntype);
}
