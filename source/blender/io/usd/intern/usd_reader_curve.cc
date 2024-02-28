/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Adapted from the Blender Alembic importer implementation. Copyright 2016 Kévin Dietrich.
 * Modifications Copyright 2021 Tangent Animation. All rights reserved. */

#include "usd_reader_curve.hh"
#include "usd.hh"

#include "BKE_attribute.hh"
#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_object.hh"

#include "BLI_index_range.hh"
#include "BLI_math_vector_types.hh"

#include "DNA_curves_types.h"
#include "DNA_object_types.h"

#include <pxr/base/vt/types.h>
#include <pxr/usd/usdGeom/basisCurves.h>

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
  int offset = 0;
  for (const int i : offsets.index_range()) {
    offsets[i] = offset;
    offset += point_count(usdCounts[i], curve_type, is_cyclic);
  }
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

void USDCurvesReader::create_object(Main *bmain, const double /*motionSampleTime*/)
{
  curve_ = static_cast<Curves *>(BKE_curves_add(bmain, name_.c_str()));

  object_ = BKE_object_add_only_object(bmain, OB_CURVES, name_.c_str());
  object_->data = curve_;
}

void USDCurvesReader::read_object_data(Main *bmain, double motionSampleTime)
{
  Curves *cu = (Curves *)object_->data;
  read_curve_sample(cu, motionSampleTime);

  if (curve_prim_.GetPointsAttr().ValueMightBeTimeVarying()) {
    add_cache_modifier();
  }

  USDXformReader::read_object_data(bmain, motionSampleTime);
}

void USDCurvesReader::read_curve_sample(Curves *curves_id, const double motionSampleTime)
{
  curve_prim_ = pxr::UsdGeomBasisCurves(prim_);
  if (!curve_prim_) {
    return;
  }

  pxr::UsdAttribute widthsAttr = curve_prim_.GetWidthsAttr();
  pxr::UsdAttribute vertexAttr = curve_prim_.GetCurveVertexCountsAttr();
  pxr::UsdAttribute pointsAttr = curve_prim_.GetPointsAttr();

  pxr::VtIntArray usdCounts;
  vertexAttr.Get(&usdCounts, motionSampleTime);

  pxr::VtVec3fArray usdPoints;
  pointsAttr.Get(&usdPoints, motionSampleTime);

  pxr::VtFloatArray usdWidths;
  widthsAttr.Get(&usdWidths, motionSampleTime);

  pxr::UsdAttribute basisAttr = curve_prim_.GetBasisAttr();
  pxr::TfToken basis;
  basisAttr.Get(&basis, motionSampleTime);

  pxr::UsdAttribute typeAttr = curve_prim_.GetTypeAttr();
  pxr::TfToken type;
  typeAttr.Get(&type, motionSampleTime);

  pxr::UsdAttribute wrapAttr = curve_prim_.GetWrapAttr();
  pxr::TfToken wrap;
  wrapAttr.Get(&wrap, motionSampleTime);

  const CurveType curve_type = get_curve_type(type, basis);
  const bool is_cyclic = wrap == pxr::UsdGeomTokens->periodic;
  const int curves_num = usdCounts.size();
  const Array<int> new_offsets = calc_curve_offsets(usdCounts, curve_type, is_cyclic);

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

  /* Bezier curves require care in filing out their left/right handles. */
  if (type == pxr::UsdGeomTokens->cubic && basis == pxr::UsdGeomTokens->bezier) {
    curves.handle_types_left_for_write().fill(BEZIER_HANDLE_ALIGN);
    curves.handle_types_right_for_write().fill(BEZIER_HANDLE_ALIGN);

    MutableSpan<float3> handles_right = curves.handle_positions_right_for_write();
    MutableSpan<float3> handles_left = curves.handle_positions_left_for_write();
    Span<pxr::GfVec3f> points{usdPoints.data(), int64_t(usdPoints.size())};

    int usd_point_offset = 0;
    int point_offset = 0;
    for (const int i : curves.curves_range()) {
      const int usd_point_count = usdCounts[i];
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
    positions.copy_from(Span(usdPoints.data(), usdPoints.size()).cast<float3>());
  }

  if (!usdWidths.empty()) {
    bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
    bke::SpanAttributeWriter<float> radii = attributes.lookup_or_add_for_write_span<float>(
        "radius", bke::AttrDomain::Point);

    pxr::TfToken widths_interp = curve_prim_.GetWidthsInterpolation();
    if (widths_interp == pxr::UsdGeomTokens->constant) {
      radii.span.fill(usdWidths[0] / 2.0f);
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
          const int usd_point_count = usdCounts[i];
          const int point_count = bezier_point_count(usd_point_count, is_cyclic);

          int cp_offset = 0;
          for (const int cp : IndexRange(point_count)) {
            radii.span[point_offset + cp] = usdWidths[usd_point_offset + cp_offset] / 2.0f;
            cp_offset += 3;
          }

          point_offset += point_count;
          usd_point_offset += usd_point_count;
        }
      }
      else {
        for (const int i_point : curves.points_range()) {
          radii.span[i_point] = usdWidths[i_point] / 2.0f;
        }
      }
    }

    radii.finish();
  }
}

void USDCurvesReader::read_geometry(bke::GeometrySet &geometry_set,
                                    const USDMeshReadParams params,
                                    const char ** /*err_str*/)
{
  if (!curve_prim_) {
    return;
  }

  if (!geometry_set.has_curves()) {
    return;
  }

  Curves *curves = geometry_set.get_curves_for_write();
  read_curve_sample(curves, params.motion_sample_time);
}

}  // namespace blender::io::usd
