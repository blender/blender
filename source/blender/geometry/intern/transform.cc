/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
#endif

#include "GEO_transform.hh"

#include "BLI_math_base.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.hh"
#include "BLI_math_vector.hh"
#include "BLI_task.hh"

#include "DNA_grease_pencil_types.h"
#include "DNA_mesh_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_volume_types.h"

#include "BKE_attribute.hh"
#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"
#include "BKE_mesh.hh"
#include "BKE_pointcloud.hh"
#include "BKE_volume.hh"

namespace blender::geometry {

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
      position = math::transform_point(matrix, position);
    }
  });
}

static void transform_mesh(Mesh &mesh, const float4x4 &transform)
{
  transform_positions(mesh.vert_positions_for_write(), transform);
  mesh.tag_positions_changed();
}

static void translate_pointcloud(PointCloud &pointcloud, const float3 translation)
{
  if (math::is_zero(translation)) {
    return;
  }

  std::optional<Bounds<float3>> bounds;
  if (pointcloud.runtime->bounds_cache.is_cached()) {
    bounds = pointcloud.runtime->bounds_cache.data();
  }

  bke::MutableAttributeAccessor attributes = pointcloud.attributes_for_write();
  bke::SpanAttributeWriter position = attributes.lookup_or_add_for_write_span<float3>(
      "position", bke::AttrDomain::Point);
  translate_positions(position.span, translation);
  position.finish();

  if (bounds) {
    bounds->min += translation;
    bounds->max += translation;
    pointcloud.runtime->bounds_cache.ensure([&](Bounds<float3> &r_data) { r_data = *bounds; });
  }
}

static void transform_pointcloud(PointCloud &pointcloud, const float4x4 &transform)
{
  bke::MutableAttributeAccessor attributes = pointcloud.attributes_for_write();
  bke::SpanAttributeWriter position = attributes.lookup_or_add_for_write_span<float3>(
      "position", bke::AttrDomain::Point);
  transform_positions(position.span, transform);
  position.finish();
}

static void translate_greasepencil(GreasePencil &grease_pencil, const float3 translation)
{
  using namespace blender::bke::greasepencil;
  for (const int layer_index : grease_pencil.layers().index_range()) {
    if (Drawing *drawing = get_eval_grease_pencil_layer_drawing_for_write(grease_pencil,
                                                                          layer_index))
    {
      drawing->strokes_for_write().translate(translation);
    }
  }
}

static void transform_greasepencil(GreasePencil &grease_pencil, const float4x4 &transform)
{
  using namespace blender::bke::greasepencil;
  for (const int layer_index : grease_pencil.layers().index_range()) {
    if (Drawing *drawing = get_eval_grease_pencil_layer_drawing_for_write(grease_pencil,
                                                                          layer_index))
    {
      drawing->strokes_for_write().transform(transform);
    }
  }
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

static bool transform_volume(Volume &volume, const float4x4 &transform)
{
  bool found_too_small_scale = false;
#ifdef WITH_OPENVDB
  openvdb::Mat4s vdb_matrix;
  memcpy(vdb_matrix.asPointer(), &transform, sizeof(float[4][4]));
  openvdb::Mat4d vdb_matrix_d{vdb_matrix};

  const int grids_num = BKE_volume_num_grids(&volume);
  for (const int i : IndexRange(grids_num)) {
    bke::VolumeGridData *volume_grid = BKE_volume_grid_get_for_write(&volume, i);

    float4x4 grid_matrix = bke::volume_grid::get_transform_matrix(*volume_grid);
    grid_matrix = transform * grid_matrix;
    const float determinant = math::determinant(grid_matrix);
    if (!BKE_volume_grid_determinant_valid(determinant)) {
      found_too_small_scale = true;
      /* Clear the tree because it is too small. */
      bke::volume_grid::clear_tree(*volume_grid);
      if (determinant == 0) {
        /* Reset rotation and scale. */
        grid_matrix.x_axis() = float3(1, 0, 0);
        grid_matrix.y_axis() = float3(0, 1, 0);
        grid_matrix.z_axis() = float3(0, 0, 1);
      }
      else {
        /* Keep rotation but reset scale. */
        grid_matrix.x_axis() = math::normalize(grid_matrix.x_axis());
        grid_matrix.y_axis() = math::normalize(grid_matrix.y_axis());
        grid_matrix.z_axis() = math::normalize(grid_matrix.z_axis());
      }
    }
    bke::volume_grid::set_transform_matrix(*volume_grid, grid_matrix);
  }

#else
  UNUSED_VARS(volume, transform);
#endif
  return found_too_small_scale;
}

static void translate_volume(Volume &volume, const float3 translation)
{
  transform_volume(volume, math::from_location<float4x4>(translation));
}

static void transform_curve_edit_hints(bke::CurvesEditHints &edit_hints, const float4x4 &transform)
{
  if (edit_hints.positions.has_value()) {
    transform_positions(*edit_hints.positions, transform);
  }
  float3x3 deform_mat;
  copy_m3_m4(deform_mat.ptr(), transform.ptr());
  if (edit_hints.deform_mats.has_value()) {
    MutableSpan<float3x3> deform_mats = *edit_hints.deform_mats;
    threading::parallel_for(deform_mats.index_range(), 1024, [&](const IndexRange range) {
      for (const int64_t i : range) {
        deform_mats[i] = deform_mat * deform_mats[i];
      }
    });
  }
  else {
    edit_hints.deform_mats.emplace(edit_hints.curves_id_orig.geometry.point_num, deform_mat);
  }
}

static void translate_curve_edit_hints(bke::CurvesEditHints &edit_hints, const float3 &translation)
{
  if (edit_hints.positions.has_value()) {
    translate_positions(*edit_hints.positions, translation);
  }
}

void translate_geometry(bke::GeometrySet &geometry, const float3 translation)
{
  if (Curves *curves = geometry.get_curves_for_write()) {
    curves->geometry.wrap().translate(translation);
  }
  if (Mesh *mesh = geometry.get_mesh_for_write()) {
    BKE_mesh_translate(mesh, translation, false);
  }
  if (PointCloud *pointcloud = geometry.get_pointcloud_for_write()) {
    translate_pointcloud(*pointcloud, translation);
  }
  if (GreasePencil *grease_pencil = geometry.get_grease_pencil_for_write()) {
    translate_greasepencil(*grease_pencil, translation);
  }
  if (Volume *volume = geometry.get_volume_for_write()) {
    translate_volume(*volume, translation);
  }
  if (bke::Instances *instances = geometry.get_instances_for_write()) {
    translate_instances(*instances, translation);
  }
  if (bke::CurvesEditHints *curve_edit_hints = geometry.get_curve_edit_hints_for_write()) {
    translate_curve_edit_hints(*curve_edit_hints, translation);
  }
}

std::optional<TransformGeometryErrors> transform_geometry(bke::GeometrySet &geometry,
                                                          const float4x4 &transform)
{
  TransformGeometryErrors errors;
  if (Curves *curves = geometry.get_curves_for_write()) {
    curves->geometry.wrap().transform(transform);
  }
  if (Mesh *mesh = geometry.get_mesh_for_write()) {
    transform_mesh(*mesh, transform);
  }
  if (PointCloud *pointcloud = geometry.get_pointcloud_for_write()) {
    transform_pointcloud(*pointcloud, transform);
  }
  if (GreasePencil *grease_pencil = geometry.get_grease_pencil_for_write()) {
    transform_greasepencil(*grease_pencil, transform);
  }
  if (Volume *volume = geometry.get_volume_for_write()) {
    errors.volume_too_small = transform_volume(*volume, transform);
  }
  if (bke::Instances *instances = geometry.get_instances_for_write()) {
    transform_instances(*instances, transform);
  }
  if (bke::CurvesEditHints *curve_edit_hints = geometry.get_curve_edit_hints_for_write()) {
    transform_curve_edit_hints(*curve_edit_hints, transform);
  }

  if (errors.volume_too_small) {
    return errors;
  }
  return std::nullopt;
}

void transform_mesh(Mesh &mesh,
                    const float3 translation,
                    const math::Quaternion rotation,
                    const float3 scale)
{
  const float4x4 matrix = math::from_loc_rot_scale<float4x4>(translation, rotation, scale);
  transform_mesh(mesh, matrix);
}

}  // namespace blender::geometry
