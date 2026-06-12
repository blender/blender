/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_geometry_fields.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_grease_pencil_draw_time__cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>("Creation Time"_ustr).structure_type(StructureType::Field);
  b.add_output<decl::Float>("Delta Time"_ustr).structure_type(StructureType::Field);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<float> init_time = AttributeFieldInput::get_field<float, "init_time">();
  params.set_output("Creation Time"_ustr, std::move(init_time));

  Field<float> delta_time = AttributeFieldInput::get_field<float, "delta_time">();
  params.set_output("Delta Time"_ustr, std::move(delta_time));
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeGreasePencilDrawTime"_ustr);
  ntype.ui_name = "Grease Pencil Draw Time";
  ntype.ui_description = "Retrieve information as to when a Grease Pencil curve was drawn";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.default_width = bke::NodeWidth::_180;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_grease_pencil_draw_time__cc
