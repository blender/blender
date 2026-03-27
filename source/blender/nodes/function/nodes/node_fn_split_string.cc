/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string_utf8.h"

#include "../geometry/node_geometry_util.hh"
#include "node_function_util.hh"

namespace blender::nodes::node_fn_split_string_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::String>("String").optional_label();
  b.add_input<decl::String>("Separator").optional_label();
  b.add_output<decl::String>("List")
      .structure_type(StructureType::List)
      .description(
          "The parts of the input string. This contains at least one element which may be empty");
}

static Vector<std::string> split_string(const StringRef original_str, const StringRef separator)
{
  if (separator.is_empty()) {
    return {original_str};
  }
  StringRef remaining = original_str;
  Vector<std::string> result;
  while (true) {
    const int separator_pos = remaining.find(separator);
    if (separator_pos == StringRef::not_found) {
      result.append(remaining);
      return result;
    }
    result.append(remaining.substr(0, separator_pos));
    remaining = remaining.substr(separator_pos + separator.size());
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const std::string str = params.extract_input<std::string>("String"_ustr);
  const std::string separator = params.extract_input<std::string>("Separator"_ustr);

  Vector<std::string> list = split_string(str, separator);
  params.set_output("List"_ustr, List::from_container(std::move(list)));
}

static void node_register()
{
  static bke::bNodeType ntype;

  fn_node_type_base(&ntype, "FunctionNodeSplitString");
  ntype.ui_name = "Split String";
  ntype.ui_description = "Split a string into a list using a separator";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_split_string_cc
