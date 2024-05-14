/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string_utf8.h"

#include <iomanip>

#include "node_function_util.hh"

namespace blender::nodes::node_fn_string_length_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::String>("String");
  b.add_output<decl::Int>("Length");
}

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static auto str_len_fn = mf::build::SI1_SO<std::string, int>(
      "String Length", [](const std::string &a) { return BLI_strlen_utf8(a.c_str()); });
  builder.set_matching_fn(&str_len_fn);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_STRING_LENGTH, "String Length", NODE_CLASS_CONVERTER);
  ntype.declare = node_declare;
  ntype.build_multi_function = node_build_multi_function;
  blender::bke::nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_string_length_cc
