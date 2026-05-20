/* SPDX-FileCopyrightText: 2025 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string.h"

#include "RNA_enum_types.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "NOD_rna_define.hh"
#include "NOD_socket_search_link.hh"

#include "FN_multi_function_registry.hh"

#include "node_function_util.hh"

namespace blender {

static_assert(-1 == ~0, "Two's complement must be used for bitwise operations.");

namespace nodes::node_fn_bit_math_cc {

enum BitMathOperation : int16_t {
  And = 0,
  Or = 1,
  Xor = 2,
  Not = 3,
  Shift = 4,
  Rotate = 5,
};

const std::array<EnumPropertyItem, 7> bit_math_operation_items = {{
    {BitMathOperation::And,
     "AND",
     0,
     "And",
     "Returns a value where the bits of A and B are both set"},
    {BitMathOperation::Or,
     "OR",
     0,
     "Or",
     "Returns a value where the bits of either A or B are set"},
    {BitMathOperation::Xor,
     "XOR",
     0,
     "Exclusive Or",
     "Returns a value where only one bit from A and B is set"},
    {BitMathOperation::Not,
     "NOT",
     0,
     "Not",
     "Returns the opposite bit value of A, in decimal it is equivalent of A = -A - 1"},
    {BitMathOperation::Shift,
     "SHIFT",
     0,
     "Shift",
     "Shifts the bit values of A by the specified Shift amount. Positive values shift left, "
     "negative values shift right."},
    {BitMathOperation::Rotate,
     "ROTATE",
     0,
     "Rotate",
     "Rotates the bit values of A by the specified Shift amount. Positive values rotate left, "
     "negative values rotate right."},
    {0, nullptr, 0, nullptr, nullptr},
}};

constexpr static int32_t max_shift = sizeof(int32_t) * CHAR_BIT - 1;
constexpr static int32_t min_shift = -max_shift;

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Int>("A"_ustr);
  auto &b_socket = b.add_input<decl::Int>("B"_ustr);
  auto &shift = b.add_input<decl::Int>("Shift"_ustr).min(min_shift).max(max_shift);
  b.add_output<decl::Int>("Value"_ustr);

  if (const bNode *node = b.node_or_null()) {
    const BitMathOperation operation = BitMathOperation(node->custom1);
    b_socket.available(!ELEM(
        operation, BitMathOperation::Not, BitMathOperation::Shift, BitMathOperation::Rotate));
    shift.available(ELEM(operation, BitMathOperation::Shift, BitMathOperation::Rotate));
  }
};

static void node_layout(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.prop(ptr, "operation", UI_ITEM_NONE, "", ICON_NONE);
}

class SocketSearchOp {
 public:
  UString socket_name;
  BitMathOperation operation;
  void operator()(LinkSearchOpParams &params)
  {
    bNode &node = params.add_node("FunctionNodeBitMath"_ustr);
    node.custom1 = int16_t(operation);
    params.update_and_connect_available_socket(node, socket_name);
  }
};

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  if (!params.node_tree().typeinfo->validate_link(params.other_socket().type, SOCK_INT)) {
    return;
  }

  const bool is_integer = params.other_socket().type == SOCK_INT;
  const int weight = is_integer ? 0 : -1;

  const UString socket_name = (params.in_out() == SOCK_OUT) ? "Value"_ustr : "A"_ustr;

  for (const auto &item : bit_math_operation_items) {
    if (item.name != nullptr && item.identifier[0] != '\0') {
      params.add_item(
          IFACE_(item.name), SocketSearchOp{socket_name, BitMathOperation(item.value)}, weight);
    }
  }
}

static void node_label(const bNodeTree * /*ntree*/,
                       const bNode *node,
                       char *label,
                       int label_maxncpy)
{
  char name[64] = {0};
  const char *operation_name = IFACE_("Unknown");
  /* NOTE: This assumes that the matching RNA enum property also uses the default i18n context, and
   * needs to be kept manually in sync. */
  RNA_enum_name_gettexted(
      bit_math_operation_items.data(), node->custom1, BLT_I18NCONTEXT_DEFAULT, &operation_name);
  SNPRINTF(name, IFACE_("Bitwise %s"), operation_name);
  BLI_strncpy(label, name, label_maxncpy);
}

static const mf::MultiFunction *get_multi_function(const bNode &bnode)
{
  const BitMathOperation operation = BitMathOperation(bnode.custom1);
  switch (operation) {
    case BitMathOperation::And:
      return &fn::multi_function::registry::lookup("int & int"_ustr);
    case BitMathOperation::Or:
      return &fn::multi_function::registry::lookup("int | int"_ustr);
    case BitMathOperation::Xor:
      return &fn::multi_function::registry::lookup("int ^ int"_ustr);
    case BitMathOperation::Not:
      return &fn::multi_function::registry::lookup("~int"_ustr);
    case BitMathOperation::Shift:
      return &fn::multi_function::registry::lookup("shift(int, int)"_ustr);
    case BitMathOperation::Rotate:
      return &fn::multi_function::registry::lookup("rotate(int, int)"_ustr);
  }
  BLI_assert_unreachable();
  return nullptr;
}

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const mf::MultiFunction *fn = get_multi_function(builder.node());
  builder.set_matching_fn(fn);
}

static void node_rna(StructRNA *srna)
{
  PropertyRNA *prop = RNA_def_node_enum(srna,
                                        "operation",
                                        "Operation",
                                        "",
                                        bit_math_operation_items.data(),
                                        NOD_inline_enum_accessors(custom1),
                                        BitMathOperation::And);
  RNA_def_property_update_runtime(prop, rna_Node_socket_update);
}

static void node_register()
{
  static bke::bNodeType ntype;

  fn_node_type_base(&ntype, "FunctionNodeBitMath"_ustr);
  ntype.ui_name = "Bit Math";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.labelfunc = node_label;
  ntype.build_multi_function = node_build_multi_function;
  ntype.draw_buttons = node_layout;
  ntype.gather_link_search_ops = node_gather_link_searches;
  ntype.ui_description = "Perform bitwise operations on 32-bit integers";

  bke::node_register_type(ntype);
  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace nodes::node_fn_bit_math_cc

}  // namespace blender
