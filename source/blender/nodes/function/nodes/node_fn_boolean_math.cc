/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "RNA_enum_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "NOD_socket_search_link.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_boolean_math_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Bool>("Boolean", "Boolean");
  b.add_input<decl::Bool>("Boolean", "Boolean_001");
  b.add_output<decl::Bool>("Boolean");
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "operation", 0, "", ICON_NONE);
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *sockB = (bNodeSocket *)BLI_findlink(&node->inputs, 1);

  bke::nodeSetSocketAvailability(ntree, sockB, !ELEM(node->custom1, NODE_BOOLEAN_MATH_NOT));
}

static void node_label(const bNodeTree * /*tree*/,
                       const bNode *node,
                       char *label,
                       int label_maxncpy)
{
  const char *name;
  bool enum_label = RNA_enum_name(rna_enum_node_boolean_math_items, node->custom1, &name);
  if (!enum_label) {
    name = "Unknown";
  }
  BLI_strncpy_utf8(label, IFACE_(name), label_maxncpy);
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  if (!params.node_tree().typeinfo->validate_link(
          static_cast<eNodeSocketDatatype>(params.other_socket().type), SOCK_BOOLEAN))
  {
    return;
  }

  for (const EnumPropertyItem *item = rna_enum_node_boolean_math_items;
       item->identifier != nullptr;
       item++)
  {
    if (item->name != nullptr && item->identifier[0] != '\0') {
      NodeBooleanMathOperation operation = static_cast<NodeBooleanMathOperation>(item->value);
      params.add_item(IFACE_(item->name), [operation](LinkSearchOpParams &params) {
        bNode &node = params.add_node("FunctionNodeBooleanMath");
        node.custom1 = operation;
        params.update_and_connect_available_socket(node, "Boolean");
      });
    }
  }
}

static const mf::MultiFunction *get_multi_function(const bNode &bnode)
{
  static auto exec_preset = mf::build::exec_presets::AllSpanOrSingle();
  static auto and_fn = mf::build::SI2_SO<bool, bool, bool>(
      "And", [](bool a, bool b) { return a && b; }, exec_preset);
  static auto or_fn = mf::build::SI2_SO<bool, bool, bool>(
      "Or", [](bool a, bool b) { return a || b; }, exec_preset);
  static auto not_fn = mf::build::SI1_SO<bool, bool>(
      "Not", [](bool a) { return !a; }, exec_preset);
  static auto nand_fn = mf::build::SI2_SO<bool, bool, bool>(
      "Not And", [](bool a, bool b) { return !(a && b); }, exec_preset);
  static auto nor_fn = mf::build::SI2_SO<bool, bool, bool>(
      "Nor", [](bool a, bool b) { return !(a || b); }, exec_preset);
  static auto xnor_fn = mf::build::SI2_SO<bool, bool, bool>(
      "Equal", [](bool a, bool b) { return a == b; }, exec_preset);
  static auto xor_fn = mf::build::SI2_SO<bool, bool, bool>(
      "Not Equal", [](bool a, bool b) { return a != b; }, exec_preset);
  static auto imply_fn = mf::build::SI2_SO<bool, bool, bool>(
      "Imply", [](bool a, bool b) { return !a || b; }, exec_preset);
  static auto nimply_fn = mf::build::SI2_SO<bool, bool, bool>(
      "Subtract", [](bool a, bool b) { return a && !b; }, exec_preset);

  switch (bnode.custom1) {
    case NODE_BOOLEAN_MATH_AND:
      return &and_fn;
    case NODE_BOOLEAN_MATH_OR:
      return &or_fn;
    case NODE_BOOLEAN_MATH_NOT:
      return &not_fn;
    case NODE_BOOLEAN_MATH_NAND:
      return &nand_fn;
    case NODE_BOOLEAN_MATH_NOR:
      return &nor_fn;
    case NODE_BOOLEAN_MATH_XNOR:
      return &xnor_fn;
    case NODE_BOOLEAN_MATH_XOR:
      return &xor_fn;
    case NODE_BOOLEAN_MATH_IMPLY:
      return &imply_fn;
    case NODE_BOOLEAN_MATH_NIMPLY:
      return &nimply_fn;
  }

  BLI_assert_unreachable();
  return nullptr;
}

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const mf::MultiFunction *fn = get_multi_function(builder.node());
  builder.set_matching_fn(fn);
}

}  // namespace blender::nodes::node_fn_boolean_math_cc

void register_node_type_fn_boolean_math()
{
  namespace file_ns = blender::nodes::node_fn_boolean_math_cc;

  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_BOOLEAN_MATH, "Boolean Math", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::node_declare;
  ntype.labelfunc = file_ns::node_label;
  ntype.updatefunc = file_ns::node_update;
  ntype.build_multi_function = file_ns::node_build_multi_function;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.gather_link_search_ops = file_ns::node_gather_link_searches;
  nodeRegisterType(&ntype);
}
