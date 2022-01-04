/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "BLI_task.hh"
#include "BLI_timeit.hh"

#include "DNA_mesh_types.h"

#include "BKE_bvhutils.h"
#include "BKE_geometry_set.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_proximity_cc {

NODE_STORAGE_FUNCS(NodeGeometryProximity)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Target"))
      .only_realized_data()
      .supported_type({GEO_COMPONENT_TYPE_MESH, GEO_COMPONENT_TYPE_POINT_CLOUD});
  b.add_input<decl::Vector>(N_("Source Position")).implicit_field();
  b.add_output<decl::Vector>(N_("Position")).dependent_field();
  b.add_output<decl::Float>(N_("Distance")).dependent_field();
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "target_element", 0, "", ICON_NONE);
}

static void geo_proximity_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryProximity *node_storage = MEM_cnew<NodeGeometryProximity>(__func__);
  node_storage->target_element = GEO_NODE_PROX_TARGET_FACES;
  node->storage = node_storage;
}

static bool calculate_mesh_proximity(const VArray<float3> &positions,
                                     const IndexMask mask,
                                     const Mesh &mesh,
                                     const GeometryNodeProximityTargetType type,
                                     const MutableSpan<float> r_distances,
                                     const MutableSpan<float3> r_locations)
{
  BVHTreeFromMesh bvh_data;
  switch (type) {
    case GEO_NODE_PROX_TARGET_POINTS:
      BKE_bvhtree_from_mesh_get(&bvh_data, &mesh, BVHTREE_FROM_VERTS, 2);
      break;
    case GEO_NODE_PROX_TARGET_EDGES:
      BKE_bvhtree_from_mesh_get(&bvh_data, &mesh, BVHTREE_FROM_EDGES, 2);
      break;
    case GEO_NODE_PROX_TARGET_FACES:
      BKE_bvhtree_from_mesh_get(&bvh_data, &mesh, BVHTREE_FROM_LOOPTRI, 2);
      break;
  }

  if (bvh_data.tree == nullptr) {
    return false;
  }

  threading::parallel_for(mask.index_range(), 512, [&](IndexRange range) {
    BVHTreeNearest nearest;
    copy_v3_fl(nearest.co, FLT_MAX);
    nearest.index = -1;

    for (int i : range) {
      const int index = mask[i];
      /* Use the distance to the last found point as upper bound to speedup the bvh lookup. */
      nearest.dist_sq = float3::distance_squared(nearest.co, positions[index]);

      BLI_bvhtree_find_nearest(
          bvh_data.tree, positions[index], &nearest, bvh_data.nearest_callback, &bvh_data);

      if (nearest.dist_sq < r_distances[index]) {
        r_distances[index] = nearest.dist_sq;
        if (!r_locations.is_empty()) {
          r_locations[index] = nearest.co;
        }
      }
    }
  });

  free_bvhtree_from_mesh(&bvh_data);
  return true;
}

static bool calculate_pointcloud_proximity(const VArray<float3> &positions,
                                           const IndexMask mask,
                                           const PointCloud &pointcloud,
                                           MutableSpan<float> r_distances,
                                           MutableSpan<float3> r_locations)
{
  BVHTreeFromPointCloud bvh_data;
  BKE_bvhtree_from_pointcloud_get(&bvh_data, &pointcloud, 2);
  if (bvh_data.tree == nullptr) {
    return false;
  }

  threading::parallel_for(mask.index_range(), 512, [&](IndexRange range) {
    BVHTreeNearest nearest;
    copy_v3_fl(nearest.co, FLT_MAX);
    nearest.index = -1;

    for (int i : range) {
      const int index = mask[i];
      /* Use the distance to the closest point in the mesh to speedup the pointcloud bvh lookup.
       * This is ok because we only need to find the closest point in the pointcloud if it's
       * closer than the mesh. */
      nearest.dist_sq = r_distances[index];

      BLI_bvhtree_find_nearest(
          bvh_data.tree, positions[index], &nearest, bvh_data.nearest_callback, &bvh_data);

      if (nearest.dist_sq < r_distances[index]) {
        r_distances[index] = nearest.dist_sq;
        if (!r_locations.is_empty()) {
          r_locations[index] = nearest.co;
        }
      }
    }
  });

  free_bvhtree_from_pointcloud(&bvh_data);
  return true;
}

class ProximityFunction : public fn::MultiFunction {
 private:
  GeometrySet target_;
  GeometryNodeProximityTargetType type_;

 public:
  ProximityFunction(GeometrySet target, GeometryNodeProximityTargetType type)
      : target_(std::move(target)), type_(type)
  {
    static fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static fn::MFSignature create_signature()
  {
    blender::fn::MFSignatureBuilder signature{"Geometry Proximity"};
    signature.single_input<float3>("Source Position");
    signature.single_output<float3>("Position");
    signature.single_output<float>("Distance");
    return signature.build();
  }

  void call(IndexMask mask, fn::MFParams params, fn::MFContext UNUSED(context)) const override
  {
    const VArray<float3> &src_positions = params.readonly_single_input<float3>(0,
                                                                               "Source Position");
    MutableSpan<float3> positions = params.uninitialized_single_output_if_required<float3>(
        1, "Position");
    /* Make sure there is a distance array, used for finding the smaller distance when there are
     * multiple components. Theoretically it would be possible to avoid using the distance array
     * when there is only one component. However, this only adds an allocation and a single float
     * comparison per vertex, so it's likely not worth it. */
    MutableSpan<float> distances = params.uninitialized_single_output<float>(2, "Distance");

    distances.fill_indices(mask, FLT_MAX);

    bool success = false;
    if (target_.has_mesh()) {
      success |= calculate_mesh_proximity(
          src_positions, mask, *target_.get_mesh_for_read(), type_, distances, positions);
    }

    if (target_.has_pointcloud() && type_ == GEO_NODE_PROX_TARGET_POINTS) {
      success |= calculate_pointcloud_proximity(
          src_positions, mask, *target_.get_pointcloud_for_read(), distances, positions);
    }

    if (!success) {
      positions.fill_indices(mask, float3(0));
      distances.fill_indices(mask, 0.0f);
      return;
    }

    if (params.single_output_is_required(2, "Distance")) {
      threading::parallel_for(mask.index_range(), 2048, [&](IndexRange range) {
        for (const int i : range) {
          const int j = mask[i];
          distances[j] = std::sqrt(distances[j]);
        }
      });
    }
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set_target = params.extract_input<GeometrySet>("Target");
  geometry_set_target.ensure_owns_direct_data();

  if (!geometry_set_target.has_mesh() && !geometry_set_target.has_pointcloud()) {
    params.set_default_remaining_outputs();
    return;
  }

  const NodeGeometryProximity &storage = node_storage(params.node());
  Field<float3> position_field = params.extract_input<Field<float3>>("Source Position");

  auto proximity_fn = std::make_unique<ProximityFunction>(
      std::move(geometry_set_target),
      static_cast<GeometryNodeProximityTargetType>(storage.target_element));
  auto proximity_op = std::make_shared<FieldOperation>(
      FieldOperation(std::move(proximity_fn), {std::move(position_field)}));

  params.set_output("Position", Field<float3>(proximity_op, 0));
  params.set_output("Distance", Field<float>(proximity_op, 1));
}

}  // namespace blender::nodes::node_geo_proximity_cc

void register_node_type_geo_proximity()
{
  namespace file_ns = blender::nodes::node_geo_proximity_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_PROXIMITY, "Geometry Proximity", NODE_CLASS_GEOMETRY);
  node_type_init(&ntype, file_ns::geo_proximity_init);
  node_type_storage(
      &ntype, "NodeGeometryProximity", node_free_standard_storage, node_copy_standard_storage);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}
