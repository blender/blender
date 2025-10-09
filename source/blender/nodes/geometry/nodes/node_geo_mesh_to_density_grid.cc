/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"

#include "BKE_volume_grid.hh"

#include "GEO_mesh_to_volume.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_mesh_to_density_grid_cc {

NODE_STORAGE_FUNCS(NodeGeometryMeshToVolume)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Mesh")
      .supported_type(GeometryComponent::Type::Mesh)
      .description("Mesh whose inner volume is converted to a density grid");
  b.add_input<decl::Float>("Density").default_value(1.0f).min(0.01f).max(FLT_MAX);
  b.add_input<decl::Float>("Voxel Size")
      .default_value(0.3f)
      .min(0.01f)
      .max(FLT_MAX)
      .subtype(PROP_DISTANCE);
  b.add_input<decl::Float>("Gradient Width")
      .default_value(0.2f)
      .min(0.0001f)
      .max(FLT_MAX)
      .subtype(PROP_DISTANCE)
      .description("Width of the gradient inside of the mesh");
  b.add_output<decl::Float>("Density Grid").structure_type(StructureType::Grid);
}

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  const GeometrySet geometry_set = params.extract_input<GeometrySet>("Mesh");
  const Mesh *mesh = geometry_set.get_mesh();
  if (!mesh || mesh->faces_num == 0) {
    params.set_default_remaining_outputs();
    return;
  }
  bke::VolumeGrid<float> grid = geometry::mesh_to_density_grid(
      mesh->vert_positions(),
      mesh->corner_verts(),
      mesh->corner_tris(),
      params.extract_input<float>("Voxel Size"),
      params.extract_input<float>("Gradient Width"),
      params.extract_input<float>("Density"));
  if (!grid) {
    params.set_default_remaining_outputs();
    return;
  }
  params.set_output("Density Grid", std::move(grid));
#else
  node_geo_exec_with_missing_openvdb(params);
#endif
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeMeshToDensityGrid", GEO_NODE_MESH_TO_DENSITY_GRID);
  ntype.ui_name = "Mesh to Density Grid";
  ntype.ui_description = "Create a filled volume grid from a mesh";
  ntype.enum_name_legacy = "MESH_TO_DENSITY_GRID";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_mesh_to_density_grid_cc
