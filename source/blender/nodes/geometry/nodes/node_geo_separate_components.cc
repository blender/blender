/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_separate_components_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry")
      .description("Geometry to split into separate components");
  b.add_output<decl::Geometry>("Mesh").propagate_all();
  b.add_output<decl::Geometry>("Curve").propagate_all();
  b.add_output<decl::Geometry>("Grease Pencil").propagate_all();
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
  GeometrySet grease_pencil;
  GeometrySet pointclouds;
  GeometrySet volumes;
  GeometrySet instances;

  const std::string &name = geometry_set.name;
  meshes.name = name;
  curves.name = name;
  grease_pencil.name = name;
  pointclouds.name = name;
  volumes.name = name;
  instances.name = name;

  if (geometry_set.has<MeshComponent>()) {
    meshes.add(*geometry_set.get_component<MeshComponent>());
  }
  if (geometry_set.has<CurveComponent>()) {
    curves.add(*geometry_set.get_component<CurveComponent>());
  }
  if (geometry_set.has<GreasePencilComponent>()) {
    grease_pencil.add(*geometry_set.get_component<GreasePencilComponent>());
  }
  if (geometry_set.has<PointCloudComponent>()) {
    pointclouds.add(*geometry_set.get_component<PointCloudComponent>());
  }
  if (geometry_set.has<VolumeComponent>()) {
    volumes.add(*geometry_set.get_component<VolumeComponent>());
  }
  if (geometry_set.has<InstancesComponent>()) {
    instances.add(*geometry_set.get_component<InstancesComponent>());
  }

  params.set_output("Mesh", meshes);
  params.set_output("Curve", curves);
  params.set_output("Grease Pencil", grease_pencil);
  params.set_output("Point Cloud", pointclouds);
  params.set_output("Volume", volumes);
  params.set_output("Instances", instances);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeSeparateComponents", GEO_NODE_SEPARATE_COMPONENTS);
  ntype.ui_name = "Separate Components";
  ntype.ui_description =
      "Split a geometry into a separate output for each type of data in the geometry";
  ntype.enum_name_legacy = "SEPARATE_COMPONENTS";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_separate_components_cc
