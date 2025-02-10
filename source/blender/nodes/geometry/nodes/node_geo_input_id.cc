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
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeInputID", GEO_NODE_INPUT_ID);
  ntype.ui_name = "ID";
  ntype.ui_description =
      "Retrieve a stable random identifier value from the \"id\" attribute on the point domain, "
      "or the index if the attribute does not exist";
  ntype.enum_name_legacy = "INPUT_ID";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  blender::bke::node_register_type(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_id_cc
