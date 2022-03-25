/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_spline_resolution_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Int>(N_("Resolution")).field_source();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<int> resolution_field = AttributeFieldInput::Create<int>("resolution");
  params.set_output("Resolution", std::move(resolution_field));
}

}  // namespace blender::nodes::node_geo_input_spline_resolution_cc

void register_node_type_geo_input_spline_resolution()
{
  namespace file_ns = blender::nodes::node_geo_input_spline_resolution_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_INPUT_SPLINE_RESOLUTION, "Spline Resolution", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
