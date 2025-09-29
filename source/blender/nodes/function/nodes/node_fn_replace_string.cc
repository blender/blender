/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string_utils.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_replace_string_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::String>("String").optional_label();
  b.add_output<decl::String>("String").align_with_previous();
  b.add_input<decl::String>("Find").description("The string to find in the input string");
  b.add_input<decl::String>("Replace").description("The string to replace each match with");
}

static std::string replace_all(const StringRefNull str,
                               const StringRefNull from,
                               const StringRefNull to)
{
  if (from.is_empty()) {
    return str;
  }
  char *new_str_ptr = BLI_string_replaceN(str.c_str(), from.c_str(), to.c_str());
  std::string new_str{new_str_ptr};
  MEM_freeN(new_str_ptr);
  return new_str;
}

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static auto substring_fn = mf::build::SI3_SO<std::string, std::string, std::string, std::string>(
      "Replace", [](const std::string &str, const std::string &find, const std::string &replace) {
        return replace_all(str, find, replace);
      });
  builder.set_matching_fn(&substring_fn);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  fn_node_type_base(&ntype, "FunctionNodeReplaceString", FN_NODE_REPLACE_STRING);
  ntype.ui_name = "Replace String";
  ntype.ui_description = "Replace a given string segment with another";
  ntype.enum_name_legacy = "REPLACE_STRING";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.build_multi_function = node_build_multi_function;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_replace_string_cc
