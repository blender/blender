/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string_utils.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_replace_string_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::String>("String");
  b.add_input<decl::String>("Find").description("The string to find in the input string");
  b.add_input<decl::String>("Replace").description("The string to replace each match with");
  b.add_output<decl::String>("String");
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

  fn_node_type_base(&ntype, FN_NODE_REPLACE_STRING, "Replace String", NODE_CLASS_CONVERTER);
  ntype.declare = node_declare;
  ntype.build_multi_function = node_build_multi_function;
  blender::bke::nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_replace_string_cc
