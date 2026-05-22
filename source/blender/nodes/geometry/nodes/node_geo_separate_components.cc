/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_separate_components_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry"_ustr)
      .description("Geometry to split into separate components");
  b.add_output<decl::Geometry>("Mesh"_ustr).propagate_all_geometry();
  b.add_output<decl::Geometry>("Curve"_ustr).propagate_all_geometry();
  b.add_output<decl::Geometry>("Grease Pencil"_ustr).propagate_all_geometry();
  b.add_output<decl::Geometry>("Point Cloud"_ustr).propagate_all_geometry();
  b.add_output<decl::Geometry>("Volume"_ustr)
      .translation_context(BLT_I18NCONTEXT_ID_ID)
      .propagate_all_geometry();
  b.add_output<decl::Geometry>("Instances"_ustr).propagate_all_geometry();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry"_ustr);

  GeometrySet meshes;
  GeometrySet curves;
  GeometrySet grease_pencil;
  GeometrySet pointclouds;
  GeometrySet volumes;
  GeometrySet instances;

  const StringRef name = geometry_set.name();
  if (!name.is_empty()) {
    meshes.set_name(name);
    curves.set_name(name);
    grease_pencil.set_name(name);
    pointclouds.set_name(name);
    volumes.set_name(name);
    instances.set_name(name);
  }

  meshes.copy_bundle_from(geometry_set);
  curves.copy_bundle_from(geometry_set);
  grease_pencil.copy_bundle_from(geometry_set);
  pointclouds.copy_bundle_from(geometry_set);
  volumes.copy_bundle_from(geometry_set);
  instances.copy_bundle_from(geometry_set);

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

  params.set_output("Mesh"_ustr, meshes);
  params.set_output("Curve"_ustr, curves);
  params.set_output("Grease Pencil"_ustr, grease_pencil);
  params.set_output("Point Cloud"_ustr, pointclouds);
  params.set_output("Volume"_ustr, volumes);
  params.set_output("Instances"_ustr, instances);
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeSeparateComponents"_ustr, GEO_NODE_SEPARATE_COMPONENTS);
  ntype.ui_name = "Separate Components";
  ntype.ui_description =
      "Split a geometry into a separate output for each type of data in the geometry";
  ntype.enum_name_legacy = "SEPARATE_COMPONENTS";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.default_width = bke::NodeWidth::_160;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_separate_components_cc
