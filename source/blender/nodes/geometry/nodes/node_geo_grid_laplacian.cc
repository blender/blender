/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_volume_grid.hh"

#include "node_geometry_util.hh"

#ifdef WITH_OPENVDB
#  include "openvdb/tools/GridOperators.h"
#endif

namespace blender::nodes::node_geo_grid_laplacian_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Grid").hide_value().structure_type(StructureType::Grid);
  b.add_output<decl::Float>("Laplacian").structure_type(StructureType::Grid);
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
  const openvdb::FloatGrid &vdb_grid = grid.grid(tree_token);
  openvdb::FloatGrid::Ptr laplacian_vdb_grid = openvdb::tools::laplacian(vdb_grid);
  params.set_output("Laplacian", bke::VolumeGrid<float>(std::move(laplacian_vdb_grid)));
#else
  node_geo_exec_with_missing_openvdb(params);
#endif
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeGridLaplacian");
  ntype.ui_name = "Grid Laplacian";
  ntype.ui_description = "Compute the divergence of the gradient of the input grid";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_grid_laplacian_cc
