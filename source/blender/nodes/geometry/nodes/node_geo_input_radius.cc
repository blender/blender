/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_radius_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>("Radius").default_value(1.0f).min(0.0f).field_source();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<float> radius_field = AttributeFieldInput::Create<float>("radius");
  params.set_output("Radius", std::move(radius_field));
}

}  // namespace blender::nodes::node_geo_input_radius_cc

void register_node_type_geo_input_radius()
{
  namespace file_ns = blender::nodes::node_geo_input_radius_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_INPUT_RADIUS, "Radius", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
