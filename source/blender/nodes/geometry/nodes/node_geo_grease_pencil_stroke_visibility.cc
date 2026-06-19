/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_geometry_fields.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_grease_pencil_stroke_visibility__cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Bool>("Is Hidden"_ustr).structure_type(StructureType::Field);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<bool> hide_stroke = AttributeFieldInput::get_field<bool, "hide_stroke">();
  params.set_output("Is Hidden"_ustr, std::move(hide_stroke));
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeGreasePencilStrokeVisibility"_ustr);
  ntype.ui_name = "Grease Pencil Stroke Visibility";
  ntype.ui_description = "Retrieve the visibility of Grease Pencil strokes";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.default_width = bke::NodeWidth::_200;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_grease_pencil_stroke_visibility__cc
