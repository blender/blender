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

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
#endif

#include "BLI_float4x4.hh"

#include "DNA_pointcloud_types.h"
#include "DNA_volume_types.h"

#include "BKE_mesh.h"
#include "BKE_volume.h"

#include "DEG_depsgraph_query.h"

#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_transform_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_VECTOR, N_("Translation"), 0.0f, 0.0f, 0.0f, 1.0f, -FLT_MAX, FLT_MAX, PROP_TRANSLATION},
    {SOCK_VECTOR, N_("Rotation"), 0.0f, 0.0f, 0.0f, 1.0f, -FLT_MAX, FLT_MAX, PROP_EULER},
    {SOCK_VECTOR, N_("Scale"), 1.0f, 1.0f, 1.0f, 1.0f, -FLT_MAX, FLT_MAX, PROP_XYZ},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_transform_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

namespace blender::nodes {

static bool use_translate(const float3 rotation, const float3 scale)
{
  if (compare_ff(rotation.length_squared(), 0.0f, 1e-9f) != 1) {
    return false;
  }
  if (compare_ff(scale.x, 1.0f, 1e-9f) != 1 || compare_ff(scale.y, 1.0f, 1e-9f) != 1 ||
      compare_ff(scale.z, 1.0f, 1e-9f) != 1) {
    return false;
  }
  return true;
}

void transform_mesh(Mesh *mesh,
                    const float3 translation,
                    const float3 rotation,
                    const float3 scale)
{
  /* Use only translation if rotation and scale are zero. */
  if (use_translate(rotation, scale)) {
    if (!translation.is_zero()) {
      BKE_mesh_translate(mesh, translation, false);
    }
  }
  else {
    const float4x4 matrix = float4x4::from_loc_eul_scale(translation, rotation, scale);
    BKE_mesh_transform(mesh, matrix.values, false);
    BKE_mesh_calc_normals(mesh);
  }
}

static void transform_pointcloud(PointCloud *pointcloud,
                                 const float3 translation,
                                 const float3 rotation,
                                 const float3 scale)
{
  /* Use only translation if rotation and scale don't apply. */
  if (use_translate(rotation, scale)) {
    for (const int i : IndexRange(pointcloud->totpoint)) {
      add_v3_v3(pointcloud->co[i], translation);
    }
  }
  else {
    const float4x4 matrix = float4x4::from_loc_eul_scale(translation, rotation, scale);
    for (const int i : IndexRange(pointcloud->totpoint)) {
      float3 &co = *(float3 *)pointcloud->co[i];
      co = matrix * co;
    }
  }
}

static void transform_instances(InstancesComponent &instances,
                                const float3 translation,
                                const float3 rotation,
                                const float3 scale)
{
  MutableSpan<float4x4> transforms = instances.transforms();

  /* Use only translation if rotation and scale don't apply. */
  if (use_translate(rotation, scale)) {
    for (float4x4 &transform : transforms) {
      add_v3_v3(transform.ptr()[3], translation);
    }
  }
  else {
    const float4x4 matrix = float4x4::from_loc_eul_scale(translation, rotation, scale);
    for (float4x4 &transform : transforms) {
      transform = matrix * transform;
    }
  }
}

static void transform_volume(Volume *volume,
                             const float3 translation,
                             const float3 rotation,
                             const float3 scale,
                             GeoNodeExecParams &params)
{
#ifdef WITH_OPENVDB
  /* Scaling an axis to zero is not supported for volumes. */
  const float3 limited_scale = {
      (scale.x == 0.0f) ? FLT_EPSILON : scale.x,
      (scale.y == 0.0f) ? FLT_EPSILON : scale.y,
      (scale.z == 0.0f) ? FLT_EPSILON : scale.z,
  };

  const Main *bmain = DEG_get_bmain(params.depsgraph());
  BKE_volume_load(volume, bmain);

  const float4x4 matrix = float4x4::from_loc_eul_scale(translation, rotation, limited_scale);

  openvdb::Mat4s vdb_matrix;
  memcpy(vdb_matrix.asPointer(), matrix, sizeof(float[4][4]));
  openvdb::Mat4d vdb_matrix_d{vdb_matrix};

  const int num_grids = BKE_volume_num_grids(volume);
  for (const int i : IndexRange(num_grids)) {
    VolumeGrid *volume_grid = BKE_volume_grid_get_for_write(volume, i);

    openvdb::GridBase::Ptr grid = BKE_volume_grid_openvdb_for_write(volume, volume_grid, false);
    openvdb::math::Transform &grid_transform = grid->transform();
    grid_transform.postMult(vdb_matrix_d);
  }
#else
  UNUSED_VARS(volume, translation, rotation, scale, params);
#endif
}

static void geo_node_transform_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  const float3 translation = params.extract_input<float3>("Translation");
  const float3 rotation = params.extract_input<float3>("Rotation");
  const float3 scale = params.extract_input<float3>("Scale");

  if (geometry_set.has_mesh()) {
    Mesh *mesh = geometry_set.get_mesh_for_write();
    transform_mesh(mesh, translation, rotation, scale);
  }

  if (geometry_set.has_pointcloud()) {
    PointCloud *pointcloud = geometry_set.get_pointcloud_for_write();
    transform_pointcloud(pointcloud, translation, rotation, scale);
  }

  if (geometry_set.has_instances()) {
    InstancesComponent &instances = geometry_set.get_component_for_write<InstancesComponent>();
    transform_instances(instances, translation, rotation, scale);
  }

  if (geometry_set.has_volume()) {
    Volume *volume = geometry_set.get_volume_for_write();
    transform_volume(volume, translation, rotation, scale, params);
  }

  params.set_output("Geometry", std::move(geometry_set));
}
}  // namespace blender::nodes

void register_node_type_geo_transform()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_TRANSFORM, "Transform", NODE_CLASS_GEOMETRY, 0);
  node_type_socket_templates(&ntype, geo_node_transform_in, geo_node_transform_out);
  ntype.geometry_node_execute = blender::nodes::geo_node_transform_exec;
  nodeRegisterType(&ntype);
}
