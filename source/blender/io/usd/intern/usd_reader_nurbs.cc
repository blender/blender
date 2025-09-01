/* SPDX-FileCopyrightText: 2021 Tangent Animation. All rights reserved.
 * SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Adapted from the Blender Alembic importer implementation. */

#include "usd_reader_nurbs.hh"

#include "BKE_curves.hh"

#include "BLI_offset_indices.hh"
#include "BLI_span.hh"

#include "DNA_curves_types.h"

#include <pxr/base/vt/types.h>

#include <pxr/usd/usdGeom/curves.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>

#include "CLG_log.h"
static CLG_LogRef LOG = {"io.usd"};

namespace blender::io::usd {

/* Store incoming USD data privately and expose Blender-friendly Spans publicly. */
struct USDCurveData {
 private:
  pxr::VtArray<pxr::GfVec3f> points_;
  pxr::VtArray<int> counts_;
  pxr::VtArray<int> orders_;
  pxr::VtArray<double> knots_;
  pxr::VtArray<double> weights_;
  pxr::VtArray<float> widths_;
  pxr::VtArray<pxr::GfVec3f> velocities_;

 public:
  Span<float3> points() const
  {
    return Span(points_.cdata(), points_.size()).cast<float3>();
  }
  Span<int> counts() const
  {
    return Span(counts_.cdata(), counts_.size());
  }
  Span<int> orders() const
  {
    return Span(orders_.cdata(), orders_.size());
  }
  Span<double> knots() const
  {
    return Span(knots_.cdata(), knots_.size());
  }
  Span<double> weights() const
  {
    return Span(weights_.cdata(), weights_.size());
  }
  Span<float> widths() const
  {
    return Span(widths_.cdata(), widths_.size());
  }
  Span<float3> velocities() const
  {
    return Span(velocities_.cdata(), velocities_.size()).cast<float3>();
  }

  bool load(const pxr::UsdGeomNurbsCurves &curve_prim, const pxr::UsdTimeCode time)
  {
    curve_prim.GetCurveVertexCountsAttr().Get(&counts_, time);
    curve_prim.GetOrderAttr().Get(&orders_, time);

    if (counts_.size() != orders_.size()) {
      CLOG_WARN(&LOG,
                "Curve vertex and order size mismatch for NURBS prim %s",
                curve_prim.GetPrim().GetPrimPath().GetAsString().c_str());
      return false;
    }

    if (std::any_of(counts_.cbegin(), counts_.cend(), [](int value) { return value < 0; }) ||
        std::any_of(orders_.cbegin(), orders_.cend(), [](int value) { return value < 0; }))
    {
      CLOG_WARN(&LOG,
                "Invalid curve vertex count or order value detected for NURBS prim %s",
                curve_prim.GetPrim().GetPrimPath().GetAsString().c_str());
      return false;
    }

    curve_prim.GetPointsAttr().Get(&points_, time);
    curve_prim.GetKnotsAttr().Get(&knots_, time);
    curve_prim.GetWidthsAttr().Get(&widths_, time);

    curve_prim.GetPointWeightsAttr().Get(&weights_, time);
    if (!weights_.empty() && points_.size() != weights_.size()) {
      CLOG_WARN(&LOG,
                "Invalid curve weights count for NURBS prim %s",
                curve_prim.GetPrim().GetPrimPath().GetAsString().c_str());

      /* Only clear, but continue to load other curve data. */
      weights_.clear();
    }

    curve_prim.GetVelocitiesAttr().Get(&velocities_, time);
    if (!velocities_.empty() && points_.size() != velocities_.size()) {
      CLOG_WARN(&LOG,
                "Invalid curve velocity count for NURBS prim %s",
                curve_prim.GetPrim().GetPrimPath().GetAsString().c_str());

      /* Only clear, but continue to load other curve data. */
      velocities_.clear();
    }

    return true;
  }
};

struct CurveData {
  Array<int> blender_offsets;
  Array<int> usd_offsets;
  Array<int> usd_knot_offsets;
  Array<bool> is_cyclic;
};

static KnotsMode determine_knots_mode(const Span<double> usd_knots,
                                      const int order,
                                      const bool is_cyclic)
{
  /* TODO: We have to convert knot values to float for usage in Blender APIs. Look into making
   * calculate_multiplicity_sequence a template. */
  Array<float> blender_knots(usd_knots.size());
  for (const int knot_i : usd_knots.index_range()) {
    blender_knots[knot_i] = float(usd_knots[knot_i]);
  }

  Vector<int> multiplicity = bke::curves::nurbs::calculate_multiplicity_sequence(blender_knots);
  const int head = multiplicity.first();
  const int tail = multiplicity.last();
  const Span<int> inner = multiplicity.as_span().slice(1, multiplicity.size() - 2);

  /* If the knot vector starts and ends with full multiplicity knots, then this is classified as
   * Blender's endpoint mode. */
  const int degree = order - 1;
  const bool is_endpoint = is_cyclic ? (tail >= degree) : (head == order && tail >= order);

  /* If all of the inner multiplicities are equal to the degree, then this is a Bezier curve. */
  if (degree > 1 &&
      std::all_of(inner.begin(), inner.end(), [degree](int value) { return value == degree; }))
  {
    return is_endpoint ? NURBS_KNOT_MODE_ENDPOINT_BEZIER : NURBS_KNOT_MODE_BEZIER;
  }

  if (is_endpoint) {
    return NURBS_KNOT_MODE_ENDPOINT;
  }

  /* If all of the inner knot values are equally spaced, then this is a regular/uniform curve and
   * we assume that our normal knot mode will match. Use custom knots otherwise. */
  const Span<float> inner_values = blender_knots.as_span().drop_front(head).drop_back(tail);
  if (inner_values.size() > 2) {
    const float delta = inner_values[1] - inner_values[0];
    if (delta < 0) {
      /* Invalid knot vector. Use normal mode. */
      return NURBS_KNOT_MODE_NORMAL;
    }
    for (int i = 2; i < inner.size(); i++) {
      if (inner_values[i] - inner_values[i - 1] != delta) {
        /* The knot values are not equally spaced. Use custom knots. */
        return NURBS_KNOT_MODE_CUSTOM;
      }
    }
  }

  /* Nothing matches. Use normal mode. */
  return NURBS_KNOT_MODE_NORMAL;
}

static CurveData calc_curve_offsets(const Span<float3> usd_points,
                                    const Span<int> usd_counts,
                                    const Span<int> usd_orders,
                                    const Span<double> usd_knots)
{
  CurveData data;
  data.blender_offsets.reinitialize(usd_counts.size() + 1);
  data.usd_offsets.reinitialize(usd_counts.size() + 1);
  data.usd_knot_offsets.reinitialize(usd_counts.size() + 1);
  data.is_cyclic.reinitialize(usd_counts.size());

  Span<float3> usd_remaining_points = usd_points;
  Span<double> usd_remaining_knots = usd_knots;

  for (const int curve_i : usd_counts.index_range()) {
    const int points_num = usd_counts[curve_i];
    const int knots_num = points_num + usd_orders[curve_i];
    const int degree = usd_orders[curve_i] - 1;
    const Span<double> usd_current_knots = usd_remaining_knots.take_front(knots_num);
    const Span<float3> usd_current_points = usd_remaining_points.take_front(points_num);
    if (knots_num < 4 || knots_num != usd_current_knots.size()) {
      data.is_cyclic[curve_i] = false;
    }
    else {
      data.is_cyclic[curve_i] = usd_current_points.take_front(degree) ==
                                usd_current_points.take_back(degree);
    }

    int blender_count = usd_counts[curve_i];

    /* Account for any repeated degree(order - 1) number of points from USD cyclic curves which
     * Blender does not use internally. */
    if (data.is_cyclic[curve_i]) {
      blender_count -= degree;
    }

    data.blender_offsets[curve_i] = blender_count;
    data.usd_offsets[curve_i] = points_num;
    data.usd_knot_offsets[curve_i] = knots_num;

    /* Move to next sequence of values. */
    usd_remaining_points = usd_remaining_points.drop_front(points_num);
    usd_remaining_knots = usd_remaining_knots.drop_front(knots_num);
  }

  offset_indices::accumulate_counts_to_offsets(data.blender_offsets);
  offset_indices::accumulate_counts_to_offsets(data.usd_offsets);
  offset_indices::accumulate_counts_to_offsets(data.usd_knot_offsets);
  return data;
}

/** Returns true if the number of curves or the number of curve points in each curve differ. */
static bool curves_topology_changed(const bke::CurvesGeometry &curves, const Span<int> usd_offsets)
{
  if (curves.offsets() != usd_offsets) {
    return true;
  }

  return false;
}

static IndexRange get_usd_points_range_de_dup(IndexRange blender_points_range,
                                              IndexRange usd_points_range)
{
  /* Take from the front of USD's range to exclude any duplicates at the end. */
  return usd_points_range.take_front(blender_points_range.size());
};

bool USDNurbsReader::is_animated() const
{
  if (curve_prim_.GetPointsAttr().ValueMightBeTimeVarying() ||
      curve_prim_.GetWidthsAttr().ValueMightBeTimeVarying() ||
      curve_prim_.GetPointWeightsAttr().ValueMightBeTimeVarying())
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

void USDNurbsReader::read_curve_sample(Curves *curves_id, const pxr::UsdTimeCode time)
{
  USDCurveData usd_data;
  if (!usd_data.load(curve_prim_, time)) {
    return;
  }

  const Span<float3> usd_points = usd_data.points();
  const Span<int> usd_counts = usd_data.counts();
  const Span<int> usd_orders = usd_data.orders();
  const Span<double> usd_knots = usd_data.knots();
  const Span<double> usd_weights = usd_data.weights();
  const Span<float3> usd_velocities = usd_data.velocities();

  /* Calculate and set the Curves topology. */
  CurveData data = calc_curve_offsets(usd_points, usd_counts, usd_orders, usd_knots);

  bke::CurvesGeometry &curves = curves_id->geometry.wrap();
  if (curves_topology_changed(curves, data.blender_offsets)) {
    curves.resize(data.blender_offsets.last(), usd_counts.size());
    curves.offsets_for_write().copy_from(data.blender_offsets);
    curves.fill_curve_types(CurveType::CURVE_TYPE_NURBS);
  }

  /* NOTE: USD contains duplicated points for periodic(cyclic) curves. The indices into each curve
   * will differ from what Blender expects so we need to maintain and use separate offsets for
   * each. A side effect of this dissonance is that all primvar/attribute loading needs to be
   * handled in a special manner vs. what might be seen in our other USD readers. */
  const OffsetIndices blender_points_by_curve = curves.points_by_curve();
  const OffsetIndices usd_points_by_curve = OffsetIndices<int>(data.usd_offsets,
                                                               offset_indices::NoSortCheck{});
  const OffsetIndices usd_knots_by_curve = OffsetIndices<int>(data.usd_knot_offsets,
                                                              offset_indices::NoSortCheck{});

  /* TODO: We cannot read custom primvars for cyclic curves at the moment. */
  const bool can_read_primvars = std::all_of(
      data.is_cyclic.begin(), data.is_cyclic.end(), [](bool item) { return item == false; });

  /* Set all curve data. */
  MutableSpan<float3> curves_positions = curves.positions_for_write();
  for (const int curve_i : blender_points_by_curve.index_range()) {
    const IndexRange blender_points_range = blender_points_by_curve[curve_i];
    const IndexRange usd_points_range_de_dup = get_usd_points_range_de_dup(
        blender_points_range, usd_points_by_curve[curve_i]);

    curves_positions.slice(blender_points_range)
        .copy_from(usd_points.slice(usd_points_range_de_dup));
  }

  MutableSpan<bool> curves_cyclic = curves.cyclic_for_write();
  curves_cyclic.copy_from(data.is_cyclic);

  MutableSpan<int8_t> curves_nurbs_orders = curves.nurbs_orders_for_write();
  for (const int curve_i : blender_points_by_curve.index_range()) {
    curves_nurbs_orders[curve_i] = int8_t(usd_orders[curve_i]);
  }

  MutableSpan<int8_t> curves_knots_mode = curves.nurbs_knots_modes_for_write();
  for (const int curve_i : blender_points_by_curve.index_range()) {
    const IndexRange usd_knots_range = usd_knots_by_curve[curve_i];
    curves_knots_mode[curve_i] = determine_knots_mode(
        usd_knots.slice(usd_knots_range), usd_orders[curve_i], data.is_cyclic[curve_i]);
  }

  /* Load in the optional weights. */
  if (!usd_weights.is_empty()) {
    MutableSpan<float> curves_weights = curves.nurbs_weights_for_write();
    for (const int curve_i : blender_points_by_curve.index_range()) {
      const IndexRange blender_points_range = blender_points_by_curve[curve_i];
      const IndexRange usd_points_range_de_dup = get_usd_points_range_de_dup(
          blender_points_range, usd_points_by_curve[curve_i]);

      const Span<double> usd_weights_de_dup = usd_weights.slice(usd_points_range_de_dup);
      int64_t usd_point_i = 0;
      for (const int point_i : blender_points_range) {
        curves_weights[point_i] = float(usd_weights_de_dup[usd_point_i]);
        usd_point_i++;
      }
    }
  }

  /* Load in the optional velocities. */
  if (!usd_velocities.is_empty()) {
    bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
    bke::SpanAttributeWriter<float3> curves_velocity =
        attributes.lookup_or_add_for_write_only_span<float3>("velocity", bke::AttrDomain::Point);

    for (const int curve_i : blender_points_by_curve.index_range()) {
      const IndexRange blender_points_range = blender_points_by_curve[curve_i];
      const IndexRange usd_points_range_de_dup = get_usd_points_range_de_dup(
          blender_points_range, usd_points_by_curve[curve_i]);

      curves_velocity.span.slice(blender_points_range)
          .copy_from(usd_velocities.slice(usd_points_range_de_dup));
    }

    curves_velocity.finish();
  }

  /* Once all of the curves metadata (orders, cyclic, knots_mode) has been set, we can prepare
   * Blender for any custom knots that need to be loaded. */
  MutableSpan<float> blender_custom_knots;
  OffsetIndices<int> blender_knots_by_curve;
  for (const int curve_i : curves.curves_range()) {
    if (curves_knots_mode[curve_i] != NURBS_KNOT_MODE_CUSTOM) {
      continue;
    }

    /* If this is our first time through, we need to update Blender's topology data to prepare for
     * the incoming custom knots. */
    if (blender_custom_knots.is_empty()) {
      curves.nurbs_custom_knots_update_size();
      blender_knots_by_curve = curves.nurbs_custom_knots_by_curve();
      blender_custom_knots = curves.nurbs_custom_knots_for_write();
    }

    const IndexRange blender_knots_range = blender_knots_by_curve[curve_i];
    const IndexRange usd_knots_range = usd_knots_by_curve[curve_i];
    const Span<double> usd_knots_values = usd_knots.slice(usd_knots_range);
    MutableSpan<float> blender_knots = blender_custom_knots.slice(blender_knots_range);
    int usd_knot_i = 0;
    for (float &blender_knot : blender_knots) {
      blender_knot = usd_knots_values[usd_knot_i] > 0.0 ? float(usd_knots_values[usd_knot_i]) : 0;
      usd_knot_i++;
    }
  }

  /* Curve widths. */
  const Span<float> usd_widths = usd_data.widths();
  if (!usd_widths.is_empty()) {
    MutableSpan<float> radii = curves.radius_for_write();

    const pxr::TfToken widths_interp = curve_prim_.GetWidthsInterpolation();
    if (widths_interp == pxr::UsdGeomTokens->constant || usd_widths.size() == 1) {
      radii.fill(usd_widths[0] / 2.0f);
    }
    else if (widths_interp == pxr::UsdGeomTokens->varying) {
      int point_offset = 0;
      for (const int curve_i : curves.curves_range()) {
        const float usd_curve_radius = usd_widths[curve_i] / 2.0f;
        int point_count = usd_counts[curve_i];
        if (curves_cyclic[curve_i]) {
          point_count -= usd_orders[curve_i] - 1;
        }
        for (const int point : IndexRange(point_count)) {
          radii[point_offset + point] = usd_curve_radius;
        }

        point_offset += point_count;
      }
    }
    else if (widths_interp == pxr::UsdGeomTokens->vertex) {
      for (const int curve_i : curves.curves_range()) {
        const IndexRange blender_points_range = blender_points_by_curve[curve_i];
        const IndexRange usd_points_range = usd_points_by_curve[curve_i];

        /* Take from the front of USD's range to exclude any duplicates at the end. */
        const IndexRange usd_points_range_de_dup = usd_points_range.take_front(
            blender_points_range.size());

        const Span<float> usd_widths_de_dup = usd_widths.slice(usd_points_range_de_dup);
        int64_t usd_point_i = 0;
        for (const int point_i : blender_points_range) {
          radii[point_i] = usd_widths_de_dup[usd_point_i] / 2.0f;
          usd_point_i++;
        }
      }
    }
  }

  if (can_read_primvars) {
    this->read_custom_data(curves, time);
  }
}

}  // namespace blender::io::usd
