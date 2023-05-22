/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
#  include <openvdb/tools/Interpolation.h>
#  include <openvdb/tools/PointScatter.h>
#endif

#include "DNA_node_types.h"
#include "DNA_pointcloud_types.h"

#include "BKE_pointcloud.h"
#include "BKE_volume.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "DEG_depsgraph_query.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_distribute_points_in_volume_cc {

NODE_STORAGE_FUNCS(NodeGeometryDistributePointsInVolume)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Volume")
      .supported_type(GEO_COMPONENT_TYPE_VOLUME)
      .translation_context(BLT_I18NCONTEXT_ID_ID);
  b.add_input<decl::Float>("Density")
      .default_value(1.0f)
      .min(0.0f)
      .max(100000.0f)
      .subtype(PROP_NONE)
      .description("Number of points to sample per unit volume");
  b.add_input<decl::Int>("Seed").min(-10000).max(10000).description(
      "Seed used by the random number generator to generate random points");
  b.add_input<decl::Vector>("Spacing")
      .default_value({0.3, 0.3, 0.3})
      .min(0.0001f)
      .subtype(PROP_XYZ)
      .description("Spacing between grid points");
  b.add_input<decl::Float>("Threshold")
      .default_value(0.1f)
      .min(0.0f)
      .max(FLT_MAX)
      .description("Minimum density of a volume cell to contain a grid point");
  b.add_output<decl::Geometry>("Points").propagate_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", 0, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryDistributePointsInVolume *data = MEM_cnew<NodeGeometryDistributePointsInVolume>(
      __func__);
  data->mode = GEO_NODE_DISTRIBUTE_POINTS_IN_VOLUME_DENSITY_RANDOM;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const NodeGeometryDistributePointsInVolume &storage = node_storage(*node);
  GeometryNodeDistributePointsInVolumeMode mode = GeometryNodeDistributePointsInVolumeMode(
      storage.mode);

  bNodeSocket *sock_density = static_cast<bNodeSocket *>(node->inputs.first)->next;
  bNodeSocket *sock_seed = sock_density->next;
  bNodeSocket *sock_spacing = sock_seed->next;
  bNodeSocket *sock_threshold = sock_spacing->next;

  bke::nodeSetSocketAvailability(
      ntree, sock_density, mode == GEO_NODE_DISTRIBUTE_POINTS_IN_VOLUME_DENSITY_RANDOM);
  bke::nodeSetSocketAvailability(
      ntree, sock_seed, mode == GEO_NODE_DISTRIBUTE_POINTS_IN_VOLUME_DENSITY_RANDOM);
  bke::nodeSetSocketAvailability(
      ntree, sock_spacing, mode == GEO_NODE_DISTRIBUTE_POINTS_IN_VOLUME_DENSITY_GRID);
  bke::nodeSetSocketAvailability(
      ntree, sock_threshold, mode == GEO_NODE_DISTRIBUTE_POINTS_IN_VOLUME_DENSITY_GRID);
}

#ifdef WITH_OPENVDB
/* Implements the interface required by #openvdb::tools::NonUniformPointScatter. */
class PositionsVDBWrapper {
 private:
  float3 offset_fix_;
  Vector<float3> &vector_;

 public:
  PositionsVDBWrapper(Vector<float3> &vector, const float3 offset_fix)
      : offset_fix_(offset_fix), vector_(vector)
  {
  }
  PositionsVDBWrapper(const PositionsVDBWrapper &wrapper) = default;

  void add(const openvdb::Vec3R &pos)
  {
    vector_.append(float3(float(pos[0]), float(pos[1]), float(pos[2])) + offset_fix_);
  }
};

/* Use #std::mt19937 as a random number generator,
 * it has a very long period and thus there should be no visible patterns in the generated points.
 */
using RNGType = std::mt19937;
/* Non-uniform scatter allows the amount of points to be scaled with the volume's density. */
using NonUniformPointScatterVDB =
    openvdb::tools::NonUniformPointScatter<PositionsVDBWrapper, RNGType>;

static void point_scatter_density_random(const openvdb::FloatGrid &grid,
                                         const float density,
                                         const int seed,
                                         Vector<float3> &r_positions)
{
  /* Offset points by half a voxel so that grid points are aligned with world grid points. */
  const float3 offset_fix = {0.5f * float(grid.voxelSize().x()),
                             0.5f * float(grid.voxelSize().y()),
                             0.5f * float(grid.voxelSize().z())};
  /* Setup and call into OpenVDB's point scatter API. */
  PositionsVDBWrapper vdb_position_wrapper = PositionsVDBWrapper(r_positions, offset_fix);
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
  const double min_spacing = std::min(voxel_spacing.x(),
                                      std::min(voxel_spacing.y(), voxel_spacing.z()));
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
          const openvdb::Vec3d local_pos = grid.indexToWorld(idx_pos + half_voxel);
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
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Volume");

  const NodeGeometryDistributePointsInVolume &storage = node_storage(params.node());
  const GeometryNodeDistributePointsInVolumeMode mode = GeometryNodeDistributePointsInVolumeMode(
      storage.mode);

  float density;
  int seed;
  float3 spacing{0, 0, 0};
  float threshold;
  if (mode == GEO_NODE_DISTRIBUTE_POINTS_IN_VOLUME_DENSITY_RANDOM) {
    density = params.extract_input<float>("Density");
    seed = params.extract_input<int>("Seed");
  }
  else if (mode == GEO_NODE_DISTRIBUTE_POINTS_IN_VOLUME_DENSITY_GRID) {
    spacing = params.extract_input<float3>("Spacing");
    threshold = params.extract_input<float>("Threshold");
  }

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (!geometry_set.has_volume()) {
      geometry_set.keep_only({GEO_COMPONENT_TYPE_POINT_CLOUD, GEO_COMPONENT_TYPE_INSTANCES});
      return;
    }
    const VolumeComponent *component = geometry_set.get_component_for_read<VolumeComponent>();
    const Volume *volume = component->get_for_read();
    BKE_volume_load(volume, DEG_get_bmain(params.depsgraph()));

    Vector<float3> positions;

    for (const int i : IndexRange(BKE_volume_num_grids(volume))) {
      const VolumeGrid *volume_grid = BKE_volume_grid_get_for_read(volume, i);
      if (volume_grid == nullptr) {
        continue;
      }

      openvdb::GridBase::ConstPtr base_grid = BKE_volume_grid_openvdb_for_read(volume,
                                                                               volume_grid);
      if (!base_grid) {
        continue;
      }

      if (!base_grid->isType<openvdb::FloatGrid>()) {
        continue;
      }

      const openvdb::FloatGrid::ConstPtr grid = openvdb::gridConstPtrCast<openvdb::FloatGrid>(
          base_grid);

      if (mode == GEO_NODE_DISTRIBUTE_POINTS_IN_VOLUME_DENSITY_RANDOM) {
        point_scatter_density_random(*grid, density, seed, positions);
      }
      else if (mode == GEO_NODE_DISTRIBUTE_POINTS_IN_VOLUME_DENSITY_GRID) {
        point_scatter_density_grid(*grid, spacing, threshold, positions);
      }
    }

    PointCloud *pointcloud = BKE_pointcloud_new_nomain(positions.size());
    bke::MutableAttributeAccessor point_attributes = pointcloud->attributes_for_write();
    pointcloud->positions_for_write().copy_from(positions);
    bke::SpanAttributeWriter<float> point_radii =
        point_attributes.lookup_or_add_for_write_only_span<float>("radius", ATTR_DOMAIN_POINT);

    point_radii.span.fill(0.05f);
    point_radii.finish();

    geometry_set.replace_pointcloud(pointcloud);
    geometry_set.keep_only_during_modify({GEO_COMPONENT_TYPE_POINT_CLOUD});
  });

  params.set_output("Points", std::move(geometry_set));

#else
  params.set_default_remaining_outputs();
  params.error_message_add(NodeWarningType::Error,
                           TIP_("Disabled, Blender was compiled without OpenVDB"));
#endif
}
}  // namespace blender::nodes::node_geo_distribute_points_in_volume_cc

void register_node_type_geo_distribute_points_in_volume()
{
  namespace file_ns = blender::nodes::node_geo_distribute_points_in_volume_cc;

  static bNodeType ntype;
  geo_node_type_base(&ntype,
                     GEO_NODE_DISTRIBUTE_POINTS_IN_VOLUME,
                     "Distribute Points in Volume",
                     NODE_CLASS_GEOMETRY);
  node_type_storage(&ntype,
                    "NodeGeometryDistributePointsInVolume",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  ntype.initfunc = file_ns::node_init;
  ntype.updatefunc = file_ns::node_update;
  blender::bke::node_type_size(&ntype, 170, 100, 320);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}
