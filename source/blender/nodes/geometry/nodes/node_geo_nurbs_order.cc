/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_geometry_fields.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_nurbs_order__cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Int>("Order"_ustr).structure_type(StructureType::Field);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<int> nurbs_order = AttributeFieldInput::get_field<int, "nurbs_order">();
  params.set_output("Order"_ustr, std::move(nurbs_order));
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeNURBSOrder"_ustr);
  ntype.ui_name = "NURBS Order";
  ntype.ui_description = "Retrieve how many curve control points influence each evaluated point";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_nurbs_order__cc
