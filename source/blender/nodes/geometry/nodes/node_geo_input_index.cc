/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_index_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Int>("Index").field_source();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<int> index_field{std::make_shared<fn::IndexFieldInput>()};
  params.set_output("Index", std::move(index_field));
}

}  // namespace blender::nodes::node_geo_input_index_cc

void register_node_type_geo_input_index()
{
  namespace file_ns = blender::nodes::node_geo_input_index_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_INPUT_INDEX, "Index", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
