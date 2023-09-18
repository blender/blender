/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_id_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Int>("ID").field_source().description(
      "The values from the \"id\" attribute on points, or the index if that attribute does not "
      "exist");
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<int> position_field{std::make_shared<bke::IDAttributeFieldInput>()};
  params.set_output("ID", std::move(position_field));
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_INPUT_ID, "ID", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_id_cc
