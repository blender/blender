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

#include "DNA_mesh_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_volume_types.h"

#include "BKE_mesh.h"
#include "BKE_pointcloud.h"
#include "BKE_spline.hh"
#include "BKE_volume.h"

#include "DEG_depsgraph_query.h"

#include "node_geometry_util.hh"

namespace blender::nodes {

static void geo_node_transform_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry");
  b.add_input<decl::Vector>("Translation").subtype(PROP_TRANSLATION);
  b.add_input<decl::Vector>("Rotation").subtype(PROP_EULER);
  b.add_input<decl::Vector>("Scale").default_value({1, 1, 1}).subtype(PROP_XYZ);
  b.add_output<decl::Geometry>("Geometry");
}

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

static void translate_mesh(Mesh &mesh, const float3 translation)
{
  if (!translation.is_zero()) {
    BKE_mesh_translate(&mesh, translation, false);
  }
}

static void transform_mesh(Mesh &mesh, const float4x4 &transform)
{
  BKE_mesh_transform(&mesh, transform.values, false);
  BKE_mesh_normals_tag_dirty(&mesh);
}

void transform_mesh(Mesh &mesh,
                    const float3 translation,
                    const float3 rotation,
                    const float3 scale)
{
  const float4x4 matrix = float4x4::from_loc_eul_scale(translation, rotation, scale);
  transform_mesh(mesh, matrix);
}

static void translate_pointcloud(PointCloud &pointcloud, const float3 translation)
{
  CustomData_duplicate_referenced_layer(&pointcloud.pdata, CD_PROP_FLOAT3, pointcloud.totpoint);
  BKE_pointcloud_update_customdata_pointers(&pointcloud);
  for (const int i : IndexRange(pointcloud.totpoint)) {
    add_v3_v3(pointcloud.co[i], translation);
  }
}

static void transform_pointcloud(PointCloud &pointcloud, const float4x4 &transform)
{
  CustomData_duplicate_referenced_layer(&pointcloud.pdata, CD_PROP_FLOAT3, pointcloud.totpoint);
  BKE_pointcloud_update_customdata_pointers(&pointcloud);
  for (const int i : IndexRange(pointcloud.totpoint)) {
    float3 &co = *(float3 *)pointcloud.co[i];
    co = transform * co;
  }
}

static void translate_instances(InstancesComponent &instances, const float3 translation)
{
  MutableSpan<float4x4> transforms = instances.instance_transforms();
  for (float4x4 &transform : transforms) {
    add_v3_v3(transform.ptr()[3], translation);
  }
}

static void transform_instances(InstancesComponent &instances, const float4x4 &transform)
{
  MutableSpan<float4x4> instance_transforms = instances.instance_transforms();
  for (float4x4 &instance_transform : instance_transforms) {
    instance_transform = transform * instance_transform;
  }
}

static void transform_volume(Volume &volume, const float4x4 &transform, const Depsgraph &depsgraph)
{
#ifdef WITH_OPENVDB
  /* Scaling an axis to zero is not supported for volumes. */
  const float3 translation = transform.translation();
  const float3 rotation = transform.to_euler();
  const float3 scale = transform.scale();
  const float3 limited_scale = {
      (scale.x == 0.0f) ? FLT_EPSILON : scale.x,
      (scale.y == 0.0f) ? FLT_EPSILON : scale.y,
      (scale.z == 0.0f) ? FLT_EPSILON : scale.z,
  };
  const float4x4 scale_limited_transform = float4x4::from_loc_eul_scale(
      translation, rotation, limited_scale);

  const Main *bmain = DEG_get_bmain(&depsgraph);
  BKE_volume_load(&volume, bmain);

  openvdb::Mat4s vdb_matrix;
  memcpy(vdb_matrix.asPointer(), &scale_limited_transform, sizeof(float[4][4]));
  openvdb::Mat4d vdb_matrix_d{vdb_matrix};

  const int num_grids = BKE_volume_num_grids(&volume);
  for (const int i : IndexRange(num_grids)) {
    VolumeGrid *volume_grid = BKE_volume_grid_get_for_write(&volume, i);

    openvdb::GridBase::Ptr grid = BKE_volume_grid_openvdb_for_write(&volume, volume_grid, false);
    openvdb::math::Transform &grid_transform = grid->transform();
    grid_transform.postMult(vdb_matrix_d);
  }
#else
  UNUSED_VARS(volume, transform, depsgraph);
#endif
}

static void translate_volume(Volume &volume, const float3 translation, const Depsgraph &depsgraph)
{
  transform_volume(volume, float4x4::from_location(translation), depsgraph);
}

void transform_geometry_set(GeometrySet &geometry,
                            const float4x4 &transform,
                            const Depsgraph &depsgraph)
{
  if (CurveEval *curve = geometry.get_curve_for_write()) {
    curve->transform(transform);
  }
  if (Mesh *mesh = geometry.get_mesh_for_write()) {
    transform_mesh(*mesh, transform);
  }
  if (PointCloud *pointcloud = geometry.get_pointcloud_for_write()) {
    transform_pointcloud(*pointcloud, transform);
  }
  if (Volume *volume = geometry.get_volume_for_write()) {
    transform_volume(*volume, transform, depsgraph);
  }
  if (geometry.has_instances()) {
    transform_instances(geometry.get_component_for_write<InstancesComponent>(), transform);
  }
}

static void translate_geometry_set(GeometrySet &geometry,
                                   const float3 translation,
                                   const Depsgraph &depsgraph)
{
  if (CurveEval *curve = geometry.get_curve_for_write()) {
    curve->translate(translation);
  }
  if (Mesh *mesh = geometry.get_mesh_for_write()) {
    translate_mesh(*mesh, translation);
  }
  if (PointCloud *pointcloud = geometry.get_pointcloud_for_write()) {
    translate_pointcloud(*pointcloud, translation);
  }
  if (Volume *volume = geometry.get_volume_for_write()) {
    translate_volume(*volume, translation, depsgraph);
  }
  if (geometry.has_instances()) {
    translate_instances(geometry.get_component_for_write<InstancesComponent>(), translation);
  }
}

static void geo_node_transform_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  const float3 translation = params.extract_input<float3>("Translation");
  const float3 rotation = params.extract_input<float3>("Rotation");
  const float3 scale = params.extract_input<float3>("Scale");

  /* Use only translation if rotation and scale don't apply. */
  if (use_translate(rotation, scale)) {
    translate_geometry_set(geometry_set, translation, *params.depsgraph());
  }
  else {
    transform_geometry_set(geometry_set,
                           float4x4::from_loc_eul_scale(translation, rotation, scale),
                           *params.depsgraph());
  }

  params.set_output("Geometry", std::move(geometry_set));
}
}  // namespace blender::nodes

void register_node_type_geo_transform()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_TRANSFORM, "Transform", NODE_CLASS_GEOMETRY, 0);
  ntype.declare = blender::nodes::geo_node_transform_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_transform_exec;
  nodeRegisterType(&ntype);
}
