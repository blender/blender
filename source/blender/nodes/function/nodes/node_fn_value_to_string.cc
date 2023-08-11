/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_function_util.hh"
#include <iomanip>
#include <sstream>

namespace blender::nodes::node_fn_value_to_string_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Value");
  b.add_input<decl::Int>("Decimals").min(0);
  b.add_output<decl::String>("String");
}

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static auto to_str_fn = mf::build::SI2_SO<float, int, std::string>(
      "Value To String", [](float a, int b) {
        std::stringstream stream;
        stream << std::fixed << std::setprecision(std::max(0, b)) << a;
        return stream.str();
      });
  builder.set_matching_fn(&to_str_fn);
}

static void node_register()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_VALUE_TO_STRING, "Value to String", NODE_CLASS_CONVERTER);
  ntype.declare = node_declare;
  ntype.build_multi_function = node_build_multi_function;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_value_to_string_cc
