/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"
#include "BKE_curves_utils.hh"

#include "BLI_math_rotation_legacy.hh"
#include "BLI_task.hh"

#include "GEO_fillet_curves.hh"

namespace blender::geometry {

static void duplicate_fillet_point_data(const OffsetIndices<int> src_points_by_curve,
                                        const OffsetIndices<int> dst_points_by_curve,
                                        const IndexMask &curve_selection,
                                        const Span<int> all_point_offsets,
                                        const GSpan src,
                                        GMutableSpan dst)
{
  curve_selection.foreach_index(GrainSize(512), [&](const int curve_i) {
    const IndexRange src_points = src_points_by_curve[curve_i];
    const IndexRange dst_points = dst_points_by_curve[curve_i];
    const IndexRange offsets_range = bke::curves::per_curve_point_offsets_range(src_points,
                                                                                curve_i);
    bke::attribute_math::gather_to_groups(all_point_offsets.slice(offsets_range),
                                          IndexRange(src_points.size()),
                                          src.slice(src_points),
                                          dst.slice(dst_points));
  });
}

static void calculate_result_offsets(const OffsetIndices<int> src_points_by_curve,
                                     const IndexMask &selection,
                                     const IndexMask &unselected,
                                     const VArray<float> &radii,
                                     const VArray<int> &counts,
                                     const Span<bool> cyclic,
                                     MutableSpan<int> dst_curve_offsets,
                                     MutableSpan<int> dst_point_offsets)
{
  /* Fill the offsets array with the curve point counts, then accumulate them to form offsets. */
  offset_indices::copy_group_sizes(src_points_by_curve, unselected, dst_curve_offsets);
  selection.foreach_index(GrainSize(512), [&](const int curve_i) {
    const IndexRange src_points = src_points_by_curve[curve_i];
    const IndexRange offsets_range = bke::curves::per_curve_point_offsets_range(src_points,
                                                                                curve_i);

    MutableSpan<int> point_offsets = dst_point_offsets.slice(offsets_range);
    MutableSpan<int> point_counts = point_offsets.drop_back(1);

    counts.materialize_compressed(src_points, point_counts);
    for (int &count : point_counts) {
      /* Make sure the number of cuts is greater than zero and add one for the existing point. */
      count = std::max(count, 0) + 1;
    }
    if (!cyclic[curve_i]) {
      /* Endpoints on non-cyclic curves cannot be filleted. */
      point_counts.first() = 1;
      point_counts.last() = 1;
    }
    /* Implicitly "deselect" points with zero radius. */
    devirtualize_varray(radii, [&](const auto radii) {
      for (const int i : IndexRange(src_points.size())) {
        if (radii[src_points[i]] == 0.0f) {
          point_counts[i] = 1;
        }
      }
    });

    offset_indices::accumulate_counts_to_offsets(point_offsets);

    dst_curve_offsets[curve_i] = point_offsets.last();
  });
  offset_indices::accumulate_counts_to_offsets(dst_curve_offsets);
}

static void calculate_directions(const Span<float3> positions, MutableSpan<float3> directions)
{
  for (const int i : positions.index_range().drop_back(1)) {
    directions[i] = math::normalize(positions[i + 1] - positions[i]);
  }
  directions.last() = math::normalize(positions.first() - positions.last());
}

static void calculate_angles(const Span<float3> directions, MutableSpan<float> angles)
{
  angles.first() = M_PI - angle_v3v3(-directions.last(), directions.first());
  for (const int i : directions.index_range().drop_front(1)) {
    angles[i] = M_PI - angle_v3v3(-directions[i - 1], directions[i]);
  }
}

/**
 * Find the portion of the previous and next segments used by the current and next point fillets.
 * If more than the total length of the segment would be used, scale the current point's radius
 * just enough to make the two points meet in the middle.
 */
static float limit_radius(const float3 &position_prev,
                          const float3 &position,
                          const float3 &position_next,
                          const float angle_prev,
                          const float angle,
                          const float angle_next,
                          const float radius_prev,
                          const float radius,
                          const float radius_next)
{
  const float displacement = radius * std::tan(angle / 2.0f);

  const float displacement_prev = radius_prev * std::tan(angle_prev / 2.0f);
  const float segment_length_prev = math::distance(position, position_prev);
  const float total_displacement_prev = displacement_prev + displacement;
  const float factor_prev = std::clamp(
      math::safe_divide(segment_length_prev, total_displacement_prev), 0.0f, 1.0f);

  const float displacement_next = radius_next * std::tan(angle_next / 2.0f);
  const float segment_length_next = math::distance(position, position_next);
  const float total_displacement_next = displacement_next + displacement;
  const float factor_next = std::clamp(
      math::safe_divide(segment_length_next, total_displacement_next), 0.0f, 1.0f);

  return radius * std::min(factor_prev, factor_next);
}

static void limit_radii(const Span<float3> positions,
                        const Span<float> angles,
                        const Span<float> radii,
                        const bool cyclic,
                        MutableSpan<float> radii_clamped)
{
  if (cyclic) {
    /* First point. */
    radii_clamped.first() = limit_radius(positions.last(),
                                         positions.first(),
                                         positions[1],
                                         angles.last(),
                                         angles.first(),
                                         angles[1],
                                         radii.last(),
                                         radii.first(),
                                         radii[1]);
    /* All middle points. */
    for (const int i : positions.index_range().drop_back(1).drop_front(1)) {
      const int i_prev = i - 1;
      const int i_next = i + 1;
      radii_clamped[i] = limit_radius(positions[i_prev],
                                      positions[i],
                                      positions[i_next],
                                      angles[i_prev],
                                      angles[i],
                                      angles[i_next],
                                      radii[i_prev],
                                      radii[i],
                                      radii[i_next]);
    }
    /* Last point. */
    radii_clamped.last() = limit_radius(positions.last(1),
                                        positions.last(),
                                        positions.first(),
                                        angles.last(1),
                                        angles.last(),
                                        angles.first(),
                                        radii.last(1),
                                        radii.last(),
                                        radii.first());
  }
  else {
    const int i_last = positions.index_range().last();
    /* First point. */
    radii_clamped.first() = 0.0f;
    /* All middle points. */
    for (const int i : positions.index_range().drop_back(1).drop_front(1)) {
      const int i_prev = i - 1;
      const int i_next = i + 1;
      /* Use a zero radius for the first and last points, because they don't have fillets.
       * This logic could potentially be unrolled, but it doesn't seem worth it. */
      const float radius_prev = i_prev == 0 ? 0.0f : radii[i_prev];
      const float radius_next = i_next == i_last ? 0.0f : radii[i_next];
      radii_clamped[i] = limit_radius(positions[i_prev],
                                      positions[i],
                                      positions[i_next],
                                      angles[i_prev],
                                      angles[i],
                                      angles[i_next],
                                      radius_prev,
                                      radii[i],
                                      radius_next);
    }
    /* Last point. */
    radii_clamped.last() = 0.0f;
  }
}

static void calculate_fillet_positions(const Span<float3> src_positions,
                                       const Span<float> angles,
                                       const Span<float> radii,
                                       const Span<float3> directions,
                                       const OffsetIndices<int> dst_offsets,
                                       MutableSpan<float3> dst)
{
  const int i_src_last = src_positions.index_range().last();
  threading::parallel_for(src_positions.index_range(), 512, [&](IndexRange range) {
    for (const int i_src : range) {
      const IndexRange arc = dst_offsets[i_src];
      const float3 &src = src_positions[i_src];
      if (arc.size() == 1) {
        dst[arc.first()] = src;
        continue;
      }

      const int i_src_prev = i_src == 0 ? i_src_last : i_src - 1;
      const float angle = angles[i_src];
      const float radius = radii[i_src];
      const float displacement = radius * std::tan(angle / 2.0f);
      const float3 prev_dir = -directions[i_src_prev];
      const float3 &next_dir = directions[i_src];
      const float3 arc_start = src + prev_dir * displacement;
      const float3 arc_end = src + next_dir * displacement;

      dst[arc.first()] = arc_start;
      dst[arc.last()] = arc_end;

      const IndexRange middle = arc.drop_front(1).drop_back(1);
      if (middle.is_empty()) {
        continue;
      }

      const float3 axis = -math::normalize(math::cross(prev_dir, next_dir));
      const float3 center_direction = math::normalize(math::midpoint(next_dir, prev_dir));
      const float distance_to_center = std::sqrt(pow2f(radius) + pow2f(displacement));
      const float3 center = src + center_direction * distance_to_center;

      /* Rotate each middle fillet point around the center. */
      const float segment_angle = angle / (middle.size() + 1);
      for (const int i : IndexRange(middle.size())) {
        const int point_i = middle[i];
        dst[point_i] = math::rotate_around_axis(arc_start, center, axis, segment_angle * (i + 1));
      }
    }
  });
}

/**
 * Set handles for the "Bezier" mode where we rely on setting the inner handles to approximate a
 * circular arc. The outer (previous and next) handles outside the result fillet segment are set
 * to vector handles.
 */
static void calculate_bezier_handles_bezier_mode(const Span<float3> src_handles_l,
                                                 const Span<float3> src_handles_r,
                                                 const Span<int8_t> src_types_l,
                                                 const Span<int8_t> src_types_r,
                                                 const Span<float> angles,
                                                 const Span<float> radii,
                                                 const Span<float3> directions,
                                                 const OffsetIndices<int> dst_offsets,
                                                 const Span<float3> dst_positions,
                                                 MutableSpan<float3> dst_handles_l,
                                                 MutableSpan<float3> dst_handles_r,
                                                 MutableSpan<int8_t> dst_types_l,
                                                 MutableSpan<int8_t> dst_types_r)
{
  const int i_src_last = src_handles_l.index_range().last();
  const int i_dst_last = dst_positions.index_range().last();
  threading::parallel_for(src_handles_l.index_range(), 512, [&](IndexRange range) {
    for (const int i_src : range) {
      const IndexRange arc = dst_offsets[i_src];
      if (arc.size() == 1) {
        dst_handles_l[arc.first()] = src_handles_l[i_src];
        dst_handles_r[arc.first()] = src_handles_r[i_src];
        dst_types_l[arc.first()] = src_types_l[i_src];
        dst_types_r[arc.first()] = src_types_r[i_src];
        continue;
      }
      BLI_assert(arc.size() == 2);
      const int i_dst_a = arc.first();
      const int i_dst_b = arc.last();

      const int i_src_prev = i_src == 0 ? i_src_last : i_src - 1;
      const float angle = angles[i_src];
      const float radius = radii[i_src];
      const float3 prev_dir = -directions[i_src_prev];
      const float3 &next_dir = directions[i_src];

      const float3 &arc_start = dst_positions[arc.first()];
      const float3 &arc_end = dst_positions[arc.last()];

      /* Calculate the point's handles on the outside of the fillet segment,
       * connecting to the next or previous result points. */
      const int i_dst_prev = i_dst_a == 0 ? i_dst_last : i_dst_a - 1;
      const int i_dst_next = i_dst_b == i_dst_last ? 0 : i_dst_b + 1;
      dst_handles_l[i_dst_a] = bke::curves::bezier::calculate_vector_handle(
          dst_positions[i_dst_a], dst_positions[i_dst_prev]);
      dst_handles_r[i_dst_b] = bke::curves::bezier::calculate_vector_handle(
          dst_positions[i_dst_b], dst_positions[i_dst_next]);
      dst_types_l[i_dst_a] = BEZIER_HANDLE_VECTOR;
      dst_types_r[i_dst_b] = BEZIER_HANDLE_VECTOR;

      /* The inner handles are aligned with the aligned with the outer vector
       * handles, but have a specific length to best approximate a circle. */
      const float handle_length = (4.0f / 3.0f) * radius * std::tan(angle / 4.0f);
      dst_handles_r[i_dst_a] = arc_start - prev_dir * handle_length;
      dst_handles_l[i_dst_b] = arc_end - next_dir * handle_length;
      dst_types_r[i_dst_a] = BEZIER_HANDLE_ALIGN;
      dst_types_l[i_dst_b] = BEZIER_HANDLE_ALIGN;
    }
  });
}

/**
 * In the poly fillet mode, all the inner handles are set to vector handles, along with the "outer"
 * (previous and next) handles at each fillet.
 */
static void calculate_bezier_handles_poly_mode(const Span<float3> src_handles_l,
                                               const Span<float3> src_handles_r,
                                               const Span<int8_t> src_types_l,
                                               const Span<int8_t> src_types_r,
                                               const OffsetIndices<int> dst_offsets,
                                               const Span<float3> dst_positions,
                                               MutableSpan<float3> dst_handles_l,
                                               MutableSpan<float3> dst_handles_r,
                                               MutableSpan<int8_t> dst_types_l,
                                               MutableSpan<int8_t> dst_types_r)
{
  const int i_dst_last = dst_positions.index_range().last();
  threading::parallel_for(src_handles_l.index_range(), 512, [&](IndexRange range) {
    for (const int i_src : range) {
      const IndexRange arc = dst_offsets[i_src];
      if (arc.size() == 1) {
        dst_handles_l[arc.first()] = src_handles_l[i_src];
        dst_handles_r[arc.first()] = src_handles_r[i_src];
        dst_types_l[arc.first()] = src_types_l[i_src];
        dst_types_r[arc.first()] = src_types_r[i_src];
        continue;
      }

      /* The fillet's next and previous handles are vector handles, as are the inner handles. */
      dst_types_l.slice(arc).fill(BEZIER_HANDLE_VECTOR);
      dst_types_r.slice(arc).fill(BEZIER_HANDLE_VECTOR);

      /* Calculate the point's handles on the outside of the fillet segment. This point
       * won't be selected for a fillet if it is the first or last in a non-cyclic curve. */

      const int i_dst_prev = arc.first() == 0 ? i_dst_last : arc.one_before_start();
      const int i_dst_next = arc.last() == i_dst_last ? 0 : arc.one_after_last();
      dst_handles_l[arc.first()] = bke::curves::bezier::calculate_vector_handle(
          dst_positions[arc.first()], dst_positions[i_dst_prev]);
      dst_handles_r[arc.last()] = bke::curves::bezier::calculate_vector_handle(
          dst_positions[arc.last()], dst_positions[i_dst_next]);

      /* Set the values for the inner handles. */
      const IndexRange middle = arc.drop_front(1).drop_back(1);
      for (const int i : middle) {
        dst_handles_r[i] = bke::curves::bezier::calculate_vector_handle(dst_positions[i],
                                                                        dst_positions[i - 1]);
        dst_handles_l[i] = bke::curves::bezier::calculate_vector_handle(dst_positions[i],
                                                                        dst_positions[i + 1]);
      }
    }
  });
}

static bke::CurvesGeometry fillet_curves(const bke::CurvesGeometry &src_curves,
                                         const IndexMask &curve_selection,
                                         const VArray<float> &radius_input,
                                         const VArray<int> &counts,
                                         const bool limit_radius,
                                         const bool use_bezier_mode,
                                         const bke::AttributeFilter &attribute_filter)
{
  if (src_curves.is_empty()) {
    return src_curves;
  }
  const OffsetIndices src_points_by_curve = src_curves.points_by_curve();
  const Span<float3> positions = src_curves.positions();
  const VArraySpan<bool> cyclic{src_curves.cyclic()};
  const bke::AttributeAccessor src_attributes = src_curves.attributes();
  IndexMaskMemory memory;
  const IndexMask unselected = curve_selection.complement(src_curves.curves_range(), memory);

  bke::CurvesGeometry dst_curves = bke::curves::copy_only_curve_domain(src_curves);
  /* Stores the offset of every result point for every original point.
   * The extra length is used in order to store an extra zero for every curve. */
  Array<int> dst_point_offsets(src_curves.points_num() + src_curves.curves_num());
  calculate_result_offsets(src_points_by_curve,
                           curve_selection,
                           unselected,
                           radius_input,
                           counts,
                           cyclic,
                           dst_curves.offsets_for_write(),
                           dst_point_offsets);
  const OffsetIndices dst_points_by_curve = dst_curves.points_by_curve();
  const Span<int> all_point_offsets = dst_point_offsets.as_span();

  dst_curves.resize(dst_curves.offsets().last(), dst_curves.curves_num());
  bke::MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();
  MutableSpan<float3> dst_positions = dst_curves.positions_for_write();

  VArraySpan<int8_t> src_types_l;
  VArraySpan<int8_t> src_types_r;
  Span<float3> src_handles_l;
  Span<float3> src_handles_r;
  MutableSpan<int8_t> dst_types_l;
  MutableSpan<int8_t> dst_types_r;
  MutableSpan<float3> dst_handles_l;
  MutableSpan<float3> dst_handles_r;
  if (src_curves.has_curve_with_type(CURVE_TYPE_BEZIER)) {
    src_types_l = src_curves.handle_types_left();
    src_types_r = src_curves.handle_types_right();
    src_handles_l = *src_curves.handle_positions_left();
    src_handles_r = *src_curves.handle_positions_right();

    dst_types_l = dst_curves.handle_types_left_for_write();
    dst_types_r = dst_curves.handle_types_right_for_write();
    dst_handles_l = dst_curves.handle_positions_left_for_write();
    dst_handles_r = dst_curves.handle_positions_right_for_write();
  }

  curve_selection.foreach_segment(GrainSize(512), [&](const IndexMaskSegment segment) {
    Array<float3> directions;
    Array<float> angles;
    Array<float> radii;
    Array<float> input_radii_buffer;

    for (const int curve_i : segment) {
      const IndexRange src_points = src_points_by_curve[curve_i];
      const IndexRange offsets_range = bke::curves::per_curve_point_offsets_range(src_points,
                                                                                  curve_i);
      const OffsetIndices<int> offsets(all_point_offsets.slice(offsets_range));
      const IndexRange dst_points = dst_points_by_curve[curve_i];
      const Span<float3> src_positions = positions.slice(src_points);

      directions.reinitialize(src_points.size());
      calculate_directions(src_positions, directions);

      angles.reinitialize(src_points.size());
      calculate_angles(directions, angles);

      radii.reinitialize(src_points.size());
      if (limit_radius) {
        input_radii_buffer.reinitialize(src_points.size());
        radius_input.materialize_compressed(src_points, input_radii_buffer);
        limit_radii(src_positions, angles, input_radii_buffer, cyclic[curve_i], radii);
      }
      else {
        radius_input.materialize_compressed(src_points, radii);
      }

      calculate_fillet_positions(positions.slice(src_points),
                                 angles,
                                 radii,
                                 directions,
                                 offsets,
                                 dst_positions.slice(dst_points));

      if (src_curves.has_curve_with_type(CURVE_TYPE_BEZIER)) {
        if (use_bezier_mode) {
          calculate_bezier_handles_bezier_mode(src_handles_l.slice(src_points),
                                               src_handles_r.slice(src_points),
                                               src_types_l.slice(src_points),
                                               src_types_r.slice(src_points),
                                               angles,
                                               radii,
                                               directions,
                                               offsets,
                                               dst_positions.slice(dst_points),
                                               dst_handles_l.slice(dst_points),
                                               dst_handles_r.slice(dst_points),
                                               dst_types_l.slice(dst_points),
                                               dst_types_r.slice(dst_points));
        }
        else {
          calculate_bezier_handles_poly_mode(src_handles_l.slice(src_points),
                                             src_handles_r.slice(src_points),
                                             src_types_l.slice(src_points),
                                             src_types_r.slice(src_points),
                                             offsets,
                                             dst_positions.slice(dst_points),
                                             dst_handles_l.slice(dst_points),
                                             dst_handles_r.slice(dst_points),
                                             dst_types_l.slice(dst_points),
                                             dst_types_r.slice(dst_points));
        }
      }
    }
  });

  for (auto &attribute : bke::retrieve_attributes_for_transfer(
           src_attributes,
           dst_attributes,
           {bke::AttrDomain::Point},
           bke::attribute_filter_with_skip_ref(attribute_filter,
                                               {"position",
                                                "handle_type_left",
                                                "handle_type_right",
                                                "handle_right",
                                                "handle_left"})))
  {
    duplicate_fillet_point_data(src_points_by_curve,
                                dst_points_by_curve,
                                curve_selection,
                                all_point_offsets,
                                attribute.src,
                                attribute.dst.span);
    attribute.dst.finish();
  }

  bke::copy_attributes_group_to_group(src_attributes,
                                      bke::AttrDomain::Point,
                                      bke::AttrDomain::Point,
                                      attribute_filter,
                                      src_points_by_curve,
                                      dst_points_by_curve,
                                      unselected,
                                      dst_attributes);
  if (src_curves.nurbs_has_custom_knots()) {
    bke::curves::nurbs::update_custom_knot_modes(
        dst_curves.curves_range(), NURBS_KNOT_MODE_NORMAL, NURBS_KNOT_MODE_NORMAL, dst_curves);
  }
  return dst_curves;
}

bke::CurvesGeometry fillet_curves_poly(const bke::CurvesGeometry &src_curves,
                                       const IndexMask &curve_selection,
                                       const VArray<float> &radius,
                                       const VArray<int> &count,
                                       const bool limit_radius,
                                       const bke::AttributeFilter &attribute_filter)
{
  return fillet_curves(
      src_curves, curve_selection, radius, count, limit_radius, false, attribute_filter);
}

bke::CurvesGeometry fillet_curves_bezier(const bke::CurvesGeometry &src_curves,
                                         const IndexMask &curve_selection,
                                         const VArray<float> &radius,
                                         const bool limit_radius,
                                         const bke::AttributeFilter &attribute_filter)
{
  return fillet_curves(src_curves,
                       curve_selection,
                       radius,
                       VArray<int>::from_single(1, src_curves.points_num()),
                       limit_radius,
                       true,
                       attribute_filter);
}

}  // namespace blender::geometry
