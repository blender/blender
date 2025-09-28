/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string_utf8.h"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_find_in_string_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::String>("String").optional_label();
  b.add_input<decl::String>("Search");
  b.add_output<decl::Int>("First Found");
  b.add_output<decl::Int>("Count");
}

static int string_find(const StringRef text, const StringRef token)
{
  if (text.is_empty() || token.is_empty()) {
    return 0;
  }
  const int pos = text.find(token, 0);
  size_t r_len_bytes;
  const int pos_n = BLI_strnlen_utf8_ex(text.data(), pos, &r_len_bytes);
  return pos_n;
}

static int string_count(const StringRef text, const StringRef token)
{
  if (text.is_empty() || token.is_empty()) {
    return 0;
  }
  int count = 0;
  const int match_len = token.size();
  int pos = 0;
  while ((pos = text.find(token, pos)) != StringRef::not_found) {
    count++;
    pos += match_len;
  }
  return count;
}

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static auto token_position_count = mf::build::SI2_SO2<std::string, std::string, int, int>(
      "Find in String",
      [](const std::string &text, const std::string &token, int &first, int &count) -> void {
        first = string_find(text, token);
        count = string_count(text, token);
      });

  builder.set_matching_fn(&token_position_count);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  fn_node_type_base(&ntype, "FunctionNodeFindInString", FN_NODE_FIND_IN_STRING);
  ntype.ui_name = "Find in String";
  ntype.ui_description =
      "Find the number of times a given string occurs in another string and the position of the "
      "first match";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.build_multi_function = node_build_multi_function;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_find_in_string_cc
