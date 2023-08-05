/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "GEO_realize_instances.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_geo_realize_instances_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry");
  b.add_output<decl::Geometry>("Geometry").propagate_all();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  GeometryComponentEditData::remember_deformed_curve_positions_if_necessary(geometry_set);
  geometry::RealizeInstancesOptions options;
  options.keep_original_ids = false;
  options.realize_instance_attributes = true;
  options.propagation_info = params.get_output_propagation_info("Geometry");
  geometry_set = geometry::realize_instances(geometry_set, options);
  params.set_output("Geometry", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_realize_instances_cc

void register_node_type_geo_realize_instances()
{
  namespace file_ns = blender::nodes::node_geo_realize_instances_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_REALIZE_INSTANCES, "Realize Instances", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
