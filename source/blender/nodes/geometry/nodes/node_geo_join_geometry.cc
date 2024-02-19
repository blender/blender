/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "GEO_join_geometries.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_join_geometry_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry").multi_input();
  b.add_output<decl::Geometry>("Geometry").propagate_all();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Vector<GeometrySet> geometry_sets = params.extract_input<Vector<GeometrySet>>("Geometry");

  const AnonymousAttributePropagationInfo &propagation_info = params.get_output_propagation_info(
      "Geometry");

  for (GeometrySet &geometry : geometry_sets) {
    GeometryComponentEditData::remember_deformed_positions_if_necessary(geometry);
  }

  GeometrySet geometry_set_result = geometry::join_geometries(geometry_sets, propagation_info);

  params.set_output("Geometry", std::move(geometry_set_result));
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_JOIN_GEOMETRY, "Join Geometry", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_join_geometry_cc
