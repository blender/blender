/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string_utf8.h"

#include "fast_float.h"

#include "node_function_util.hh"

#include "NOD_rna_define.hh"
#include "NOD_socket_search_link.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include <charconv>

namespace blender::nodes::node_fn_string_to_value_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::String>("String").optional_label();

  const bNode *node = b.node_or_null();
  if (node != nullptr) {
    const eNodeSocketDatatype data_type = eNodeSocketDatatype(node->custom1);
    b.add_output(data_type, "Value");
  }

  b.add_output<decl::Int>("Length");
}

static const mf::MultiFunction *get_multi_function(const bNode &bnode)
{
  static auto str_to_float_fn = mf::build::SI1_SO2<std::string, float, int>(
      "String to Value", [](const std::string &s, float &value, int &length) -> void {
        const auto result = fast_float::from_chars(s.data(), s.data() + s.size(), value);
        length = BLI_strnlen_utf8(s.data(), result.ptr - s.data());
      });

  static auto str_to_int_fn = mf::build::SI1_SO2<std::string, int, int>(
      "String to Value", [](const std::string &s, int &value, int &length) -> void {
        const auto result = std::from_chars(s.data(), s.data() + s.size(), value);
        length = BLI_strnlen_utf8(s.data(), result.ptr - s.data());
      });

  switch (eNodeSocketDatatype(bnode.custom1)) {
    case SOCK_FLOAT:
      return &str_to_float_fn;
    case SOCK_INT:
      return &str_to_int_fn;
    default:
      BLI_assert_unreachable();
      return nullptr;
  }
}

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const mf::MultiFunction *fn = get_multi_function(builder.node());
  builder.set_matching_fn(fn);
}

static void node_init(bNodeTree *, bNode *node)
{
  node->custom1 = SOCK_FLOAT;
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const eNodeSocketDatatype socket_type = eNodeSocketDatatype(params.other_socket().type);
  if (params.in_out() == SOCK_IN) {
    if (socket_type == SOCK_STRING) {
      params.add_item(IFACE_("String"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("FunctionNodeStringToValue");
        params.update_and_connect_available_socket(node, "String");
      });
    }
  }
  else if (params.in_out() == SOCK_OUT) {
    if (ELEM(socket_type, SOCK_INT, SOCK_BOOLEAN)) {
      params.add_item(IFACE_("Value"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("FunctionNodeStringToValue");
        node.custom1 = SOCK_INT;
        params.update_and_connect_available_socket(node, "Value");
      });
    }
    else if (params.node_tree().typeinfo->validate_link(SOCK_FLOAT, socket_type)) {
      params.add_item(IFACE_("Value"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("FunctionNodeStringToValue");
        node.custom1 = SOCK_FLOAT;
        params.update_and_connect_available_socket(node, "Value");
      });
    }

    if (socket_type == SOCK_INT) {
      params.add_item(IFACE_("Length"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("FunctionNodeStringToValue");
        params.update_and_connect_available_socket(node, "Length");
      });
    }
  }
}

static void node_layout(uiLayout *layout, bContext *, PointerRNA *ptr)
{
  layout->prop(ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_rna(StructRNA *srna)
{
  static const EnumPropertyItem data_types[] = {
      {SOCK_FLOAT, "FLOAT", ICON_NODE_SOCKET_FLOAT, "Float", "Floating-point value"},
      {SOCK_INT, "INT", ICON_NODE_SOCKET_INT, "Integer", "32-bit integer"},
      {0, nullptr, 0, nullptr, nullptr}};

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

  fn_node_type_base(&ntype, "FunctionNodeStringToValue");
  ntype.ui_name = "String to Value";
  ntype.ui_description = "Derive a numeric value from a given string representation";
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

}  // namespace blender::nodes::node_fn_string_to_value_cc
