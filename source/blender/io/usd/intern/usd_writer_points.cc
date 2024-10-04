/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_writer_points.hh"
#include "usd_attribute_utils.hh"
#include "usd_utils.hh"

#include "BKE_attribute.hh"
#include "BKE_report.hh"

#include "DNA_pointcloud_types.h"

#include <pxr/base/vt/array.h>
#include <pxr/usd/usdGeom/points.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>

namespace blender::io::usd {

void USDPointsWriter::do_write(HierarchyContext &context)
{
  const pxr::UsdStageRefPtr stage = usd_export_context_.stage;
  const pxr::SdfPath &usd_path = usd_export_context_.usd_path;
  const pxr::UsdTimeCode timecode = get_export_time_code();

  const PointCloud *points = static_cast<const PointCloud *>(context.object->data);
  Span<pxr::GfVec3f> positions = points->positions().cast<pxr::GfVec3f>();
  VArray<float> radii = *points->attributes().lookup<float>("radius", bke::AttrDomain::Point);

  const pxr::UsdGeomPoints usd_points = pxr::UsdGeomPoints::Define(stage, usd_path);

  pxr::VtArray<pxr::GfVec3f> usd_positions;
  usd_positions.assign(positions.begin(), positions.end());

  pxr::UsdAttribute attr_positions = usd_points.CreatePointsAttr(pxr::VtValue(), true);
  if (!attr_positions.HasValue()) {
    attr_positions.Set(usd_positions, pxr::UsdTimeCode::Default());
  }
  usd_value_writer_.SetAttribute(attr_positions, usd_positions, timecode);

  if (!radii.is_empty()) {
    pxr::VtArray<float> usd_widths;
    usd_widths.resize(radii.size());
    for (const int i : radii.index_range()) {
      usd_widths[i] = radii[i] * 2.0f;
    }

    pxr::UsdAttribute attr_widths = usd_points.CreateWidthsAttr(pxr::VtValue(), true);
    if (!attr_widths.HasValue()) {
      attr_widths.Set(usd_widths, pxr::UsdTimeCode::Default());
    }
    usd_value_writer_.SetAttribute(attr_widths, usd_widths, timecode);
  }

  this->write_velocities(points, usd_points, timecode);
  this->write_custom_data(points, usd_points, timecode);

  const pxr::UsdPrim usd_prim = usd_points.GetPrim();
  this->set_extents(usd_prim, timecode);
}

static std::optional<pxr::TfToken> convert_blender_domain_to_usd(
    const bke::AttrDomain blender_domain)
{
  switch (blender_domain) {
    case bke::AttrDomain::Point:
      return pxr::UsdGeomTokens->varying;

    default:
      return std::nullopt;
  }
}

void USDPointsWriter::write_generic_data(const bke::AttributeIter &attr,
                                         const pxr::UsdGeomPoints &usd_points,
                                         const pxr::UsdTimeCode timecode)
{
  const std::optional<pxr::TfToken> pv_interp = convert_blender_domain_to_usd(attr.domain);
  const std::optional<pxr::SdfValueTypeName> pv_type = convert_blender_type_to_usd(attr.data_type);

  if (!pv_interp || !pv_type) {
    BKE_reportf(this->reports(),
                RPT_WARNING,
                "Attribute '%s' (Blender domain %d, type %d) cannot be converted to USD",
                attr.name.c_str(),
                int(attr.domain),
                attr.data_type);
    return;
  }

  const GVArray attribute = *attr.get();
  if (attribute.is_empty()) {
    return;
  }

  const pxr::TfToken pv_name(
      make_safe_name(attr.name, usd_export_context_.export_params.allow_unicode));
  const pxr::UsdGeomPrimvarsAPI pv_api = pxr::UsdGeomPrimvarsAPI(usd_points);

  pxr::UsdGeomPrimvar pv_attr = pv_api.CreatePrimvar(pv_name, *pv_type, *pv_interp);

  copy_blender_attribute_to_primvar(
      attribute, attr.data_type, timecode, pv_attr, usd_value_writer_);
}

void USDPointsWriter::write_custom_data(const PointCloud *points,
                                        const pxr::UsdGeomPoints &usd_points,
                                        const pxr::UsdTimeCode timecode)
{
  const bke::AttributeAccessor attributes = points->attributes();

  attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    /* Skip "internal" Blender properties and attributes dealt with elsewhere. */
    if (iter.name[0] == '.' || bke::attribute_name_is_anonymous(iter.name) ||
        ELEM(iter.name, "position", "radius", "id", "velocity"))
    {
      return;
    }

    this->write_generic_data(iter, usd_points, timecode);
  });
}

void USDPointsWriter::write_velocities(const PointCloud *points,
                                       const pxr::UsdGeomPoints &usd_points,
                                       const pxr::UsdTimeCode timecode)
{
  const VArraySpan velocity = *points->attributes().lookup<float3>(
      "velocity", blender::bke::AttrDomain::Point);
  if (velocity.is_empty()) {
    return;
  }

  Span<pxr::GfVec3f> data = velocity.cast<pxr::GfVec3f>();
  pxr::VtArray<pxr::GfVec3f> usd_velocities;
  usd_velocities.assign(data.begin(), data.end());

  pxr::UsdAttribute attr_vel = usd_points.CreateVelocitiesAttr(pxr::VtValue(), true);
  if (!attr_vel.HasValue()) {
    attr_vel.Set(usd_velocities, pxr::UsdTimeCode::Default());
  }

  usd_value_writer_.SetAttribute(attr_vel, usd_velocities, timecode);
}

void USDPointsWriter::set_extents(const pxr::UsdPrim &prim, const pxr::UsdTimeCode timecode)
{
  pxr::UsdGeomBoundable boundable(prim);

  pxr::VtArray<pxr::GfVec3f> extent;
  pxr::UsdGeomBoundable::ComputeExtentFromPlugins(boundable, timecode, &extent);

  pxr::UsdAttribute attr_extent = boundable.CreateExtentAttr(pxr::VtValue(), true);
  if (!attr_extent.HasValue()) {
    attr_extent.Set(extent, pxr::UsdTimeCode::Default());
  }

  usd_value_writer_.SetAttribute(attr_extent, extent, timecode);
}

}  // namespace blender::io::usd
