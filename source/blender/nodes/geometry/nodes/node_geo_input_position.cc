/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_position_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Vector>("Position").field_source();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<float3> position_field{AttributeFieldInput::Create<float3>("position")};
  params.set_output("Position", std::move(position_field));
}

}  // namespace blender::nodes::node_geo_input_position_cc

void register_node_type_geo_input_position()
{
  namespace file_ns = blender::nodes::node_geo_input_position_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_INPUT_POSITION, "Position", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
