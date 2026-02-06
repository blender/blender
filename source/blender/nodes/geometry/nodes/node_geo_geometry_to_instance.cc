/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_instances.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_geometry_to_instance_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry")
      .multi_input()
      .description("Each input geometry is turned into a separate instance");
  b.add_output<decl::Geometry>("Instances").propagate_all();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeoNodesMultiInput<GeometrySet> geometries =
      params.extract_input<GeoNodesMultiInput<GeometrySet>>("Geometry");
  auto instances = std::make_unique<bke::Instances>(geometries.values.size());

  MutableSpan<int> handles = instances->reference_handles_for_write();

  for (const int i : geometries.values.index_range()) {
    GeometrySet &geometry = geometries.values[i];
    geometry.ensure_owns_direct_data();
    handles[i] = instances->add_reference(std::move(geometry));
  }

  instances->transforms_for_write().fill(float4x4::identity());

  params.set_output("Instances", GeometrySet::from_instances(std::move(instances)));
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeGeometryToInstance", GEO_NODE_GEOMETRY_TO_INSTANCE);
  ntype.ui_name = "Geometry to Instance";
  ntype.ui_description =
      "Convert each input geometry into an instance, which can be much faster than the Join "
      "Geometry node when the inputs are large";
  ntype.enum_name_legacy = "GEOMETRY_TO_INSTANCE";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  bke::node_type_size(ntype, 160, 100, 300);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_geometry_to_instance_cc
