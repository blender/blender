/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_spline_cyclic_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Bool>("Cyclic").field_source();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<bool> cyclic_field = AttributeFieldInput::from<bool>("cyclic");
  params.set_output("Cyclic", std::move(cyclic_field));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeInputSplineCyclic", GEO_NODE_INPUT_SPLINE_CYCLIC);
  ntype.ui_name = "Is Spline Cyclic";
  ntype.ui_description = "Retrieve whether each spline endpoint connects to the beginning";
  ntype.enum_name_legacy = "INPUT_SPLINE_CYCLIC";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_spline_cyclic_cc
