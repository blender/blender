/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_volume_grid.hh"

#include "node_geometry_util.hh"

#ifdef WITH_OPENVDB
#  include "openvdb/tools/LevelSetFilter.h"
#endif

namespace blender::nodes::node_geo_sdf_grid_median_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Float>("Grid").hide_value().structure_type(StructureType::Grid);
  b.add_output<decl::Float>("Grid").structure_type(StructureType::Grid).align_with_previous();
  b.add_input<decl::Int>("Width").default_value(1).min(0).description(
      "Filter kernel radius in voxels");
  b.add_input<decl::Int>("Iterations")
      .default_value(1)
      .min(0)
      .description("Number of iterations to apply the filter");
}

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  auto grid = params.extract_input<bke::VolumeGrid<float>>("Grid");
  if (!grid) {
    params.set_default_remaining_outputs();
    return;
  }

  const int iterations = params.extract_input<int>("Iterations");
  const int width = params.extract_input<int>("Width");
  if (iterations <= 0 || width <= 0) {
    params.set_output("Grid", std::move(grid));
    return;
  }

  bke::VolumeTreeAccessToken tree_token;
  openvdb::FloatGrid &vdb_grid = grid.grid_for_write(tree_token);

  try {
    openvdb::tools::LevelSetFilter<openvdb::FloatGrid> filter(vdb_grid);
    for (int i = 0; i < iterations; i++) {
      filter.median(width);
    }
  }
  catch (const openvdb::RuntimeError & /*e*/) {
    node_geo_sdf_grid_error_not_levelset(params);
    return;
  }

  params.set_output("Grid", std::move(grid));
#else
  node_geo_exec_with_missing_openvdb(params);
#endif
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeSDFGridMedian");
  ntype.ui_name = "SDF Grid Median";
  ntype.ui_description =
      "Apply median filter to a signed distance field. Reduces noise while preserving sharp "
      "features and edges in the distance field";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_sdf_grid_median_cc
