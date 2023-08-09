/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_instances.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_geometry_to_instance_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry").multi_input();
  b.add_output<decl::Geometry>("Instances").propagate_all();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Vector<GeometrySet> geometries = params.extract_input<Vector<GeometrySet>>("Geometry");
  std::unique_ptr<bke::Instances> instances = std::make_unique<bke::Instances>();
  for (GeometrySet &geometry : geometries) {
    geometry.ensure_owns_direct_data();
    const int handle = instances->add_reference(std::move(geometry));
    instances->add_instance(handle, float4x4::identity());
  }
  params.set_output("Instances", GeometrySet::from_instances(instances.release()));
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_GEOMETRY_TO_INSTANCE, "Geometry to Instance", NODE_CLASS_GEOMETRY);
  blender::bke::node_type_size(&ntype, 160, 100, 300);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_geometry_to_instance_cc
