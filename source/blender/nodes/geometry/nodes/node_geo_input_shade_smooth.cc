/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_shade_smooth_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Bool>("Smooth").field_source();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<bool> shade_smooth_field = AttributeFieldInput::Create<bool>("sharp_face");
  params.set_output("Smooth", fn::invert_boolean_field(shade_smooth_field));
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_INPUT_SHADE_SMOOTH, "Is Shade Smooth", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_shade_smooth_cc
