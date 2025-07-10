/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_reader_points.hh"
#include "usd_attribute_utils.hh"

#include "BKE_geometry_set.hh"
#include "BKE_object.hh"
#include "BKE_pointcloud.hh"

#include "BLI_span.hh"

#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"

#include <pxr/usd/usdGeom/primvar.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>

namespace blender::io::usd {

void USDPointsReader::create_object(Main *bmain)
{
  PointCloud *pointcloud = BKE_pointcloud_add(bmain, name_.c_str());
  object_ = BKE_object_add_only_object(bmain, OB_POINTCLOUD, name_.c_str());
  object_->data = pointcloud;
}

void USDPointsReader::read_object_data(Main *bmain, pxr::UsdTimeCode time)
{
  const USDMeshReadParams params = create_mesh_read_params(time.GetValue(),
                                                           import_params_.mesh_read_flag);

  PointCloud *pointcloud = static_cast<PointCloud *>(object_->data);

  bke::GeometrySet geometry_set = bke::GeometrySet::from_pointcloud(
      pointcloud, bke::GeometryOwnershipType::Editable);

  read_geometry(geometry_set, params, nullptr);

  PointCloud *read_pointcloud =
      geometry_set.get_component_for_write<bke::PointCloudComponent>().release();

  if (read_pointcloud != pointcloud) {
    BKE_pointcloud_nomain_to_pointcloud(read_pointcloud, pointcloud);
  }

  if (is_animated()) {
    /* If the point cloud has animated positions or attributes, we add the cache
     * modifier. */
    add_cache_modifier();
  }

  /* Update the transform. */
  USDXformReader::read_object_data(bmain, time);
}

void USDPointsReader::read_geometry(bke::GeometrySet &geometry_set,
                                    USDMeshReadParams params,
                                    const char ** /*r_err_str*/)
{
  PointCloud *pointcloud = geometry_set.get_pointcloud_for_write();

  /* Get the existing point cloud data. */
  pxr::VtVec3fArray usd_positions;
  points_prim_.GetPointsAttr().Get(&usd_positions, params.motion_sample_time);

  if (pointcloud->totpoint != usd_positions.size()) {
    /* Size changed so we must reallocate. */
    pointcloud = BKE_pointcloud_new_nomain(usd_positions.size());
  }

  /* Update point positions and radii */
  static_assert(sizeof(pxr::GfVec3f) == sizeof(float3));
  MutableSpan<float3> positions = pointcloud->positions_for_write();
  positions.copy_from(Span(usd_positions.cdata(), usd_positions.size()).cast<float3>());

  pxr::VtFloatArray usd_widths;
  points_prim_.GetWidthsAttr().Get(&usd_widths, params.motion_sample_time);

  if (!usd_widths.empty()) {
    MutableSpan<float> radii = pointcloud->radius_for_write();
    Span<float> widths = Span(usd_widths.cdata(), usd_widths.size());

    const pxr::TfToken widths_interp = points_prim_.GetWidthsInterpolation();
    if (widths_interp == pxr::UsdGeomTokens->constant) {
      radii.fill(widths[0] / 2.0f);
    }
    else {
      for (int i_point = 0; i_point < widths.size(); i_point++) {
        radii[i_point] = widths[i_point] / 2.0f;
      }
    }
  }

  /* TODO: Read in ID and normal data.
   * See UsdGeomPoints::GetIdsAttr and UsdGeomPointBased::GetNormalsAttr */

  /* Read in velocity and generic data. */
  read_velocities(pointcloud, params.motion_sample_time);
  read_custom_data(pointcloud, params.motion_sample_time);

  geometry_set.replace_pointcloud(pointcloud);
}

void USDPointsReader::read_velocities(PointCloud *pointcloud, const pxr::UsdTimeCode time) const
{
  pxr::VtVec3fArray velocities;
  points_prim_.GetVelocitiesAttr().Get(&velocities, time);

  if (!velocities.empty()) {
    bke::MutableAttributeAccessor attributes = pointcloud->attributes_for_write();
    bke::SpanAttributeWriter<float3> velocity =
        attributes.lookup_or_add_for_write_only_span<float3>("velocity", bke::AttrDomain::Point);

    Span<pxr::GfVec3f> usd_data(velocities.cdata(), velocities.size());
    velocity.span.copy_from(usd_data.cast<float3>());
    velocity.finish();
  }
}

void USDPointsReader::read_custom_data(PointCloud *pointcloud, const pxr::UsdTimeCode time) const
{
  pxr::UsdGeomPrimvarsAPI pv_api(points_prim_);

  std::vector<pxr::UsdGeomPrimvar> primvars = pv_api.GetPrimvarsWithValues();
  for (const pxr::UsdGeomPrimvar &pv : primvars) {
    const pxr::SdfValueTypeName pv_type = pv.GetTypeName();
    if (!pv_type.IsArray()) {
      continue; /* Skip non-array primvar attributes. */
    }

    const bke::AttrDomain domain = bke::AttrDomain::Point;
    const std::optional<bke::AttrType> type = convert_usd_type_to_blender(pv_type);
    if (!type.has_value()) {
      return;
    }

    bke::MutableAttributeAccessor attributes = pointcloud->attributes_for_write();
    copy_primvar_to_blender_attribute(pv, time, *type, domain, {}, attributes);
  }
}

bool USDPointsReader::is_animated() const
{
  if (!points_prim_) {
    return false;
  }

  bool is_animated = false;

  is_animated |= points_prim_.GetPointsAttr().ValueMightBeTimeVarying();

  is_animated |= points_prim_.GetVelocitiesAttr().ValueMightBeTimeVarying();

  is_animated |= points_prim_.GetWidthsAttr().ValueMightBeTimeVarying();

  pxr::UsdGeomPrimvarsAPI pv_api(points_prim_);
  std::vector<pxr::UsdGeomPrimvar> primvars = pv_api.GetPrimvarsWithValues();
  for (const pxr::UsdGeomPrimvar &pv : primvars) {
    is_animated |= pv.ValueMightBeTimeVarying();
  }

  return is_animated;
}

}  // namespace blender::io::usd
