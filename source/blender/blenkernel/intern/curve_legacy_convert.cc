/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_task.hh"
#include "BLI_vector.hh"

#include "DNA_curve_types.h"
#include "DNA_curves_types.h"

#include "BKE_curve.h"
#include "BKE_curve_legacy_convert.hh"
#include "BKE_curves.hh"
#include "BKE_curves_utils.hh"
#include "BKE_geometry_set.hh"

namespace blender::bke {

static CurveType curve_type_from_legacy(const short type)
{
  switch (type) {
    case CU_POLY:
      return CURVE_TYPE_POLY;
    case CU_BEZIER:
      return CURVE_TYPE_BEZIER;
    case CU_NURBS:
      return CURVE_TYPE_NURBS;
  }
  BLI_assert_unreachable();
  return CURVE_TYPE_POLY;
}

static HandleType handle_type_from_legacy(const uint8_t handle_type_legacy)
{
  switch (handle_type_legacy) {
    case HD_FREE:
      return BEZIER_HANDLE_FREE;
    case HD_AUTO:
      return BEZIER_HANDLE_AUTO;
    case HD_VECT:
      return BEZIER_HANDLE_VECTOR;
    case HD_ALIGN:
      return BEZIER_HANDLE_ALIGN;
    case HD_AUTO_ANIM:
      return BEZIER_HANDLE_AUTO;
    case HD_ALIGN_DOUBLESIDE:
      return BEZIER_HANDLE_ALIGN;
  }
  BLI_assert_unreachable();
  return BEZIER_HANDLE_AUTO;
}

static NormalMode normal_mode_from_legacy(const short twist_mode)
{
  switch (twist_mode) {
    case CU_TWIST_Z_UP:
    case CU_TWIST_TANGENT:
      return NORMAL_MODE_Z_UP;
    case CU_TWIST_MINIMUM:
      return NORMAL_MODE_MINIMUM_TWIST;
  }
  BLI_assert_unreachable();
  return NORMAL_MODE_MINIMUM_TWIST;
}

static KnotsMode knots_mode_from_legacy(const short flag)
{
  switch (flag & (CU_NURB_ENDPOINT | CU_NURB_BEZIER)) {
    case CU_NURB_ENDPOINT:
      return NURBS_KNOT_MODE_ENDPOINT;
    case CU_NURB_BEZIER:
      return NURBS_KNOT_MODE_BEZIER;
    case CU_NURB_ENDPOINT | CU_NURB_BEZIER:
      return NURBS_KNOT_MODE_ENDPOINT_BEZIER;
    case 0:
      return NURBS_KNOT_MODE_NORMAL;
  }
  BLI_assert_unreachable();
  return NURBS_KNOT_MODE_NORMAL;
}

Curves *curve_legacy_to_curves(const Curve &curve_legacy, const ListBase &nurbs_list)
{
  const Vector<const Nurb *> src_curves(nurbs_list);
  if (src_curves.is_empty()) {
    return nullptr;
  }

  Curves *curves_id = curves_new_nomain(0, src_curves.size());
  CurvesGeometry &curves = curves_id->geometry.wrap();
  MutableAttributeAccessor curves_attributes = curves.attributes_for_write();

  MutableSpan<int8_t> types = curves.curve_types_for_write();
  MutableSpan<bool> cyclic = curves.cyclic_for_write();

  int offset = 0;
  MutableSpan<int> offsets = curves.offsets_for_write();
  for (const int i : src_curves.index_range()) {
    offsets[i] = offset;

    const Nurb &src_curve = *src_curves[i];
    types[i] = curve_type_from_legacy(src_curve.type);
    cyclic[i] = src_curve.flagu & CU_NURB_CYCLIC;

    offset += src_curve.pntsu;
  }
  offsets.last() = offset;
  curves.resize(curves.offsets().last(), curves.curves_num());

  curves.update_curve_types();

  const OffsetIndices points_by_curve = curves.points_by_curve();
  MutableSpan<float3> positions = curves.positions_for_write();
  SpanAttributeWriter<float> radius_attribute =
      curves_attributes.lookup_or_add_for_write_only_span<float>("radius", ATTR_DOMAIN_POINT);
  MutableSpan<float> radii = radius_attribute.span;
  MutableSpan<float> tilts = curves.tilt_for_write();

  auto create_poly = [&](const IndexMask &selection) {
    selection.foreach_index(GrainSize(256), [&](const int curve_i) {
      const Nurb &src_curve = *src_curves[curve_i];
      const Span<BPoint> src_points(src_curve.bp, src_curve.pntsu);
      const IndexRange points = points_by_curve[curve_i];

      for (const int i : src_points.index_range()) {
        const BPoint &bp = src_points[i];
        positions[points[i]] = bp.vec;
        radii[points[i]] = bp.radius;
        tilts[points[i]] = bp.tilt;
      }
    });
  };

  /* NOTE: For curve handles, legacy curves can end up in invalid situations where the handle
   * positions don't agree with the types because of evaluation, or because one-sided aligned
   * handles weren't considered. While recalculating automatic handles to fix those situations
   * is an option, currently this opts not to for the sake of flexibility. */
  auto create_bezier = [&](const IndexMask &selection) {
    MutableSpan<int> resolutions = curves.resolution_for_write();
    MutableSpan<float3> handle_positions_l = curves.handle_positions_left_for_write();
    MutableSpan<float3> handle_positions_r = curves.handle_positions_right_for_write();
    MutableSpan<int8_t> handle_types_l = curves.handle_types_left_for_write();
    MutableSpan<int8_t> handle_types_r = curves.handle_types_right_for_write();

    selection.foreach_index(GrainSize(256), [&](const int curve_i) {
      const Nurb &src_curve = *src_curves[curve_i];
      const Span<BezTriple> src_points(src_curve.bezt, src_curve.pntsu);
      const IndexRange points = points_by_curve[curve_i];

      resolutions[curve_i] = src_curve.resolu;

      for (const int i : src_points.index_range()) {
        const BezTriple &point = src_points[i];
        positions[points[i]] = point.vec[1];
        handle_positions_l[points[i]] = point.vec[0];
        handle_types_l[points[i]] = handle_type_from_legacy(point.h1);
        handle_positions_r[points[i]] = point.vec[2];
        handle_types_r[points[i]] = handle_type_from_legacy(point.h2);
        radii[points[i]] = point.radius;
        tilts[points[i]] = point.tilt;
      }
    });
  };

  auto create_nurbs = [&](const IndexMask &selection) {
    MutableSpan<int> resolutions = curves.resolution_for_write();
    MutableSpan<float> nurbs_weights = curves.nurbs_weights_for_write();
    MutableSpan<int8_t> nurbs_orders = curves.nurbs_orders_for_write();
    MutableSpan<int8_t> nurbs_knots_modes = curves.nurbs_knots_modes_for_write();

    selection.foreach_index(GrainSize(256), [&](const int curve_i) {
      const Nurb &src_curve = *src_curves[curve_i];
      const Span src_points(src_curve.bp, src_curve.pntsu);
      const IndexRange points = points_by_curve[curve_i];

      resolutions[curve_i] = src_curve.resolu;
      nurbs_orders[curve_i] = src_curve.orderu;
      nurbs_knots_modes[curve_i] = knots_mode_from_legacy(src_curve.flagu);

      for (const int i : src_points.index_range()) {
        const BPoint &bp = src_points[i];
        positions[points[i]] = bp.vec;
        radii[points[i]] = bp.radius;
        tilts[points[i]] = bp.tilt;
        nurbs_weights[points[i]] = bp.vec[3];
      }
    });
  };

  bke::curves::foreach_curve_by_type(
      curves.curve_types(),
      curves.curve_type_counts(),
      curves.curves_range(),
      [&](const IndexMask & /*selection*/) { BLI_assert_unreachable(); },
      create_poly,
      create_bezier,
      create_nurbs);

  curves.normal_mode_for_write().fill(normal_mode_from_legacy(curve_legacy.twist_mode));

  radius_attribute.finish();

  return curves_id;
}

Curves *curve_legacy_to_curves(const Curve &curve_legacy)
{
  return curve_legacy_to_curves(curve_legacy, *BKE_curve_nurbs_get_for_read(&curve_legacy));
}

}  // namespace blender::bke
