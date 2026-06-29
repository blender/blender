/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_geometry_fields.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_grease_pencil_fill_id__cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Int>("Fill ID"_ustr).structure_type(StructureType::Field);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<int> fill_id = AttributeFieldInput::get_field<int, "fill_id">();
  params.set_output("Fill ID"_ustr, std::move(fill_id));
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeGreasePencilFillID"_ustr);
  ntype.ui_name = "Grease Pencil Fill ID";
  ntype.ui_description = "Retrieve information about the grouping of Grease Pencil fills";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.default_width = bke::NodeWidth::_160;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_grease_pencil_fill_id__cc
