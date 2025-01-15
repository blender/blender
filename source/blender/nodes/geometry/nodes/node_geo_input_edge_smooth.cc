/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_edge_smooth_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Bool>("Smooth").field_source().translation_context(BLT_I18NCONTEXT_ID_MESH);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<bool> sharp = AttributeFieldInput::Create<bool>("sharp_edge");
  params.set_output("Smooth", fn::invert_boolean_field(std::move(sharp)));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeInputEdgeSmooth", GEO_NODE_INPUT_EDGE_SMOOTH);
  ntype.ui_name = "Is Edge Smooth";
  ntype.ui_description = "Retrieve whether each edge is marked for smooth or split normals";
  ntype.enum_name_legacy = "INPUT_EDGE_SMOOTH";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  blender::bke::node_register_type(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_edge_smooth_cc
