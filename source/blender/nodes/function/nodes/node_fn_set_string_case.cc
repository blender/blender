/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array.hh"
#include "BLI_string_utf8.h"
#include "BLI_vector.hh"

#include "BKE_node_runtime.hh"

#include "NOD_socket_search_link.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_set_string_case_cc {

enum class Case {
  Uppercase = 0,
  Lowercase = 1,
};

static const EnumPropertyItem case_items[] = {
    {int(Case::Uppercase), "UPPERCASE", 0, "Uppercase", "Convert all characters to uppercase"},
    {int(Case::Lowercase), "LOWERCASE", 0, "Lowercase", "Convert all characters to lowercase"},
    {},
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::String>("String"_ustr).optional_label();
  b.add_output<decl::String>("String"_ustr).align_with_previous();
  b.add_input<decl::Menu>("Case"_ustr).static_items(case_items).optional_label();
}

static std::string apply_string_case(const std::string &s, const Case mode)
{
  if (s.empty()) {
    return s;
  }

  size_t len_bytes;
  const size_t len_chars = BLI_strlen_utf8_ex(s.c_str(), &len_bytes);

  Array<char32_t, 64> utf32(len_chars + 1);
  BLI_str_utf8_as_utf32(utf32.data(), s.c_str(), utf32.size());

  for (size_t i = 0; i < len_chars; i++) {
    const char32_t c = utf32[i];
    switch (mode) {
      case Case::Uppercase:
        utf32[i] = BLI_str_utf32_char_to_upper(c);
        break;
      case Case::Lowercase:
        utf32[i] = BLI_str_utf32_char_to_lower(c);
        break;
    }
  }

  Array<char, 64> out(len_chars * 4 + 1);
  BLI_str_utf32_as_utf8(out.data(), utf32.data(), out.size());
  return std::string(out.data());
}

static void node_gather_link_search_ops(GatherLinkSearchOpParams &params)
{
  if (!params.node_tree().typeinfo->validate_link(params.other_socket().type, SOCK_STRING)) {
    return;
  }

  if (params.in_out() == SOCK_IN) {
    for (const EnumPropertyItem *item = case_items; item->identifier != nullptr; item++) {
      if (item->name != nullptr && item->identifier[0] != '\0') {
        const int value = item->value;
        params.add_item(IFACE_(item->name), [value](LinkSearchOpParams &params) {
          bNode &node = params.add_node("FunctionNodeSetStringCase"_ustr);
          bke::node_find_socket(node, SOCK_IN, "Case"_ustr)
              ->default_value_typed<bNodeSocketValueMenu>()
              ->value = value;
          params.update_and_connect_available_socket(node, "String"_ustr);
        });
      }
    }
  }
  else {
    params.add_item(IFACE_("String"), [](LinkSearchOpParams &params) {
      bNode &node = params.add_node("FunctionNodeSetStringCase"_ustr);
      params.update_and_connect_available_socket(node, "String"_ustr);
    });
  }
}

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static auto fn = mf::build::SI2_SO<std::string, MenuValue, std::string>(
      "String Case", [](const std::string &s, MenuValue mode) -> std::string {
        return apply_string_case(s, Case(mode.value));
      });
  builder.set_matching_fn(&fn);
}

static void node_register()
{
  static bke::bNodeType ntype;

  fn_cmp_node_type_base(&ntype, "FunctionNodeSetStringCase"_ustr);
  ntype.ui_name = "Set String Case";
  ntype.ui_description = "Convert the case of a string";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.gather_link_search_ops = node_gather_link_search_ops;
  ntype.build_multi_function = node_build_multi_function;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_set_string_case_cc
