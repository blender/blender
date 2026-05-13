/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_spline_resolution_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Int>("Resolution"_ustr).field_source();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  params.set_output("Resolution"_ustr, bke::AttributeFieldInput::from<int>("resolution"));
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(
      &ntype, "GeometryNodeInputSplineResolution"_ustr, GEO_NODE_INPUT_SPLINE_RESOLUTION);
  ntype.ui_name = "Spline Resolution";
  ntype.ui_description =
      "Retrieve the number of evaluated points that will be generated for every control point on "
      "curves";
  ntype.enum_name_legacy = "INPUT_SPLINE_RESOLUTION";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_spline_resolution_cc
