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

#include "BKE_volume.h"

#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_bounding_box_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_bounding_box_out[] = {
    {SOCK_GEOMETRY, N_("Mesh")},
    {SOCK_VECTOR, N_("Min")},
    {SOCK_VECTOR, N_("Max")},
    {-1, ""},
};

namespace blender::nodes {

using bke::GeometryInstanceGroup;

static void compute_min_max_from_position_and_transform(const GeometryComponent &component,
                                                        Span<float4x4> transforms,
                                                        float3 &r_min,
                                                        float3 &r_max)
{
  ReadAttributePtr position_attribute = component.attribute_try_get_for_read("position");
  if (!position_attribute) {
    BLI_assert(component.attribute_domain_size(ATTR_DOMAIN_POINT) == 0);
    return;
  }
  Span<float3> positions = position_attribute->get_span<float3>();

  for (const float4x4 &transform : transforms) {
    for (const float3 &position : positions) {
      const float3 transformed_position = transform * position;
      minmax_v3v3_v3(r_min, r_max, transformed_position);
    }
  }
}

static void compute_min_max_from_volume_and_transforms(const VolumeComponent &volume_component,
                                                       Span<float4x4> transforms,
                                                       float3 &r_min,
                                                       float3 &r_max)
{
#ifdef WITH_OPENVDB
  const Volume *volume = volume_component.get_for_read();
  if (volume == nullptr) {
    return;
  }
  for (const int i : IndexRange(BKE_volume_num_grids(volume))) {
    const VolumeGrid *volume_grid = BKE_volume_grid_get_for_read(volume, i);
    openvdb::GridBase::ConstPtr grid = BKE_volume_grid_openvdb_for_read(volume, volume_grid);

    for (const float4x4 &transform : transforms) {
      openvdb::GridBase::ConstPtr instance_grid = BKE_volume_grid_shallow_transform(grid,
                                                                                    transform);
      float3 grid_min = float3(FLT_MAX);
      float3 grid_max = float3(-FLT_MAX);
      if (BKE_volume_grid_bounds(instance_grid, grid_min, grid_max)) {
        DO_MIN(grid_min, r_min);
        DO_MAX(grid_max, r_max);
      }
    }
  }
#else
  UNUSED_VARS(volume_component, transforms, r_min, r_max);
#endif
}

static void compute_geometry_set_instances_boundbox(const GeometrySet &geometry_set,
                                                    float3 &r_min,
                                                    float3 &r_max)
{
  Vector<GeometryInstanceGroup> set_groups;
  bke::geometry_set_gather_instances(geometry_set, set_groups);

  for (const GeometryInstanceGroup &set_group : set_groups) {
    const GeometrySet &set = set_group.geometry_set;
    Span<float4x4> transforms = set_group.transforms;

    if (set.has<PointCloudComponent>()) {
      compute_min_max_from_position_and_transform(
          *set.get_component_for_read<PointCloudComponent>(), transforms, r_min, r_max);
    }
    if (set.has<MeshComponent>()) {
      compute_min_max_from_position_and_transform(
          *set.get_component_for_read<MeshComponent>(), transforms, r_min, r_max);
    }
    if (set.has<VolumeComponent>()) {
      compute_min_max_from_volume_and_transforms(
          *set.get_component_for_read<VolumeComponent>(), transforms, r_min, r_max);
    }
  }
}

static void geo_node_bounding_box_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  float3 min = float3(FLT_MAX);
  float3 max = float3(-FLT_MAX);

  if (geometry_set.has_instances()) {
    compute_geometry_set_instances_boundbox(geometry_set, min, max);
  }
  else {
    geometry_set.compute_boundbox_without_instances(&min, &max);
  }

  if (min == float3(FLT_MAX)) {
    params.set_output("Mesh", GeometrySet());
    params.set_output("Min", float3(0));
    params.set_output("Max", float3(0));
  }
  else {
    const float3 scale = max - min;
    const float3 center = min + scale / 2.0f;
    Mesh *mesh = create_cube_mesh(1.0f);
    transform_mesh(mesh, center, float3(0), scale);
    params.set_output("Mesh", GeometrySet::create_with_mesh(mesh));
    params.set_output("Min", min);
    params.set_output("Max", max);
  }
}

}  // namespace blender::nodes

void register_node_type_geo_bounding_box()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_BOUNDING_BOX, "Bounding Box", NODE_CLASS_GEOMETRY, 0);
  node_type_socket_templates(&ntype, geo_node_bounding_box_in, geo_node_bounding_box_out);
  ntype.geometry_node_execute = blender::nodes::geo_node_bounding_box_exec;
  nodeRegisterType(&ntype);
}
