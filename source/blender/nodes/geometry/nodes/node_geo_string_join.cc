/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_string_join_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::String>("Delimiter");
  b.add_input<decl::String>("Strings").multi_input().hide_value();
  b.add_output<decl::String>("String");
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Vector<SocketValueVariant> strings = params.extract_input<Vector<SocketValueVariant>>("Strings");
  const std::string delim = params.extract_input<std::string>("Delimiter");

  std::string output;
  for (const int i : strings.index_range()) {
    output += strings[i].extract<std::string>();
    if (i < (strings.size() - 1)) {
      output += delim;
    }
  }
  params.set_output("String", std::move(output));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_STRING_JOIN, "Join Strings", NODE_CLASS_CONVERTER);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  blender::bke::nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_string_join_cc
