/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
#  include <openvdb/tools/Interpolation.h>
#  include <openvdb/tools/PointScatter.h>

#  include <algorithm>
#endif

#include "DNA_node_types.h"
#include "DNA_pointcloud_types.h"

#include "BKE_pointcloud.hh"
#include "BKE_volume_grid.hh"

#include "NOD_rna_define.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "GEO_randomize.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_distribute_points_in_grid_cc {

enum class DistributeMode {
  Random = 0,
  Grid = 1,
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Grid").hide_value().structure_type(StructureType::Grid);
  auto &density = b.add_input<decl::Float>("Density")
                      .default_value(1.0f)
                      .min(0.0f)
                      .max(100000.0f)
                      .subtype(PROP_NONE)
                      .description(
                          "When combined with each voxel's value, determines the number of points "
                          "to sample per unit volume");
  auto &seed = b.add_input<decl::Int>("Seed").min(-10000).max(10000).description(
      "Seed used by the random number generator to generate random points");
  auto &spacing = b.add_input<decl::Vector>("Spacing")
                      .default_value({0.3, 0.3, 0.3})
                      .min(0.0001f)
                      .subtype(PROP_XYZ)
                      .description("Spacing between grid points");
  auto &threshold = b.add_input<decl::Float>("Threshold")
                        .default_value(0.1f)
                        .min(0.0f)
                        .max(FLT_MAX)
                        .description("Minimum density of a voxel to contain a grid point");
  b.add_output<decl::Geometry>("Points").propagate_all();

  const bNode *node = b.node_or_null();
  if (node != nullptr) {
    const auto mode = DistributeMode(node->custom1);

    density.available(mode == DistributeMode::Random);
    seed.available(mode == DistributeMode::Random);
    spacing.available(mode == DistributeMode::Grid);
    threshold.available(mode == DistributeMode::Grid);
  }
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "mode", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = int16_t(DistributeMode::Random);
}

#ifdef WITH_OPENVDB
/* Implements the interface required by #openvdb::tools::NonUniformPointScatter. */
class PositionsVDBWrapper {
 private:
  Vector<float3> &vector_;

 public:
  PositionsVDBWrapper(Vector<float3> &vector) : vector_(vector) {}
  PositionsVDBWrapper(const PositionsVDBWrapper &wrapper) = default;

  void add(const openvdb::Vec3R &pos)
  {
    vector_.append(float3(float(pos[0]), float(pos[1]), float(pos[2])));
  }
};

/* Use #std::mt19937 as a random number generator. It has a very long period and thus there should
 * be no visible patterns in the generated points. */
using RNGType = std::mt19937;
/* Non-uniform scatter allows the amount of points to be scaled with the volume's density. */
using NonUniformPointScatterVDB =
    openvdb::tools::NonUniformPointScatter<PositionsVDBWrapper, RNGType>;

static void point_scatter_density_random(const openvdb::FloatGrid &grid,
                                         const float density,
                                         const int seed,
                                         Vector<float3> &r_positions)
{
  /* Setup and call into OpenVDB's point scatter API. */
  PositionsVDBWrapper vdb_position_wrapper(r_positions);
  RNGType random_generator(seed);
  NonUniformPointScatterVDB point_scatter(vdb_position_wrapper, density, random_generator);
  point_scatter(grid);
}

static void point_scatter_density_grid(const openvdb::FloatGrid &grid,
                                       const float3 spacing,
                                       const float threshold,
                                       Vector<float3> &r_positions)
{
  const openvdb::Vec3d half_voxel(0.5, 0.5, 0.5);
  const openvdb::Vec3d voxel_spacing(double(spacing.x) / grid.voxelSize().x(),
                                     double(spacing.y) / grid.voxelSize().y(),
                                     double(spacing.z) / grid.voxelSize().z());

  /* Abort if spacing is zero. */
  const double min_spacing = std::min({voxel_spacing.x(), voxel_spacing.y(), voxel_spacing.z()});
  if (std::abs(min_spacing) < 0.0001) {
    return;
  }

  /* Iterate through tiles and voxels on the grid. */
  for (openvdb::FloatGrid::ValueOnCIter cell = grid.cbeginValueOn(); cell; ++cell) {
    /* Check if the cell's value meets the minimum threshold. */
    if (cell.getValue() < threshold) {
      continue;
    }
    /* Compute the bounding box of each tile/voxel. */
    const openvdb::CoordBBox bbox = cell.getBoundingBox();
    const openvdb::Vec3d box_min = bbox.min().asVec3d() - half_voxel;
    const openvdb::Vec3d box_max = bbox.max().asVec3d() + half_voxel;

    /* Pick a starting point rounded up to the nearest possible point. */
    double abs_spacing_x = std::abs(voxel_spacing.x());
    double abs_spacing_y = std::abs(voxel_spacing.y());
    double abs_spacing_z = std::abs(voxel_spacing.z());
    const openvdb::Vec3d start(ceil(box_min.x() / abs_spacing_x) * abs_spacing_x,
                               ceil(box_min.y() / abs_spacing_y) * abs_spacing_y,
                               ceil(box_min.z() / abs_spacing_z) * abs_spacing_z);

    /* Iterate through all possible points in box. */
    for (double x = start.x(); x < box_max.x(); x += abs_spacing_x) {
      for (double y = start.y(); y < box_max.y(); y += abs_spacing_y) {
        for (double z = start.z(); z < box_max.z(); z += abs_spacing_z) {
          /* Transform with grid matrix and add point. */
          const openvdb::Vec3d idx_pos(x, y, z);
          const openvdb::Vec3d local_pos = grid.indexToWorld(idx_pos);
          r_positions.append({float(local_pos.x()), float(local_pos.y()), float(local_pos.z())});
        }
      }
    }
  }
}

#endif /* WITH_OPENVDB */

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  const bke::VolumeGrid<float> volume_grid = params.extract_input<bke::VolumeGrid<float>>("Grid");
  if (!volume_grid) {
    params.set_default_remaining_outputs();
    return;
  }

  bke::VolumeTreeAccessToken tree_token;
  const openvdb::GridBase &base_grid = volume_grid.grid(tree_token);
  if (!base_grid.isType<openvdb::FloatGrid>()) {
    params.set_default_remaining_outputs();
    return;
  }
  const openvdb::FloatGrid &grid = static_cast<const openvdb::FloatGrid &>(base_grid);

  const DistributeMode mode = DistributeMode(params.node().custom1);

  float density;
  int seed;
  float3 spacing{0, 0, 0};
  float threshold;
  if (mode == DistributeMode::Random) {
    density = params.extract_input<float>("Density");
    seed = params.extract_input<int>("Seed");
  }
  else if (mode == DistributeMode::Grid) {
    spacing = params.extract_input<float3>("Spacing");
    threshold = params.extract_input<float>("Threshold");
  }

  Vector<float3> positions;
  switch (mode) {
    case DistributeMode::Random:
      point_scatter_density_random(grid, density, seed, positions);
      break;
    case DistributeMode::Grid:
      point_scatter_density_grid(grid, spacing, threshold, positions);
      break;
  }

  PointCloud *pointcloud = BKE_pointcloud_new_nomain(positions.size());
  pointcloud->positions_for_write().copy_from(positions);

  geometry::debug_randomize_point_order(pointcloud);

  params.set_output("Points", GeometrySet::from_pointcloud(pointcloud));
#else
  node_geo_exec_with_missing_openvdb(params);
#endif
}

static void node_rna(StructRNA *srna)
{
  static const EnumPropertyItem mode_items[] = {
      {int(DistributeMode::Random),
       "DENSITY_RANDOM",
       0,
       "Random",
       "Distribute points randomly inside of the volume"},
      {int(DistributeMode::Grid),
       "DENSITY_GRID",
       0,
       "Grid",
       "Distribute the points in a grid pattern inside of the volume"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_node_enum(srna,
                    "mode",
                    "Distribution Method",
                    "Method to use for scattering points",
                    mode_items,
                    NOD_inline_enum_accessors(custom1),
                    int(DistributeMode::Random));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(
      &ntype, "GeometryNodeDistributePointsInGrid", GEO_NODE_DISTRIBUTE_POINTS_IN_GRID);
  ntype.ui_name = "Distribute Points in Grid";
  ntype.ui_description = "Generate points inside a volume grid";
  ntype.enum_name_legacy = "DISTRIBUTE_POINTS_IN_GRID";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.initfunc = node_init;
  blender::bke::node_type_size(ntype, 170, 100, 320);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  blender::bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_distribute_points_in_grid_cc
