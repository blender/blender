/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
#endif

#include "BLI_float4x4.hh"

#include "DNA_mesh_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_volume_types.h"

#include "BKE_curves.hh"
#include "BKE_instances.hh"
#include "BKE_mesh.h"
#include "BKE_pointcloud.h"
#include "BKE_volume.h"

#include "DEG_depsgraph_query.h"

#include "node_geometry_util.hh"

namespace blender::nodes {

static bool use_translate(const float3 rotation, const float3 scale)
{
  if (compare_ff(math::length_squared(rotation), 0.0f, 1e-9f) != 1) {
    return false;
  }
  if (compare_ff(scale.x, 1.0f, 1e-9f) != 1 || compare_ff(scale.y, 1.0f, 1e-9f) != 1 ||
      compare_ff(scale.z, 1.0f, 1e-9f) != 1) {
    return false;
  }
  return true;
}

static void translate_positions(MutableSpan<float3> positions, const float3 &translation)
{
  threading::parallel_for(positions.index_range(), 2048, [&](const IndexRange range) {
    for (float3 &position : positions.slice(range)) {
      position += translation;
    }
  });
}

static void transform_positions(MutableSpan<float3> positions, const float4x4 &matrix)
{
  threading::parallel_for(positions.index_range(), 1024, [&](const IndexRange range) {
    for (float3 &position : positions.slice(range)) {
      position = matrix * position;
    }
  });
}

static void translate_mesh(Mesh &mesh, const float3 translation)
{
  if (!math::is_zero(translation)) {
    BKE_mesh_translate(&mesh, translation, false);
  }
}

static void transform_mesh(Mesh &mesh, const float4x4 &transform)
{
  BKE_mesh_transform(&mesh, transform.values, false);
}

static void translate_pointcloud(PointCloud &pointcloud, const float3 translation)
{
  MutableAttributeAccessor attributes = pointcloud.attributes_for_write();
  SpanAttributeWriter position = attributes.lookup_or_add_for_write_span<float3>(
      "position", ATTR_DOMAIN_POINT);
  translate_positions(position.span, translation);
  position.finish();
}

static void transform_pointcloud(PointCloud &pointcloud, const float4x4 &transform)
{
  MutableAttributeAccessor attributes = pointcloud.attributes_for_write();
  SpanAttributeWriter position = attributes.lookup_or_add_for_write_span<float3>(
      "position", ATTR_DOMAIN_POINT);
  transform_positions(position.span, transform);
  position.finish();
}

static void translate_instances(bke::Instances &instances, const float3 translation)
{
  MutableSpan<float4x4> transforms = instances.transforms();
  threading::parallel_for(transforms.index_range(), 1024, [&](const IndexRange range) {
    for (float4x4 &instance_transform : transforms.slice(range)) {
      add_v3_v3(instance_transform.ptr()[3], translation);
    }
  });
}

static void transform_instances(bke::Instances &instances, const float4x4 &transform)
{
  MutableSpan<float4x4> transforms = instances.transforms();
  threading::parallel_for(transforms.index_range(), 1024, [&](const IndexRange range) {
    for (float4x4 &instance_transform : transforms.slice(range)) {
      instance_transform = transform * instance_transform;
    }
  });
}

static void transform_volume(GeoNodeExecParams &params,
                             Volume &volume,
                             const float4x4 &transform,
                             const Depsgraph &depsgraph)
{
#ifdef WITH_OPENVDB
  const Main *bmain = DEG_get_bmain(&depsgraph);
  BKE_volume_load(&volume, bmain);

  openvdb::Mat4s vdb_matrix;
  memcpy(vdb_matrix.asPointer(), &transform, sizeof(float[4][4]));
  openvdb::Mat4d vdb_matrix_d{vdb_matrix};

  bool found_too_small_scale = false;
  const int grids_num = BKE_volume_num_grids(&volume);
  for (const int i : IndexRange(grids_num)) {
    VolumeGrid *volume_grid = BKE_volume_grid_get_for_write(&volume, i);
    float4x4 grid_matrix;
    BKE_volume_grid_transform_matrix(volume_grid, grid_matrix.values);
    mul_m4_m4_pre(grid_matrix.values, transform.values);
    const float determinant = determinant_m4(grid_matrix.values);
    if (!BKE_volume_grid_determinant_valid(determinant)) {
      found_too_small_scale = true;
      /* Clear the tree because it is too small. */
      BKE_volume_grid_clear_tree(volume, *volume_grid);
      if (determinant == 0) {
        /* Reset rotation and scale. */
        copy_v3_fl3(grid_matrix.values[0], 1, 0, 0);
        copy_v3_fl3(grid_matrix.values[1], 0, 1, 0);
        copy_v3_fl3(grid_matrix.values[2], 0, 0, 1);
      }
      else {
        /* Keep rotation but reset scale. */
        normalize_v3(grid_matrix.values[0]);
        normalize_v3(grid_matrix.values[1]);
        normalize_v3(grid_matrix.values[2]);
      }
    }
    BKE_volume_grid_transform_matrix_set(volume_grid, grid_matrix.values);
  }
  if (found_too_small_scale) {
    params.error_message_add(NodeWarningType::Warning,
                             TIP_("Volume scale is lower than permitted by OpenVDB"));
  }
#else
  UNUSED_VARS(params, volume, transform, depsgraph);
#endif
}

static void translate_volume(GeoNodeExecParams &params,
                             Volume &volume,
                             const float3 translation,
                             const Depsgraph &depsgraph)
{
  transform_volume(params, volume, float4x4::from_location(translation), depsgraph);
}

static void transform_curve_edit_hints(bke::CurvesEditHints &edit_hints, const float4x4 &transform)
{
  if (edit_hints.positions.has_value()) {
    for (float3 &pos : *edit_hints.positions) {
      pos = transform * pos;
    }
  }
  float3x3 deform_mat;
  copy_m3_m4(deform_mat.values, transform.values);
  if (edit_hints.deform_mats.has_value()) {
    for (float3x3 &mat : *edit_hints.deform_mats) {
      mat = deform_mat * mat;
    }
  }
  else {
    edit_hints.deform_mats.emplace(edit_hints.curves_id_orig.geometry.point_num, deform_mat);
  }
}

static void translate_curve_edit_hints(bke::CurvesEditHints &edit_hints, const float3 &translation)
{
  if (edit_hints.positions.has_value()) {
    for (float3 &pos : *edit_hints.positions) {
      pos += translation;
    }
  }
}

static void translate_geometry_set(GeoNodeExecParams &params,
                                   GeometrySet &geometry,
                                   const float3 translation,
                                   const Depsgraph &depsgraph)
{
  if (Curves *curves = geometry.get_curves_for_write()) {
    bke::CurvesGeometry::wrap(curves->geometry).translate(translation);
  }
  if (Mesh *mesh = geometry.get_mesh_for_write()) {
    translate_mesh(*mesh, translation);
  }
  if (PointCloud *pointcloud = geometry.get_pointcloud_for_write()) {
    translate_pointcloud(*pointcloud, translation);
  }
  if (Volume *volume = geometry.get_volume_for_write()) {
    translate_volume(params, *volume, translation, depsgraph);
  }
  if (bke::Instances *instances = geometry.get_instances_for_write()) {
    translate_instances(*instances, translation);
  }
  if (bke::CurvesEditHints *curve_edit_hints = geometry.get_curve_edit_hints_for_write()) {
    translate_curve_edit_hints(*curve_edit_hints, translation);
  }
}

void transform_geometry_set(GeoNodeExecParams &params,
                            GeometrySet &geometry,
                            const float4x4 &transform,
                            const Depsgraph &depsgraph)
{
  if (Curves *curves = geometry.get_curves_for_write()) {
    bke::CurvesGeometry::wrap(curves->geometry).transform(transform);
  }
  if (Mesh *mesh = geometry.get_mesh_for_write()) {
    transform_mesh(*mesh, transform);
  }
  if (PointCloud *pointcloud = geometry.get_pointcloud_for_write()) {
    transform_pointcloud(*pointcloud, transform);
  }
  if (Volume *volume = geometry.get_volume_for_write()) {
    transform_volume(params, *volume, transform, depsgraph);
  }
  if (bke::Instances *instances = geometry.get_instances_for_write()) {
    transform_instances(*instances, transform);
  }
  if (bke::CurvesEditHints *curve_edit_hints = geometry.get_curve_edit_hints_for_write()) {
    transform_curve_edit_hints(*curve_edit_hints, transform);
  }
}

void transform_mesh(Mesh &mesh,
                    const float3 translation,
                    const float3 rotation,
                    const float3 scale)
{
  const float4x4 matrix = float4x4::from_loc_eul_scale(translation, rotation, scale);
  transform_mesh(mesh, matrix);
}

}  // namespace blender::nodes

namespace blender::nodes::node_geo_transform_geometry_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::Vector>(N_("Translation")).subtype(PROP_TRANSLATION);
  b.add_input<decl::Vector>(N_("Rotation")).subtype(PROP_EULER);
  b.add_input<decl::Vector>(N_("Scale")).default_value({1, 1, 1}).subtype(PROP_XYZ);
  b.add_output<decl::Geometry>(N_("Geometry")).propagate_all();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  const float3 translation = params.extract_input<float3>("Translation");
  const float3 rotation = params.extract_input<float3>("Rotation");
  const float3 scale = params.extract_input<float3>("Scale");

  /* Use only translation if rotation and scale don't apply. */
  if (use_translate(rotation, scale)) {
    translate_geometry_set(params, geometry_set, translation, *params.depsgraph());
  }
  else {
    transform_geometry_set(params,
                           geometry_set,
                           float4x4::from_loc_eul_scale(translation, rotation, scale),
                           *params.depsgraph());
  }

  params.set_output("Geometry", std::move(geometry_set));
}
}  // namespace blender::nodes::node_geo_transform_geometry_cc

void register_node_type_geo_transform_geometry()
{
  namespace file_ns = blender::nodes::node_geo_transform_geometry_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_TRANSFORM_GEOMETRY, "Transform Geometry", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
