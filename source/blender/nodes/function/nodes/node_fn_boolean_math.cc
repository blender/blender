/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_listbase.h"
#include "BLI_string_utf8.h"

#include "FN_multi_function_registry.hh"

#include "RNA_enum_types.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "NOD_inverse_eval_params.hh"
#include "NOD_socket_search_link.hh"
#include "NOD_value_elem_eval.hh"

#include "NOD_rna_define.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_boolean_math_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Bool>("Boolean"_ustr, "Boolean"_ustr);

  const bNode *node = b.node_or_null();
  if (node != nullptr) {
    const auto type = NodeBooleanMathOperation(node->custom1);
    if (type != NODE_BOOLEAN_MATH_NOT) {
      b.add_input<decl::Bool>("Boolean"_ustr, "Boolean_001"_ustr);
    }
  }
  b.add_output<decl::Bool>("Boolean"_ustr);
}

static void node_layout(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.prop(ptr, "operation", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_label(const bNodeTree * /*tree*/,
                       const bNode *node,
                       char *label,
                       int label_maxncpy)
{
  const char *name;
  bool enum_label = RNA_enum_name(rna_enum_node_boolean_math_items, node->custom1, &name);
  if (!enum_label) {
    name = N_("Unknown");
  }
  BLI_strncpy_utf8(label, IFACE_(name), label_maxncpy);
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  if (!params.node_tree().typeinfo->validate_link(eNodeSocketDatatype(params.other_socket().type),
                                                  SOCK_BOOLEAN))
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
        bNode &node = params.add_node("FunctionNodeBooleanMath"_ustr);
        node.custom1 = operation;
        params.update_and_connect_available_socket(node, "Boolean"_ustr);
      });
    }
  }
}

static const mf::MultiFunction *get_multi_function(const bNode &bnode)
{
  switch (bnode.custom1) {
    case NODE_BOOLEAN_MATH_AND:
      return &fn::multi_function::registry::lookup("bool && bool"_ustr);
    case NODE_BOOLEAN_MATH_OR:
      return &fn::multi_function::registry::lookup("bool || bool"_ustr);
    case NODE_BOOLEAN_MATH_NOT:
      return &fn::multi_function::registry::lookup("!bool"_ustr);
    case NODE_BOOLEAN_MATH_NAND:
      return &fn::multi_function::registry::lookup("!(bool && bool)"_ustr);
    case NODE_BOOLEAN_MATH_NOR:
      return &fn::multi_function::registry::lookup("!(bool || bool)"_ustr);
    case NODE_BOOLEAN_MATH_XNOR:
      return &fn::multi_function::registry::lookup("bool == bool"_ustr);
    case NODE_BOOLEAN_MATH_XOR:
      return &fn::multi_function::registry::lookup("bool != bool"_ustr);
    case NODE_BOOLEAN_MATH_IMPLY:
      return &fn::multi_function::registry::lookup("!bool || bool"_ustr);
    case NODE_BOOLEAN_MATH_NIMPLY:
      return &fn::multi_function::registry::lookup("bool && !bool"_ustr);
  }

  BLI_assert_unreachable();
  return nullptr;
}

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const mf::MultiFunction *fn = get_multi_function(builder.node());
  builder.set_matching_fn(fn);
}

static void node_eval_elem(value_elem::ElemEvalParams &params)
{
  using namespace value_elem;
  const NodeBooleanMathOperation op = NodeBooleanMathOperation(params.node.custom1);
  switch (op) {
    case NODE_BOOLEAN_MATH_NOT: {
      params.set_output_elem("Boolean"_ustr, params.get_input_elem<BoolElem>("Boolean"_ustr));
      break;
    }
    default: {
      break;
    }
  }
}

static void node_eval_inverse_elem(value_elem::InverseElemEvalParams &params)
{
  using namespace value_elem;
  const NodeBooleanMathOperation op = NodeBooleanMathOperation(params.node.custom1);
  switch (op) {
    case NODE_BOOLEAN_MATH_NOT: {
      params.set_input_elem("Boolean"_ustr, params.get_output_elem<BoolElem>("Boolean"_ustr));
      break;
    }
    default: {
      break;
    }
  }
}

static void node_eval_inverse(inverse_eval::InverseEvalParams &params)
{
  const NodeBooleanMathOperation op = NodeBooleanMathOperation(params.node.custom1);
  const UString first_input_id = "Boolean"_ustr;
  const UString output_id = "Boolean"_ustr;
  switch (op) {
    case NODE_BOOLEAN_MATH_NOT: {
      params.set_input(first_input_id, !params.get_output<bool>(output_id));
      break;
    }
    default: {
      break;
    }
  }
}

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(srna,
                    "operation",
                    "Operation",
                    "",
                    rna_enum_node_boolean_math_items,
                    NOD_inline_enum_accessors(custom1));
}

static void node_register()
{
  static bke::bNodeType ntype;

  fn_node_type_base(&ntype, "FunctionNodeBooleanMath"_ustr, FN_NODE_BOOLEAN_MATH);
  ntype.ui_name = "Boolean Math";
  ntype.ui_description = "Perform a logical operation on the given boolean inputs";
  ntype.enum_name_legacy = "BOOLEAN_MATH";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.labelfunc = node_label;
  ntype.build_multi_function = node_build_multi_function;
  ntype.draw_buttons = node_layout;
  ntype.gather_link_search_ops = node_gather_link_searches;
  ntype.eval_elem = node_eval_elem;
  ntype.eval_inverse_elem = node_eval_inverse_elem;
  ntype.eval_inverse = node_eval_inverse;
  bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_boolean_math_cc
