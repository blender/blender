/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_spline_cyclic_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Bool>("Cyclic").field_source();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<bool> cyclic_field = AttributeFieldInput::Create<bool>("cyclic");
  params.set_output("Cyclic", std::move(cyclic_field));
}

}  // namespace blender::nodes::node_geo_input_spline_cyclic_cc

void register_node_type_geo_input_spline_cyclic()
{
  namespace file_ns = blender::nodes::node_geo_input_spline_cyclic_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_INPUT_SPLINE_CYCLIC, "Is Spline Cyclic", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
