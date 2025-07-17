/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_curve_tilt_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>("Tilt").field_source();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<float> tilt_field = AttributeFieldInput::from<float>("tilt");
  params.set_output("Tilt", std::move(tilt_field));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeInputCurveTilt", GEO_NODE_INPUT_CURVE_TILT);
  ntype.ui_name = "Curve Tilt";
  ntype.ui_description =
      "Retrieve the angle at each control point used to twist the curve's normal around its "
      "tangent";
  ntype.enum_name_legacy = "INPUT_CURVE_TILT";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_curve_tilt_cc
