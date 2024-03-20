/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_separate_components_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry");
  b.add_output<decl::Geometry>("Mesh").propagate_all();
  b.add_output<decl::Geometry>("Curve").propagate_all();
  b.add_output<decl::Geometry>("Grease Pencil").propagate_all();
  b.add_output<decl::Geometry>("Point Cloud").propagate_all();
  b.add_output<decl::Geometry>("Volume")
      .translation_context(BLT_I18NCONTEXT_ID_ID)
      .propagate_all();
  b.add_output<decl::Geometry>("Instances").propagate_all();
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  LISTBASE_FOREACH (bNodeSocket *, socket, &node->outputs) {
    if (STREQ(socket->identifier, "Grease Pencil")) {
      bke::nodeSetSocketAvailability(ntree, socket, U.experimental.use_grease_pencil_version3);
      break;
    }
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  GeometrySet meshes;
  GeometrySet curves;
  GeometrySet grease_pencil;
  GeometrySet point_clouds;
  GeometrySet volumes;
  GeometrySet instances;

  if (geometry_set.has<MeshComponent>()) {
    meshes.add(*geometry_set.get_component<MeshComponent>());
  }
  if (geometry_set.has<CurveComponent>()) {
    curves.add(*geometry_set.get_component<CurveComponent>());
  }
  if (geometry_set.has<GreasePencilComponent>() && U.experimental.use_grease_pencil_version3) {
    grease_pencil.add(*geometry_set.get_component<GreasePencilComponent>());
  }
  if (geometry_set.has<PointCloudComponent>()) {
    point_clouds.add(*geometry_set.get_component<PointCloudComponent>());
  }
  if (geometry_set.has<VolumeComponent>()) {
    volumes.add(*geometry_set.get_component<VolumeComponent>());
  }
  if (geometry_set.has<InstancesComponent>()) {
    instances.add(*geometry_set.get_component<InstancesComponent>());
  }

  params.set_output("Mesh", meshes);
  params.set_output("Curve", curves);
  if (U.experimental.use_grease_pencil_version3) {
    params.set_output("Grease Pencil", grease_pencil);
  }
  params.set_output("Point Cloud", point_clouds);
  params.set_output("Volume", volumes);
  params.set_output("Instances", instances);
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_SEPARATE_COMPONENTS, "Separate Components", NODE_CLASS_GEOMETRY);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.updatefunc = node_update;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_separate_components_cc
