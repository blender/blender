/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "NOD_math_functions.hh"
#include "NOD_socket_search_link.hh"

#include "RNA_enum_types.hh"

#include "GPU_material.hh"

#include "COM_utilities_gpu_material.hh"

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
       item++)
  {
    if (item->name != nullptr && item->identifier[0] != '\0') {
      params.add_item(CTX_IFACE_(BLT_I18NCONTEXT_ID_NODETREE, item->name),
                      SocketSearchOp{"Value", (NodeMathOperation)item->value},
                      weight);
    }
  }
}

using namespace blender::compositor;

static NodeMathOperation get_operation(const bNode &node)
{
  return static_cast<NodeMathOperation>(node.custom1);
}

static const char *get_shader_function_name(const bNode &node)
{
  return get_float_math_operation_info(get_operation(node))->shader_name.c_str();
}

static bool get_should_clamp(const bNode &node)
{
  return node.custom2 & SHD_MATH_CLAMP;
}

static int node_gpu_material(GPUMaterial *material,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *inputs,
                             GPUNodeStack *outputs)
{
  const bool is_valid = GPU_stack_link(
      material, node, get_shader_function_name(*node), inputs, outputs);

  if (!is_valid || !get_should_clamp(*node)) {
    return is_valid;
  }

  const float min = 0.0f;
  const float max = 1.0f;
  return GPU_link(material,
                  "clamp_value",
                  get_shader_node_output(*node, outputs, "Value").link,
                  GPU_constant(&min),
                  GPU_constant(&max),
                  &get_shader_node_output(*node, outputs, "Value").link);
}

}  // namespace blender::nodes::node_composite_math_cc

void register_node_type_cmp_math()
{
  namespace file_ns = blender::nodes::node_composite_math_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeMath", CMP_NODE_MATH);
  ntype.ui_name = "Math";
  ntype.ui_description = "Perform math operations";
  ntype.enum_name_legacy = "MATH";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = file_ns::cmp_node_math_declare;
  ntype.labelfunc = node_math_label;
  ntype.updatefunc = node_math_update;
  ntype.gpu_fn = file_ns::node_gpu_material;
  ntype.gather_link_search_ops = file_ns::node_gather_link_searches;
  ntype.build_multi_function = blender::nodes::node_math_build_multi_function;

  blender::bke::node_register_type(ntype);
}
