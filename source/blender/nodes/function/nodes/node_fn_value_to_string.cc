/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_function_util.hh"

#include "NOD_rna_define.hh"
#include "NOD_socket_search_link.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include <iomanip>
#include <sstream>
#include <string>

namespace blender::nodes::node_fn_value_to_string_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();

  if (node != nullptr) {
    const eNodeSocketDatatype data_type = eNodeSocketDatatype(node->custom1);
    b.add_input(data_type, "Value");

    auto &decimals = b.add_input<decl::Int>("Decimals").min(0);
    decimals.available(data_type == SOCK_FLOAT);
  }

  b.add_output<decl::String>("String");
}

static const mf::MultiFunction *get_multi_function(const bNode &bnode)
{
  static auto float_to_str_fn = mf::build::SI2_SO<float, int, std::string>(
      "Value To String", [](float a, int b) {
        std::stringstream stream;
        stream << std::fixed << std::setprecision(std::max(0, b)) << a;
        return stream.str();
      });

  static auto int_to_str_fn = mf::build::SI1_SO<int, std::string>(
      "Value To String", [](int a) { return std::to_string(a); });

  switch (bnode.custom1) {
    case SOCK_FLOAT:
      return &float_to_str_fn;
    case SOCK_INT:
      return &int_to_str_fn;
  }

  BLI_assert_unreachable();
  return nullptr;
}

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const mf::MultiFunction *fn = get_multi_function(builder.node());
  builder.set_matching_fn(fn);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = SOCK_FLOAT;
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const eNodeSocketDatatype socket_type = eNodeSocketDatatype(params.other_socket().type);
  if (params.in_out() == SOCK_IN) {
    if (socket_type == SOCK_INT) {
      params.add_item(IFACE_("Value"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("FunctionNodeValueToString");
        node.custom1 = SOCK_INT;
        params.update_and_connect_available_socket(node, "Value");
      });
      params.add_item(IFACE_("Decimals"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("FunctionNodeValueToString");
        params.update_and_connect_available_socket(node, "Decimals");
      });
    }
    else {
      if (params.node_tree().typeinfo->validate_link(socket_type, SOCK_FLOAT)) {
        params.add_item(IFACE_("Value"), [](LinkSearchOpParams &params) {
          bNode &node = params.add_node("FunctionNodeValueToString");
          node.custom1 = SOCK_FLOAT;
          params.update_and_connect_available_socket(node, "Value");
        });
      }
    }
  }
  else {
    if (socket_type == SOCK_STRING) {
      params.add_item(IFACE_("String"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("FunctionNodeValueToString");
        params.update_and_connect_available_socket(node, "String");
      });
    }
  }
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_rna(StructRNA *srna)
{
  static const EnumPropertyItem data_types[] = {
      {SOCK_FLOAT, "FLOAT", ICON_NODE_SOCKET_FLOAT, "Float", "Floating-point value"},
      {SOCK_INT, "INT", ICON_NODE_SOCKET_INT, "Integer", "32-bit integer"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_node_enum(srna,
                    "data_type",
                    "Data Type",
                    "",
                    data_types,
                    NOD_inline_enum_accessors(custom1),
                    SOCK_FLOAT);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  fn_node_type_base(&ntype, "FunctionNodeValueToString", FN_NODE_VALUE_TO_STRING);
  ntype.ui_name = "Value to String";
  ntype.ui_description = "Generate a string representation of the given input value";
  ntype.enum_name_legacy = "VALUE_TO_STRING";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.draw_buttons = node_layout;
  ntype.build_multi_function = node_build_multi_function;
  ntype.gather_link_search_ops = node_gather_link_searches;
  blender::bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_value_to_string_cc
