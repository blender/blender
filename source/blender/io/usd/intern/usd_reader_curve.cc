/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Adapted from the Blender Alembic importer implementation. Copyright 2016 Kévin Dietrich.
 * Modifications Copyright 2021 Tangent Animation. All rights reserved. */

#include "usd_reader_curve.hh"

#include "BKE_attribute.hh"
#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_object.hh"

#include "BLI_index_range.hh"
#include "BLI_math_vector_types.hh"

#include "DNA_curves_types.h"
#include "DNA_object_types.h"

#include <pxr/base/vt/array.h>
#include <pxr/base/vt/types.h>
#include <pxr/usd/usdGeom/basisCurves.h>
#include <pxr/usd/usdGeom/curves.h>

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
  else {
    return usdCount;
  }
}

/** Return the sum of the values of each element in `usdCounts`. This is used for precomputing the
 * total number of points for all curves in some curve primitive. */
static int accumulate_point_count(const pxr::VtIntArray &usdCounts,
                                  CurveType curve_type,
                                  bool is_cyclic)
{
  int result = 0;
  for (int v : usdCounts) {
    result += point_count(v, curve_type, is_cyclic);
  }
  return result;
}

static void add_bezier_control_point(int cp,
                                     int offset,
                                     MutableSpan<float3> positions,
                                     MutableSpan<float3> handles_left,
                                     MutableSpan<float3> handles_right,
                                     const Span<pxr::GfVec3f> &usdPoints)
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
static bool curves_topology_changed(const CurvesGeometry &geometry,
                                    const pxr::VtIntArray &usdCounts,
                                    CurveType curve_type,
                                    int expected_total_point_num,
                                    bool is_cyclic)
{
  if (geometry.curve_num != usdCounts.size()) {
    return true;
  }
  if (geometry.point_num != expected_total_point_num) {
    return true;
  }

  for (const int curve_idx : IndexRange(geometry.curve_num)) {
    const int expected_curve_point_num = point_count(usdCounts[curve_idx], curve_type, is_cyclic);
    const int current_curve_point_num = geometry.curve_offsets[curve_idx];

    if (current_curve_point_num != expected_curve_point_num) {
      return true;
    }
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

void USDCurvesReader::read_curve_sample(Curves *cu, const double motionSampleTime)
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
  const int num_subcurves = usdCounts.size();
  const int num_points = accumulate_point_count(usdCounts, curve_type, is_cyclic);
  const int default_resolution = 6;

  bke::CurvesGeometry &geometry = cu->geometry.wrap();
  if (curves_topology_changed(geometry, usdCounts, curve_type, num_points, is_cyclic)) {
    geometry.resize(num_points, num_subcurves);
  }

  geometry.fill_curve_types(curve_type);
  geometry.resolution_for_write().fill(default_resolution);

  if (is_cyclic) {
    geometry.cyclic_for_write().fill(true);
  }

  if (curve_type == CURVE_TYPE_NURBS) {
    const int8_t curve_order = type == pxr::UsdGeomTokens->cubic ? 4 : 2;
    geometry.nurbs_orders_for_write().fill(curve_order);
  }

  MutableSpan<int> offsets = geometry.offsets_for_write();
  MutableSpan<float3> positions = geometry.positions_for_write();

  /* Bezier curves require care in filing out their left/right handles. */
  if (type == pxr::UsdGeomTokens->cubic && basis == pxr::UsdGeomTokens->bezier) {
    geometry.handle_types_left_for_write().fill(BEZIER_HANDLE_ALIGN);
    geometry.handle_types_right_for_write().fill(BEZIER_HANDLE_ALIGN);

    MutableSpan<float3> handles_right = geometry.handle_positions_right_for_write();
    MutableSpan<float3> handles_left = geometry.handle_positions_left_for_write();
    Span<pxr::GfVec3f> points{usdPoints.data(), int64_t(usdPoints.size())};

    int usd_point_offset = 0;
    int point_offset = 0;
    for (const int i : IndexRange(num_subcurves)) {
      const int usd_point_count = usdCounts[i];
      const int point_count = bezier_point_count(usd_point_count, is_cyclic);

      offsets[i] = point_offset;

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
    int offset = 0;
    for (const int i : IndexRange(num_subcurves)) {
      const int num_verts = usdCounts[i];
      offsets[i] = offset;
      offset += num_verts;
    }

    for (const int i_point : geometry.points_range()) {
      positions[i_point] = to_float3(usdPoints[i_point]);
    }
  }

  if (!usdWidths.empty()) {
    bke::SpanAttributeWriter<float> radii =
        geometry.attributes_for_write().lookup_or_add_for_write_span<float>(
            "radius", bke::AttrDomain::Point);

    pxr::TfToken widths_interp = curve_prim_.GetWidthsInterpolation();
    if (widths_interp == pxr::UsdGeomTokens->constant) {
      radii.span.fill(usdWidths[0] / 2);
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
        for (const int i : IndexRange(num_subcurves)) {
          const int usd_point_count = usdCounts[i];
          const int point_count = bezier_point_count(usd_point_count, is_cyclic);

          int cp_offset = 0;
          for (const int cp : IndexRange(point_count)) {
            radii.span[point_offset + cp] = usdWidths[usd_point_offset + cp_offset] / 2;
            cp_offset += 3;
          }

          point_offset += point_count;
          usd_point_offset += usd_point_count;
        }
      }
      else {
        for (const int i_point : geometry.points_range()) {
          radii.span[i_point] = usdWidths[i_point] / 2;
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
