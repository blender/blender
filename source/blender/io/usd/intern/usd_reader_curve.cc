/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Adapted from the Blender Alembic importer implementation. Copyright 2016 Kévin Dietrich.
 * Modifications Copyright 2021 Tangent Animation. All rights reserved. */

#include "usd_reader_curve.hh"
#include "usd.hh"
#include "usd_attribute_utils.hh"
#include "usd_hash_types.hh"

#include "BKE_attribute.hh"
#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"

#include "BLI_index_range.hh"
#include "BLI_math_vector_types.hh"

#include "DNA_curves_types.h"
#include "DNA_object_types.h"

#include <pxr/base/vt/types.h>
#include <pxr/usd/usdGeom/basisCurves.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>

namespace blender::io::usd {

static inline float3 to_float3(pxr::GfVec3f vec3f)
{
  return float3(vec3f.data());
}

static inline int bezier_point_count(int usd_count, bool is_cyclic)
{
  return is_cyclic ? (usd_count / 3) : ((usd_count / 3) + 1);
}

static int point_count(int usdCount, CurveType curve_type, bool is_cyclic)
{
  if (curve_type == CURVE_TYPE_BEZIER) {
    return bezier_point_count(usdCount, is_cyclic);
  }
  return usdCount;
}

static Array<int> calc_curve_offsets(const pxr::VtIntArray &usdCounts,
                                     const CurveType curve_type,
                                     bool is_cyclic)
{
  Array<int> offsets(usdCounts.size() + 1);
  threading::parallel_for(IndexRange(usdCounts.size()), 4096, [&](const IndexRange range) {
    for (const int i : range) {
      offsets[i] = point_count(usdCounts[i], curve_type, is_cyclic);
    }
  });
  offset_indices::accumulate_counts_to_offsets(offsets);
  return offsets;
}

static void add_bezier_control_point(int cp,
                                     int offset,
                                     MutableSpan<float3> positions,
                                     MutableSpan<float3> handles_left,
                                     MutableSpan<float3> handles_right,
                                     const Span<pxr::GfVec3f> usdPoints)
{
  if (offset == 0) {
    positions[cp] = to_float3(usdPoints[offset]);
    handles_right[cp] = to_float3(usdPoints[offset + 1]);
    handles_left[cp] = 2.0f * positions[cp] - handles_right[cp];
  }
  else if (offset == usdPoints.size() - 1) {
    positions[cp] = to_float3(usdPoints[offset]);
    handles_left[cp] = to_float3(usdPoints[offset - 1]);
    handles_right[cp] = 2.0f * positions[cp] - handles_left[cp];
  }
  else {
    positions[cp] = to_float3(usdPoints[offset]);
    handles_left[cp] = to_float3(usdPoints[offset - 1]);
    handles_right[cp] = to_float3(usdPoints[offset + 1]);
  }
}

/** Returns true if the number of curves or the number of curve points in each curve differ. */
static bool curves_topology_changed(const bke::CurvesGeometry &curves, const Span<int> usd_offsets)
{
  if (curves.offsets() != usd_offsets) {
    return true;
  }

  return false;
}

static CurveType get_curve_type(pxr::TfToken type, pxr::TfToken basis)
{
  if (type == pxr::UsdGeomTokens->cubic) {
    if (basis == pxr::UsdGeomTokens->bezier) {
      return CURVE_TYPE_BEZIER;
    }
    if (basis == pxr::UsdGeomTokens->bspline) {
      return CURVE_TYPE_NURBS;
    }
    if (basis == pxr::UsdGeomTokens->catmullRom) {
      return CURVE_TYPE_CATMULL_ROM;
    }
  }

  return CURVE_TYPE_POLY;
}

static std::optional<bke::AttrDomain> convert_usd_interp_to_blender(const pxr::TfToken usd_domain)
{
  static const blender::Map<pxr::TfToken, bke::AttrDomain> domain_map = []() {
    blender::Map<pxr::TfToken, bke::AttrDomain> map;
    map.add_new(pxr::UsdGeomTokens->vertex, bke::AttrDomain::Point);
    map.add_new(pxr::UsdGeomTokens->varying, bke::AttrDomain::Point);
    map.add_new(pxr::UsdGeomTokens->constant, bke::AttrDomain::Curve);
    map.add_new(pxr::UsdGeomTokens->uniform, bke::AttrDomain::Curve);
    return map;
  }();

  const bke::AttrDomain *value = domain_map.lookup_ptr(usd_domain);

  if (value == nullptr) {
    return std::nullopt;
  }

  return *value;
}

void USDCurvesReader::create_object(Main *bmain)
{
  Curves *curve = BKE_curves_add(bmain, name_.c_str());

  object_ = BKE_object_add_only_object(bmain, OB_CURVES, name_.c_str());
  object_->data = curve;
}

void USDCurvesReader::read_object_data(Main *bmain, pxr::UsdTimeCode time)
{
  Curves *cu = (Curves *)object_->data;
  this->read_curve_sample(cu, time);

  if (this->is_animated()) {
    this->add_cache_modifier();
  }

  USDXformReader::read_object_data(bmain, time);
}

void USDCurvesReader::read_velocities(bke::CurvesGeometry &curves,
                                      const pxr::UsdGeomCurves &usd_curves,
                                      const pxr::UsdTimeCode time) const
{
  pxr::VtVec3fArray velocities;
  usd_curves.GetVelocitiesAttr().Get(&velocities, time);

  if (!velocities.empty()) {
    bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
    bke::SpanAttributeWriter<float3> velocity =
        attributes.lookup_or_add_for_write_only_span<float3>("velocity", bke::AttrDomain::Point);

    Span<pxr::GfVec3f> usd_data(velocities.cdata(), velocities.size());
    velocity.span.copy_from(usd_data.cast<float3>());
    velocity.finish();
  }
}

void USDCurvesReader::read_custom_data(bke::CurvesGeometry &curves,
                                       const pxr::UsdTimeCode time) const
{
  pxr::UsdGeomPrimvarsAPI pv_api(prim_);

  std::vector<pxr::UsdGeomPrimvar> primvars = pv_api.GetPrimvarsWithValues();
  for (const pxr::UsdGeomPrimvar &pv : primvars) {
    const pxr::SdfValueTypeName pv_type = pv.GetTypeName();
    if (!pv_type.IsArray()) {
      continue; /* Skip non-array primvar attributes. */
    }

    const pxr::TfToken pv_interp = pv.GetInterpolation();
    const std::optional<bke::AttrDomain> domain = convert_usd_interp_to_blender(pv_interp);
    const std::optional<bke::AttrType> type = convert_usd_type_to_blender(pv_type);

    if (!domain.has_value() || !type.has_value()) {
      const pxr::TfToken pv_name = pxr::UsdGeomPrimvar::StripPrimvarsName(pv.GetPrimvarName());
      BKE_reportf(reports(),
                  RPT_WARNING,
                  "Primvar '%s' (interpolation %s, type %s) cannot be converted to Blender",
                  pv_name.GetText(),
                  pv_interp.GetText(),
                  pv_type.GetAsToken().GetText());
      continue;
    }

    bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
    copy_primvar_to_blender_attribute(pv, time, *type, *domain, {}, attributes);
  }
}

void USDCurvesReader::read_geometry(bke::GeometrySet &geometry_set,
                                    const USDMeshReadParams params,
                                    const char ** /*r_err_str*/)
{
  if (!geometry_set.has_curves()) {
    return;
  }

  Curves *curves = geometry_set.get_curves_for_write();
  read_curve_sample(curves, params.motion_sample_time);
}

bool USDBasisCurvesReader::is_animated() const
{
  if (curve_prim_.GetPointsAttr().ValueMightBeTimeVarying() ||
      curve_prim_.GetWidthsAttr().ValueMightBeTimeVarying() ||
      curve_prim_.GetVelocitiesAttr().ValueMightBeTimeVarying())
  {
    return true;
  }

  pxr::UsdGeomPrimvarsAPI pv_api(curve_prim_);
  for (const pxr::UsdGeomPrimvar &pv : pv_api.GetPrimvarsWithValues()) {
    if (pv.ValueMightBeTimeVarying()) {
      return true;
    }
  }

  return false;
}

void USDBasisCurvesReader::read_curve_sample(Curves *curves_id, const pxr::UsdTimeCode time)
{
  pxr::VtIntArray usd_counts;
  pxr::VtVec3fArray usd_points;
  pxr::VtFloatArray usd_widths;
  pxr::TfToken basis;
  pxr::TfToken type;
  pxr::TfToken wrap;

  curve_prim_.GetCurveVertexCountsAttr().Get(&usd_counts, time);
  curve_prim_.GetPointsAttr().Get(&usd_points, time);
  curve_prim_.GetWidthsAttr().Get(&usd_widths, time);
  curve_prim_.GetBasisAttr().Get(&basis, time);
  curve_prim_.GetTypeAttr().Get(&type, time);
  curve_prim_.GetWrapAttr().Get(&wrap, time);

  const CurveType curve_type = get_curve_type(type, basis);
  const bool is_cyclic = wrap == pxr::UsdGeomTokens->periodic;
  const int curves_num = usd_counts.size();
  const Array<int> new_offsets = calc_curve_offsets(usd_counts, curve_type, is_cyclic);

  bke::CurvesGeometry &curves = curves_id->geometry.wrap();
  if (curves_topology_changed(curves, new_offsets)) {
    curves.resize(new_offsets.last(), curves_num);
  }

  curves.offsets_for_write().copy_from(new_offsets);

  curves.fill_curve_types(curve_type);

  if (is_cyclic) {
    curves.cyclic_for_write().fill(true);
  }

  if (curve_type == CURVE_TYPE_NURBS) {
    const int8_t curve_order = type == pxr::UsdGeomTokens->cubic ? 4 : 2;
    curves.nurbs_orders_for_write().fill(curve_order);
  }

  MutableSpan<float3> positions = curves.positions_for_write();
  Span<pxr::GfVec3f> points = Span(usd_points.cdata(), usd_points.size());
  Span<int> counts = Span(usd_counts.cdata(), usd_counts.size());

  /* Bezier curves require care in filing out their left/right handles. */
  if (type == pxr::UsdGeomTokens->cubic && basis == pxr::UsdGeomTokens->bezier) {
    curves.handle_types_left_for_write().fill(BEZIER_HANDLE_ALIGN);
    curves.handle_types_right_for_write().fill(BEZIER_HANDLE_ALIGN);

    MutableSpan<float3> handles_right = curves.handle_positions_right_for_write();
    MutableSpan<float3> handles_left = curves.handle_positions_left_for_write();

    int usd_point_offset = 0;
    int point_offset = 0;
    for (const int i : curves.curves_range()) {
      const int usd_point_count = counts[i];
      const int point_count = bezier_point_count(usd_point_count, is_cyclic);

      int cp_offset = 0;
      for (const int cp : IndexRange(point_count)) {
        add_bezier_control_point(cp,
                                 cp_offset,
                                 positions.slice(point_offset, point_count),
                                 handles_left.slice(point_offset, point_count),
                                 handles_right.slice(point_offset, point_count),
                                 points.slice(usd_point_offset, usd_point_count));
        cp_offset += 3;
      }

      point_offset += point_count;
      usd_point_offset += usd_point_count;
    }
  }
  else {
    static_assert(sizeof(pxr::GfVec3f) == sizeof(float3));
    positions.copy_from(points.cast<float3>());
  }

  if (!usd_widths.empty()) {
    MutableSpan<float> radii = curves.radius_for_write();
    Span<float> widths = Span(usd_widths.cdata(), usd_widths.size());

    pxr::TfToken widths_interp = curve_prim_.GetWidthsInterpolation();
    if (widths_interp == pxr::UsdGeomTokens->constant) {
      radii.fill(widths[0] / 2.0f);
    }
    else {
      const bool is_bezier_vertex_interp = (type == pxr::UsdGeomTokens->cubic &&
                                            basis == pxr::UsdGeomTokens->bezier &&
                                            widths_interp == pxr::UsdGeomTokens->vertex);
      if (is_bezier_vertex_interp) {
        /* Blender does not support 'vertex-varying' interpolation.
         * Assign the widths as-if it were 'varying' only. */
        int usd_point_offset = 0;
        int point_offset = 0;
        for (const int i : curves.curves_range()) {
          const int usd_point_count = counts[i];
          const int point_count = bezier_point_count(usd_point_count, is_cyclic);

          int cp_offset = 0;
          for (const int cp : IndexRange(point_count)) {
            radii[point_offset + cp] = widths[usd_point_offset + cp_offset] / 2.0f;
            cp_offset += 3;
          }

          point_offset += point_count;
          usd_point_offset += usd_point_count;
        }
      }
      else {
        for (const int i_point : curves.points_range()) {
          radii[i_point] = widths[i_point] / 2.0f;
        }
      }
    }
  }

  this->read_velocities(curves, curve_prim_, time);
  this->read_custom_data(curves, time);
}

}  // namespace blender::io::usd
