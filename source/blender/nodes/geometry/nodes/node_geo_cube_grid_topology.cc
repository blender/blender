/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_volume.hh"
#include "BKE_volume_grid.hh"

#include "NOD_socket_search_link.hh"

#include "node_geometry_util.hh"
#include <cstdint>

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
#endif

namespace blender::nodes::node_geo_cube_grid_topology_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.add_output<decl::Bool>("Topology")
      .structure_type(StructureType::Grid)
      .description("Boolean grid defining the topology/active regions");
  b.add_input<decl::Vector>("Bounds Min")
      .default_value(float3(-1.0f))
      .description("Minimum boundary of the grid (world space)");
  b.add_input<decl::Vector>("Bounds Max")
      .default_value(float3(1.0f))
      .description("Maximum boundary of the grid (world space)");

  b.add_input<decl::Int>("Resolution X")
      .default_value(32)
      .min(1)
      .description("Number of voxels in the X axis");
  b.add_input<decl::Int>("Resolution Y")
      .default_value(32)
      .min(1)
      .description("Number of voxels in the Y axis");
  b.add_input<decl::Int>("Resolution Z")
      .default_value(32)
      .min(1)
      .description("Number of voxels in the Z axis");

  PanelDeclarationBuilder &min_panel = b.add_panel("Min").default_closed(true);
  min_panel.add_input<decl::Int>("Min X").default_value(0).description(
      "Minimum coordinate in X axis (grid index space)");
  min_panel.add_input<decl::Int>("Min Y").default_value(0).description(
      "Minimum coordinate in Y axis (grid index space)");
  min_panel.add_input<decl::Int>("Min Z").default_value(0).description(
      "Minimum coordinate in Z axis (grid index space)");
}

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  using type_traits = typename bke::VolumeGridTraits<bool>;
  using TreeType = typename type_traits::TreeType;
  using GridType = openvdb::Grid<TreeType>;

  auto openvdb_grid = GridType::create(false /* background */);

  const int3 grid_min_inclusive = int3(params.extract_input<int>("Min X"),
                                       params.extract_input<int>("Min Y"),
                                       params.extract_input<int>("Min Z"));
  const int3 resolution = int3(params.extract_input<int>("Resolution X"),
                               params.extract_input<int>("Resolution Y"),
                               params.extract_input<int>("Resolution Z"));

  const float3 bounds_min = params.extract_input<float3>("Bounds Min");
  const float3 bounds_max = params.extract_input<float3>("Bounds Max");

  if (resolution.x <= 0 || resolution.y <= 0 || resolution.z <= 0) {
    params.error_message_add(NodeWarningType::Warning, TIP_("Resolution must be positive"));
    params.set_default_remaining_outputs();
    return;
  }

  if (bounds_min.x >= bounds_max.x || bounds_min.y >= bounds_max.y || bounds_min.z >= bounds_max.z)
  {
    params.error_message_add(NodeWarningType::Error,
                             TIP_("Bounding box volume must be greater than 0"));
    params.set_default_remaining_outputs();
    return;
  }

  const int3 grid_max_inclusive = grid_min_inclusive + resolution - 1;
  const openvdb::math::CoordBBox bbox(
      {grid_min_inclusive.x, grid_min_inclusive.y, grid_min_inclusive.z},
      {grid_max_inclusive.x, grid_max_inclusive.y, grid_max_inclusive.z});
  openvdb_grid->tree().denseFill(bbox, true, /*active=*/true);

  const double3 scale_fac = double3(bounds_max - bounds_min) / double3(resolution);
  if (!BKE_volume_voxel_size_valid(float3(scale_fac))) {
    params.error_message_add(NodeWarningType::Warning,
                             TIP_("Volume scale is lower than permitted by OpenVDB"));
    params.set_default_remaining_outputs();
    return;
  }

  openvdb_grid->transform().postScale(openvdb::math::Vec3d(scale_fac.x, scale_fac.y, scale_fac.z));
  const double3 translation = double3(bounds_min) - scale_fac * double3(grid_min_inclusive);
  openvdb_grid->transform().postTranslate(
      openvdb::math::Vec3d(translation.x, translation.y, translation.z));

  bke::VolumeGrid<bool> topology_grid(std::move(openvdb_grid));
  params.set_output("Topology", bke::GVolumeGrid(std::move(topology_grid)));
#else
  node_geo_exec_with_missing_openvdb(params);
#endif
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeCubeGridTopology");
  ntype.ui_name = "Cube Grid Topology";
  ntype.ui_description =
      "Create a boolean grid topology with the given dimensions, for use with the Field to Grid "
      "node";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_cube_grid_topology_cc
