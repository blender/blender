/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_geometry_fields.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_nurbs_weight__cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>("Weight"_ustr).structure_type(StructureType::Field);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<float> nurbs_weight = AttributeFieldInput::get_field<float, "nurbs_weight">();
  params.set_output("Weight"_ustr, std::move(nurbs_weight));
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeNURBSWeight"_ustr);
  ntype.ui_name = "NURBS Weight";
  ntype.ui_description = "Retrieve the influence of each NURBS control point on the curve";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_nurbs_weight__cc
