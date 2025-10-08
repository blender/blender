/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"

#include "BKE_material.hh"
#include "BKE_volume_grid.hh"
#include "BKE_volume_to_mesh.hh"

#include "GEO_randomize.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_grid_to_mesh_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Grid").hide_value().structure_type(StructureType::Grid);
  b.add_input<decl::Float>("Threshold")
      .default_value(0.1f)
      .description("Values larger than the threshold are inside the generated mesh");
  b.add_input<decl::Float>("Adaptivity").min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_output<decl::Geometry>("Mesh");
}

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  const bke::VolumeGrid<float> grid = params.extract_input<bke::VolumeGrid<float>>("Grid");
  if (!grid) {
    params.set_default_remaining_outputs();
    return;
  }
  bke::VolumeTreeAccessToken tree_token;
  Mesh *mesh = bke::volume_grid_to_mesh(grid.get().grid(tree_token),
                                        params.extract_input<float>("Threshold"),
                                        params.extract_input<float>("Adaptivity"));
  BKE_id_material_eval_ensure_default_slot(&mesh->id);
  geometry::debug_randomize_mesh_order(mesh);
  params.set_output("Mesh", GeometrySet::from_mesh(mesh));
#else
  node_geo_exec_with_missing_openvdb(params);
#endif
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeGridToMesh", GEO_NODE_GRID_TO_MESH);
  ntype.ui_name = "Grid to Mesh";
  ntype.ui_description = "Generate a mesh on the \"surface\" of a volume grid";
  ntype.enum_name_legacy = "GRID_TO_MESH";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_grid_to_mesh_cc
