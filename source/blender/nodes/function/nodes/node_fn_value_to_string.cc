/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_function_util.hh"

#include "NOD_rna_define.hh"
#include "NOD_socket_search_link.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include <algorithm>
#include <charconv>
#include <iomanip>
#include <sstream>
#include <string>

namespace blender::nodes::node_fn_value_to_string_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  const bNode *node = b.node_or_null();

  if (node != nullptr) {
    const eNodeSocketDatatype data_type = eNodeSocketDatatype(node->custom1);
    b.add_input(data_type, "Value"_ustr);

    auto &decimals = b.add_input<decl::Int>("Decimals"_ustr).min(0);
    decimals.available(data_type == SOCK_FLOAT);

    b.add_input<decl::Int>("Base"_ustr)
        .min(2)
        .max(36)
        .default_value(10)
        .description("Numeric base for the output string (e.g. 2 for binary, 16 for hexadecimal)")
        .available(data_type == SOCK_INT);

    b.add_input<decl::Int>("Padding"_ustr)
        .min(0)
        .default_value(0)
        .description("Minimum number of characters in the output, zero-padded if shorter")
        .available(data_type == SOCK_INT);
  }

  b.add_output<decl::String>("String"_ustr);
}

static const mf::MultiFunction *get_multi_function(const bNode &bnode)
{
  static auto float_to_str_fn = mf::build::SI2_SO<float, int, std::string>(
      "Value To String", [](float a, int b) {
        std::stringstream stream;
        stream << std::fixed << std::setprecision(std::max(0, b)) << a;
        return stream.str();
      });

  static auto int_to_str_fn = mf::build::SI3_SO<int, int, int, std::string>(
      "Value To String", [](int value, int base, int padding) -> std::string {
        if (base < 2 || base > 36) {
          return {};
        }
        padding = std::max(0, padding);
        /* Maximum possible string length is reached with -2^31=-2147483648 and base 2. */
        char buf[33];
        auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value, base);
        std::string result(buf, ptr);
        if (padding > int(result.size())) {
          const size_t needed = size_t(padding) - result.size();
          const size_t insert_pos = (!result.empty() && result[0] == '-') ? 1 : 0;
          result.insert(insert_pos, needed, '0');
        }
        return result;
      });

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
        bNode &node = params.add_node("FunctionNodeValueToString"_ustr);
        node.custom1 = SOCK_INT;
        params.update_and_connect_available_socket(node, "Value"_ustr);
      });
      params.add_item(IFACE_("Decimals"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("FunctionNodeValueToString"_ustr);
        params.update_and_connect_available_socket(node, "Decimals"_ustr);
      });
    }
    else {
      if (params.node_tree().typeinfo->validate_link(socket_type, SOCK_FLOAT)) {
        params.add_item(IFACE_("Value"), [](LinkSearchOpParams &params) {
          bNode &node = params.add_node("FunctionNodeValueToString"_ustr);
          node.custom1 = SOCK_FLOAT;
          params.update_and_connect_available_socket(node, "Value"_ustr);
        });
      }
    }
  }
  else {
    if (socket_type == SOCK_STRING) {
      params.add_item(IFACE_("String"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("FunctionNodeValueToString"_ustr);
        params.update_and_connect_available_socket(node, "String"_ustr);
      });
    }
  }
}

static void node_layout(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.prop(ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
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
  static bke::bNodeType ntype;

  fn_cmp_node_type_base(&ntype, "FunctionNodeValueToString"_ustr, FN_NODE_VALUE_TO_STRING);
  ntype.ui_name = "Value to String";
  ntype.ui_description = "Generate a string representation of the given input value";
  ntype.enum_name_legacy = "VALUE_TO_STRING";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.draw_buttons = node_layout;
  ntype.build_multi_function = node_build_multi_function;
  ntype.gather_link_search_ops = node_gather_link_searches;
  bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_value_to_string_cc
