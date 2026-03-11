/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_function_util.hh"

namespace blender::nodes::node_fn_trim_string_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::String>("String").optional_label();
  b.add_output<decl::String>("String").align_with_previous();
  b.add_input<decl::String>("Characters")
      .optional_label()
      .description("Individual characters to trim. The order of characters does not matter");
  b.add_input<decl::Bool>("Whitespace")
      .default_value(true)
      .description("Trim whitespace characters in addition to the provided characters");
  {
    auto &p = b.add_panel("Limit"_ustr).default_closed(true);
    p.add_input<decl::Bool>("Start").default_value(true).description(
        "Trim the beginning of the string");
    p.add_input<decl::Bool>("End").default_value(true).description(
        "Trim at the end of the string");
  }
}

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static auto trim_fn = mf::build::SI5_SO<std::string, std::string, bool, bool, bool, std::string>(
      "Trim",
      [](const std::string &input_str,
         const std::string &characters,
         const bool trim_whitespace,
         const bool trim_start,
         const bool trim_end) {
        std::string characters_to_trim = characters;
        if (trim_whitespace) {
          characters_to_trim.append(" \t\n\r");
        }
        StringRef str = input_str;
        int64_t start = 0;
        int64_t end = str.size();
        if (trim_start) {
          const int64_t i = str.find_first_not_of(characters_to_trim);
          if (i != StringRef::not_found) {
            start = i;
          }
        }
        if (trim_end) {
          const int64_t i = str.find_last_not_of(characters_to_trim);
          if (i != StringRef::not_found) {
            end = i + 1;
          }
        }
        std::string result = str.substr(start, end - start);
        return result;
      });
  builder.set_matching_fn(&trim_fn);
}

static void node_register()
{
  static bke::bNodeType ntype;

  fn_node_type_base(&ntype, "FunctionNodeTrimString");
  ntype.ui_name = "Trim String";
  ntype.ui_description = "Remove characters from the beginning and end of a string";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.build_multi_function = node_build_multi_function;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_trim_string_cc
