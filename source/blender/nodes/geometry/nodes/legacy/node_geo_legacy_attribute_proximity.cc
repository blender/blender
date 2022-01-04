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

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_legacy_attribute_proximity_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::Geometry>(N_("Target"));
  b.add_input<decl::String>(N_("Distance"));
  b.add_input<decl::String>(N_("Position"));
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "target_geometry_element", 0, "", ICON_NONE);
}

static void node_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryAttributeProximity *node_storage = MEM_cnew<NodeGeometryAttributeProximity>(
      __func__);

  node_storage->target_geometry_element = GEO_NODE_PROXIMITY_TARGET_FACES;
  node->storage = node_storage;
}

static void calculate_mesh_proximity(const VArray<float3> &positions,
                                     const Mesh &mesh,
                                     const GeometryNodeAttributeProximityTargetType type,
                                     MutableSpan<float> r_distances,
                                     MutableSpan<float3> r_locations)
{
  BVHTreeFromMesh bvh_data;
  switch (type) {
    case GEO_NODE_PROXIMITY_TARGET_POINTS:
      BKE_bvhtree_from_mesh_get(&bvh_data, &mesh, BVHTREE_FROM_VERTS, 2);
      break;
    case GEO_NODE_PROXIMITY_TARGET_EDGES:
      BKE_bvhtree_from_mesh_get(&bvh_data, &mesh, BVHTREE_FROM_EDGES, 2);
      break;
    case GEO_NODE_PROXIMITY_TARGET_FACES:
      BKE_bvhtree_from_mesh_get(&bvh_data, &mesh, BVHTREE_FROM_LOOPTRI, 2);
      break;
  }

  if (bvh_data.tree == nullptr) {
    return;
  }

  threading::parallel_for(positions.index_range(), 512, [&](IndexRange range) {
    BVHTreeNearest nearest;
    copy_v3_fl(nearest.co, FLT_MAX);
    nearest.index = -1;

    for (int i : range) {
      /* Use the distance to the last found point as upper bound to speedup the bvh lookup. */
      nearest.dist_sq = float3::distance_squared(nearest.co, positions[i]);

      BLI_bvhtree_find_nearest(
          bvh_data.tree, positions[i], &nearest, bvh_data.nearest_callback, &bvh_data);

      if (nearest.dist_sq < r_distances[i]) {
        r_distances[i] = nearest.dist_sq;
        if (!r_locations.is_empty()) {
          r_locations[i] = nearest.co;
        }
      }
    }
  });

  free_bvhtree_from_mesh(&bvh_data);
}

static void calculate_pointcloud_proximity(const VArray<float3> &positions,
                                           const PointCloud &pointcloud,
                                           MutableSpan<float> r_distances,
                                           MutableSpan<float3> r_locations)
{
  BVHTreeFromPointCloud bvh_data;
  BKE_bvhtree_from_pointcloud_get(&bvh_data, &pointcloud, 2);
  if (bvh_data.tree == nullptr) {
    return;
  }

  threading::parallel_for(positions.index_range(), 512, [&](IndexRange range) {
    BVHTreeNearest nearest;
    copy_v3_fl(nearest.co, FLT_MAX);
    nearest.index = -1;

    for (int i : range) {
      /* Use the distance to the closest point in the mesh to speedup the pointcloud bvh lookup.
       * This is ok because we only need to find the closest point in the pointcloud if it's
       * closer than the mesh. */
      nearest.dist_sq = r_distances[i];

      BLI_bvhtree_find_nearest(
          bvh_data.tree, positions[i], &nearest, bvh_data.nearest_callback, &bvh_data);

      if (nearest.dist_sq < r_distances[i]) {
        r_distances[i] = nearest.dist_sq;
        if (!r_locations.is_empty()) {
          r_locations[i] = nearest.co;
        }
      }
    }
  });

  free_bvhtree_from_pointcloud(&bvh_data);
}

static void attribute_calc_proximity(GeometryComponent &component,
                                     GeometrySet &target,
                                     GeoNodeExecParams &params)
{
  const std::string distance_name = params.get_input<std::string>("Distance");
  OutputAttribute_Typed<float> distance_attribute =
      component.attribute_try_get_for_output_only<float>(distance_name, ATTR_DOMAIN_POINT);

  const std::string location_name = params.get_input<std::string>("Position");
  OutputAttribute_Typed<float3> location_attribute =
      component.attribute_try_get_for_output_only<float3>(location_name, ATTR_DOMAIN_POINT);

  ReadAttributeLookup position_attribute = component.attribute_try_get_for_read("position");
  if (!position_attribute || (!distance_attribute && !location_attribute)) {
    return;
  }
  VArray<float3> positions = position_attribute.varray.typed<float3>();
  const NodeGeometryAttributeProximity &storage =
      *(const NodeGeometryAttributeProximity *)params.node().storage;

  Array<float> distances_internal;
  MutableSpan<float> distances;
  if (distance_attribute) {
    distances = distance_attribute.as_span();
  }
  else {
    /* Theoretically it would be possible to avoid using the distance array when it's not required
     * and there is only one component. However, this only adds an allocation and a single float
     * comparison per vertex, so it's likely not worth it. */
    distances_internal.reinitialize(positions.size());
    distances = distances_internal;
  }
  distances.fill(FLT_MAX);
  MutableSpan<float3> locations = location_attribute ? location_attribute.as_span() :
                                                       MutableSpan<float3>();

  if (target.has_mesh()) {
    calculate_mesh_proximity(
        positions,
        *target.get_mesh_for_read(),
        static_cast<GeometryNodeAttributeProximityTargetType>(storage.target_geometry_element),
        distances,
        locations);
  }

  if (target.has_pointcloud() &&
      storage.target_geometry_element == GEO_NODE_PROXIMITY_TARGET_POINTS) {
    calculate_pointcloud_proximity(
        positions, *target.get_pointcloud_for_read(), distances, locations);
  }

  if (distance_attribute) {
    /* Squared distances are used above to speed up comparisons,
     * so do the square roots now if necessary for the output attribute. */
    threading::parallel_for(distances.index_range(), 2048, [&](IndexRange range) {
      for (const int i : range) {
        distances[i] = std::sqrt(distances[i]);
      }
    });
    distance_attribute.save();
  }
  if (location_attribute) {
    location_attribute.save();
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  GeometrySet geometry_set_target = params.extract_input<GeometrySet>("Target");

  geometry_set = geometry::realize_instances_legacy(geometry_set);

  /* This isn't required. This node should be rewritten to handle instances
   * for the target geometry set. However, the generic BVH API complicates this. */
  geometry_set_target = geometry::realize_instances_legacy(geometry_set_target);

  if (geometry_set.has<MeshComponent>()) {
    attribute_calc_proximity(
        geometry_set.get_component_for_write<MeshComponent>(), geometry_set_target, params);
  }
  if (geometry_set.has<PointCloudComponent>()) {
    attribute_calc_proximity(
        geometry_set.get_component_for_write<PointCloudComponent>(), geometry_set_target, params);
  }
  if (geometry_set.has<CurveComponent>()) {
    attribute_calc_proximity(
        geometry_set.get_component_for_write<CurveComponent>(), geometry_set_target, params);
  }

  params.set_output("Geometry", geometry_set);
}

}  // namespace blender::nodes::node_geo_legacy_attribute_proximity_cc

void register_node_type_geo_legacy_attribute_proximity()
{
  namespace file_ns = blender::nodes::node_geo_legacy_attribute_proximity_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_LEGACY_ATTRIBUTE_PROXIMITY, "Attribute Proximity", NODE_CLASS_ATTRIBUTE);
  node_type_init(&ntype, file_ns::node_init);
  node_type_storage(&ntype,
                    "NodeGeometryAttributeProximity",
                    node_free_standard_storage,
                    node_copy_standard_storage);

  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}
