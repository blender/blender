/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string_utf8.h"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_reverse_string_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::String>("String"_ustr).optional_label();
  b.add_output<decl::String>("String"_ustr).align_with_previous();
}

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static auto reverse_fn = mf::build::SI1_SO<std::string, std::string>(
      "Reverse", [](const std::string &s) {
        if (s.empty()) {
          return std::string();
        }
        std::string result;
        result.reserve(s.size());

        const char *start = s.data();
        const char *curr = start + s.size();
        while (curr > start) {
          const char *prev = BLI_str_find_prev_char_utf8(curr, start);
          size_t char_len = curr - prev;
          result.append(prev, char_len);
          curr = prev;
        }
        return result;
      });
  builder.set_matching_fn(&reverse_fn);
}

static void node_register()
{
  static bke::bNodeType ntype;

  fn_cmp_node_type_base(&ntype, "FunctionNodeReverseString"_ustr);
  ntype.ui_name = "Reverse String";
  ntype.ui_description = "Reverse the order of the characters in a string";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.build_multi_function = node_build_multi_function;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_reverse_string_cc
