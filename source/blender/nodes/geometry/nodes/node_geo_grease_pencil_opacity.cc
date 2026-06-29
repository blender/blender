/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_geometry_fields.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_grease_pencil_opacity__cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>("Point"_ustr).structure_type(StructureType::Field);
  b.add_output<decl::Float>("Fill"_ustr).structure_type(StructureType::Field);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<float> point_opacity = AttributeFieldInput::get_field<float, "opacity">();
  params.set_output("Point"_ustr, std::move(point_opacity));

  Field<float> fill_opacity = AttributeFieldInput::get_field<float, "fill_opacity">();
  params.set_output("Fill"_ustr, std::move(fill_opacity));
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeGreasePencilOpacity"_ustr);
  ntype.ui_name = "Grease Pencil Opacity";
  ntype.ui_description = "Retrieve the opacity of Grease Pencil points and fills";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.default_width = bke::NodeWidth::_160;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_grease_pencil_opacity__cc
