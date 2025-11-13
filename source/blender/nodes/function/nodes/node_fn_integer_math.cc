/* SPDX-FileCopyrightText: 2024 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <numeric>

#include "BLI_math_base.h"
#include "BLI_string.h"

#include "RNA_enum_types.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "NOD_inverse_eval_params.hh"
#include "NOD_rna_define.hh"
#include "NOD_socket_search_link.hh"
#include "NOD_value_elem_eval.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_integer_math_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();

  b.add_input<decl::Int>("Value").label_fn([](bNode node) {
    switch (node.custom1) {
      case NODE_INTEGER_MATH_POWER:
        return IFACE_("Base");
      default:
        return IFACE_("Value");
    }
  });

  b.add_input<decl::Int>("Value", "Value_001").label_fn([](bNode node) {
    switch (node.custom1) {
      case NODE_INTEGER_MATH_MULTIPLY_ADD:
        return IFACE_("Multiplier");
      case NODE_INTEGER_MATH_POWER:
        return IFACE_("Exponent");
      default:
        return IFACE_("Value");
    }
  });
  b.add_input<decl::Int>("Value", "Value_002").label_fn([](bNode node) {
    switch (node.custom1) {
      case NODE_INTEGER_MATH_MULTIPLY_ADD:
        return IFACE_("Addend");
      default:
        return IFACE_("Value");
    }
  });
  b.add_output<decl::Int>("Value");
};

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "operation", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const bool one_input_ops = ELEM(
      node->custom1, NODE_INTEGER_MATH_ABSOLUTE, NODE_INTEGER_MATH_SIGN, NODE_INTEGER_MATH_NEGATE);
  const bool three_input_ops = ELEM(node->custom1, NODE_INTEGER_MATH_MULTIPLY_ADD);

  bNodeSocket *sockA = static_cast<bNodeSocket *>(node->inputs.first);
  bNodeSocket *sockB = sockA->next;
  bNodeSocket *sockC = sockB->next;

  bke::node_set_socket_availability(*ntree, *sockB, !one_input_ops);
  bke::node_set_socket_availability(*ntree, *sockC, three_input_ops);
}

class SocketSearchOp {
 public:
  std::string socket_name;
  NodeIntegerMathOperation operation;
  void operator()(LinkSearchOpParams &params)
  {
    bNode &node = params.add_node("FunctionNodeIntegerMath");
    node.custom1 = NodeIntegerMathOperation(operation);
    params.update_and_connect_available_socket(node, socket_name);
  }
};

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  if (!params.node_tree().typeinfo->validate_link(eNodeSocketDatatype(params.other_socket().type),
                                                  SOCK_INT))
  {
    return;
  }

  const bool is_integer = params.other_socket().type == SOCK_INT;
  const int weight = is_integer ? 0 : -1;

  /* Add socket A operations. */
  for (const EnumPropertyItem *item = rna_enum_node_integer_math_items;
       item->identifier != nullptr;
       item++)
  {
    if (item->name != nullptr && item->identifier[0] != '\0') {
      params.add_item(CTX_IFACE_(BLT_I18NCONTEXT_ID_NODETREE, item->name),
                      SocketSearchOp{"Value", NodeIntegerMathOperation(item->value)},
                      weight);
    }
  }
}

static void node_label(const bNodeTree * /*ntree*/,
                       const bNode *node,
                       char *label,
                       int label_maxncpy)
{
  const char *name;
  bool enum_label = RNA_enum_name(rna_enum_node_integer_math_items, node->custom1, &name);
  if (!enum_label) {
    name = CTX_N_(BLT_I18NCONTEXT_ID_NODETREE, "Unknown");
  }
  BLI_strncpy(label, CTX_IFACE_(BLT_I18NCONTEXT_ID_NODETREE, name), label_maxncpy);
}

/* Derived from `divide_round_i` but fixed to be safe and handle negative inputs. */
static int safe_divide_round_i(const int a, const int b)
{
  const int c = math::abs(b);
  return (a >= 0) ? math::safe_divide((2 * a + c), (2 * c)) * math::sign(b) :
                    -math::safe_divide((2 * -a + c), (2 * c)) * math::sign(b);
}

static const mf::MultiFunction *get_multi_function(const bNode &bnode)
{
  NodeIntegerMathOperation operation = NodeIntegerMathOperation(bnode.custom1);
  static auto exec_preset = mf::build::exec_presets::AllSpanOrSingle();
  static auto add_fn = mf::build::SI2_SO<int, int, int>(
      "Add", [](int a, int b) { return a + b; }, exec_preset);
  static auto sub_fn = mf::build::SI2_SO<int, int, int>(
      "Subtract", [](int a, int b) { return a - b; }, exec_preset);
  static auto multiply_fn = mf::build::SI2_SO<int, int, int>(
      "Multiply", [](int a, int b) { return a * b; }, exec_preset);
  static auto divide_fn = mf::build::SI2_SO<int, int, int>(
      "Divide", [](int a, int b) { return math::safe_divide(a, b); }, exec_preset);
  static auto divide_floor_fn = mf::build::SI2_SO<int, int, int>(
      "Divide Floor",
      [](int a, int b) { return (b != 0) ? divide_floor_i(a, b) : 0; },
      exec_preset);
  static auto divide_ceil_fn = mf::build::SI2_SO<int, int, int>(
      "Divide Ceil",
      [](int a, int b) { return (b != 0) ? -divide_floor_i(a, -b) : 0; },
      exec_preset);
  static auto divide_round_fn = mf::build::SI2_SO<int, int, int>(
      "Divide Round", [](int a, int b) { return safe_divide_round_i(a, b); }, exec_preset);
  static auto pow_fn = mf::build::SI2_SO<int, int, int>(
      "Power", [](int a, int b) { return math::pow(a, b); }, exec_preset);
  static auto madd_fn = mf::build::SI3_SO<int, int, int, int>(
      "Multiply Add", [](int a, int b, int c) { return a * b + c; }, exec_preset);
  static auto floored_mod_fn = mf::build::SI2_SO<int, int, int>(
      "Floored Modulo",
      [](int a, int b) { return b != 0 ? math::mod_periodic(a, b) : 0; },
      exec_preset);
  static auto mod_fn = mf::build::SI2_SO<int, int, int>(
      "Modulo", [](int a, int b) { return b != 0 ? a % b : 0; }, exec_preset);
  static auto abs_fn = mf::build::SI1_SO<int, int>(
      "Absolute", [](int a) { return math::abs(a); }, exec_preset);
  static auto sign_fn = mf::build::SI1_SO<int, int>(
      "Sign", [](int a) { return math::sign(a); }, exec_preset);
  static auto min_fn = mf::build::SI2_SO<int, int, int>(
      "Minimum", [](int a, int b) { return math::min(a, b); }, exec_preset);
  static auto max_fn = mf::build::SI2_SO<int, int, int>(
      "Maximum", [](int a, int b) { return math::max(a, b); }, exec_preset);
  static auto gcd_fn = mf::build::SI2_SO<int, int, int>(
      "GCD", [](int a, int b) { return std::gcd(a, b); }, exec_preset);
  static auto lcm_fn = mf::build::SI2_SO<int, int, int>(
      "LCM", [](int a, int b) { return std::lcm(a, b); }, exec_preset);
  static auto negate_fn = mf::build::SI1_SO<int, int>(
      "Negate", [](int a) { return -a; }, exec_preset);

  switch (operation) {
    case NODE_INTEGER_MATH_ADD:
      return &add_fn;
    case NODE_INTEGER_MATH_SUBTRACT:
      return &sub_fn;
    case NODE_INTEGER_MATH_MULTIPLY:
      return &multiply_fn;
    case NODE_INTEGER_MATH_DIVIDE:
      return &divide_fn;
    case NODE_INTEGER_MATH_DIVIDE_FLOOR:
      return &divide_floor_fn;
    case NODE_INTEGER_MATH_DIVIDE_CEIL:
      return &divide_ceil_fn;
    case NODE_INTEGER_MATH_DIVIDE_ROUND:
      return &divide_round_fn;
    case NODE_INTEGER_MATH_POWER:
      return &pow_fn;
    case NODE_INTEGER_MATH_MULTIPLY_ADD:
      return &madd_fn;
    case NODE_INTEGER_MATH_FLOORED_MODULO:
      return &floored_mod_fn;
    case NODE_INTEGER_MATH_MODULO:
      return &mod_fn;
    case NODE_INTEGER_MATH_ABSOLUTE:
      return &abs_fn;
    case NODE_INTEGER_MATH_SIGN:
      return &sign_fn;
    case NODE_INTEGER_MATH_MINIMUM:
      return &min_fn;
    case NODE_INTEGER_MATH_MAXIMUM:
      return &max_fn;
    case NODE_INTEGER_MATH_GCD:
      return &gcd_fn;
    case NODE_INTEGER_MATH_LCM:
      return &lcm_fn;
    case NODE_INTEGER_MATH_NEGATE:
      return &negate_fn;
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
  const NodeIntegerMathOperation op = NodeIntegerMathOperation(params.node.custom1);
  switch (op) {
    case NODE_INTEGER_MATH_ADD:
    case NODE_INTEGER_MATH_SUBTRACT:
    case NODE_INTEGER_MATH_MULTIPLY:
    case NODE_INTEGER_MATH_DIVIDE: {
      IntElem output_elem = params.get_input_elem<IntElem>("Value");
      output_elem.merge(params.get_input_elem<IntElem>("Value_001"));
      params.set_output_elem("Value", output_elem);
      break;
    }
    default:
      break;
  }
}

static void node_eval_inverse_elem(value_elem::InverseElemEvalParams &params)
{
  const NodeIntegerMathOperation op = NodeIntegerMathOperation(params.node.custom1);
  switch (op) {
    case NODE_INTEGER_MATH_ADD:
    case NODE_INTEGER_MATH_SUBTRACT:
    case NODE_INTEGER_MATH_MULTIPLY:
    case NODE_INTEGER_MATH_DIVIDE: {
      params.set_input_elem("Value", params.get_output_elem<value_elem::IntElem>("Value"));
      break;
    }
    default:
      break;
  }
}

static void node_eval_inverse(inverse_eval::InverseEvalParams &params)
{
  const NodeIntegerMathOperation op = NodeIntegerMathOperation(params.node.custom1);
  const StringRef first_input_id = "Value";
  const StringRef second_input_id = "Value_001";
  const StringRef output_id = "Value";
  switch (op) {
    case NODE_INTEGER_MATH_ADD: {
      params.set_input(first_input_id,
                       params.get_output<int>(output_id) - params.get_input<int>(second_input_id));
      break;
    }
    case NODE_INTEGER_MATH_SUBTRACT: {
      params.set_input(first_input_id,
                       params.get_output<int>(output_id) + params.get_input<int>(second_input_id));
      break;
    }
    case NODE_INTEGER_MATH_MULTIPLY: {
      params.set_input(first_input_id,
                       math::safe_divide(params.get_output<int>(output_id),
                                         params.get_input<int>(second_input_id)));
      break;
    }
    case NODE_INTEGER_MATH_DIVIDE: {
      params.set_input(first_input_id,
                       params.get_output<int>(output_id) * params.get_input<int>(second_input_id));
      break;
    }
    default: {
      break;
    }
  }
}

static void node_rna(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_node_enum(srna,
                           "operation",
                           "Operation",
                           "",
                           rna_enum_node_integer_math_items,
                           NOD_inline_enum_accessors(custom1),
                           NODE_INTEGER_MATH_ADD);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_NODETREE);
  RNA_def_property_update_runtime(prop, rna_Node_socket_update);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  fn_node_type_base(&ntype, "FunctionNodeIntegerMath", FN_NODE_INTEGER_MATH);
  ntype.ui_name = "Integer Math";
  ntype.ui_description = "Perform various math operations on the given integer inputs";
  ntype.enum_name_legacy = "INTEGER_MATH";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.labelfunc = node_label;
  ntype.updatefunc = node_update;
  ntype.build_multi_function = node_build_multi_function;
  ntype.draw_buttons = node_layout;
  ntype.gather_link_search_ops = node_gather_link_searches;
  ntype.eval_elem = node_eval_elem;
  ntype.eval_inverse_elem = node_eval_inverse_elem;
  ntype.eval_inverse = node_eval_inverse;

  blender::bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_integer_math_cc
