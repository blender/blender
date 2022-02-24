/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string_utf8.h"

#include <iomanip>

#include "node_function_util.hh"

namespace blender::nodes::node_fn_string_length_cc {

static void fn_node_string_length_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::String>(N_("String"));
  b.add_output<decl::Int>(N_("Length"));
}

static void fn_node_string_length_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static fn::CustomMF_SI_SO<std::string, int> str_len_fn{
      "String Length", [](const std::string &a) { return BLI_strlen_utf8(a.c_str()); }};
  builder.set_matching_fn(&str_len_fn);
}

}  // namespace blender::nodes::node_fn_string_length_cc

void register_node_type_fn_string_length()
{
  namespace file_ns = blender::nodes::node_fn_string_length_cc;

  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_STRING_LENGTH, "String Length", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::fn_node_string_length_declare;
  ntype.build_multi_function = file_ns::fn_node_string_length_build_multi_function;
  nodeRegisterType(&ntype);
}
