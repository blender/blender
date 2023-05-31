/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_separate_components_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry");
  b.add_output<decl::Geometry>("Mesh").propagate_all();
  b.add_output<decl::Geometry>("Curve").propagate_all();
  b.add_output<decl::Geometry>("Point Cloud").propagate_all();
  b.add_output<decl::Geometry>("Volume")
      .translation_context(BLT_I18NCONTEXT_ID_ID)
      .propagate_all();
  b.add_output<decl::Geometry>("Instances").propagate_all();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  GeometrySet meshes;
  GeometrySet curves;
  GeometrySet point_clouds;
  GeometrySet volumes;
  GeometrySet instances;

  if (geometry_set.has<MeshComponent>()) {
    meshes.add(*geometry_set.get_component_for_read<MeshComponent>());
  }
  if (geometry_set.has<CurveComponent>()) {
    curves.add(*geometry_set.get_component_for_read<CurveComponent>());
  }
  if (geometry_set.has<PointCloudComponent>()) {
    point_clouds.add(*geometry_set.get_component_for_read<PointCloudComponent>());
  }
  if (geometry_set.has<VolumeComponent>()) {
    volumes.add(*geometry_set.get_component_for_read<VolumeComponent>());
  }
  if (geometry_set.has<InstancesComponent>()) {
    instances.add(*geometry_set.get_component_for_read<InstancesComponent>());
  }

  params.set_output("Mesh", meshes);
  params.set_output("Curve", curves);
  params.set_output("Point Cloud", point_clouds);
  params.set_output("Volume", volumes);
  params.set_output("Instances", instances);
}

}  // namespace blender::nodes::node_geo_separate_components_cc

void register_node_type_geo_separate_components()
{
  namespace file_ns = blender::nodes::node_geo_separate_components_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_SEPARATE_COMPONENTS, "Separate Components", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
