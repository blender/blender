/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BLI_length_parameterize.hh"

#include "BKE_attribute.hh"
#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"
#include "BKE_curves_utils.hh"

#include "GEO_trim_curves.hh"

namespace blender::geometry {

/* -------------------------------------------------------------------- */
/** \name Lookup Curve Points
 * \{ */

/**
 * Find the point on the curve defined by the distance along the curve. Assumes curve resolution is
 * constant for all curve segments and evaluated curve points are uniformly spaced between the
 * segment endpoints in relation to the curve parameter.
 *
 * \param lengths: Accumulated length for the evaluated curve.
 * \param sample_length: Distance along the curve to determine the #CurvePoint for.
 * \param cyclic: If curve is cyclic.
 * \param resolution: Curve resolution (number of evaluated points per segment).
 * \param num_curve_points: Total number of control points in the curve.
 * \return Point on the piecewise segment matching the given distance.
 */
static bke::curves::CurvePoint lookup_point_uniform_spacing(const Span<float> lengths,
                                                            const float sample_length,
                                                            const bool cyclic,
                                                            const int resolution,
                                                            const int num_curve_points)
{
  BLI_assert(!cyclic || lengths.size() / resolution >= 2);
  const int last_index = num_curve_points - 1;
  if (sample_length <= 0.0f) {
    return {{0, 1}, 0.0f};
  }
  if (sample_length >= lengths.last()) {
    return cyclic ? bke::curves::CurvePoint{{last_index, 0}, 1.0} :
                    bke::curves::CurvePoint{{last_index - 1, last_index}, 1.0};
  }
  int eval_index;
  float eval_factor;
  length_parameterize::sample_at_length(lengths, sample_length, eval_index, eval_factor);

  const int index = eval_index / resolution;
  const int next_index = (index == last_index) ? 0 : index + 1;
  const float parameter = (eval_factor + eval_index) / resolution - index;

  return bke::curves::CurvePoint{{index, next_index}, parameter};
}

/**
 * Find the point on the 'evaluated' polygonal curve.
 */
static bke::curves::CurvePoint lookup_point_polygonal(const Span<float> lengths,
                                                      const float sample_length,
                                                      const bool cyclic,
                                                      const int evaluated_size)
{
  const int last_index = evaluated_size - 1;
  if (sample_length <= 0.0f) {
    return {{0, 1}, 0.0f};
  }
  if (sample_length >= lengths.last()) {
    return cyclic ? bke::curves::CurvePoint{{last_index, 0}, 1.0} :
                    bke::curves::CurvePoint{{last_index - 1, last_index}, 1.0};
  }

  int eval_index;
  float eval_factor;
  length_parameterize::sample_at_length(lengths, sample_length, eval_index, eval_factor);

  const int next_eval_index = (eval_index == last_index) ? 0 : eval_index + 1;
  return bke::curves::CurvePoint{{eval_index, next_eval_index}, eval_factor};
}

/**
 * Find the point on a Bezier curve using the 'bezier_offsets' cache.
 */
static bke::curves::CurvePoint lookup_point_bezier(const Span<int> bezier_offsets,
                                                   const Span<float> lengths,
                                                   const float sample_length,
                                                   const bool cyclic,
                                                   const int num_curve_points)
{
  const int last_index = num_curve_points - 1;
  if (sample_length <= 0.0f) {
    return {{0, 1}, 0.0f};
  }
  if (sample_length >= lengths.last()) {
    return cyclic ? bke::curves::CurvePoint{{last_index, 0}, 1.0} :
                    bke::curves::CurvePoint{{last_index - 1, last_index}, 1.0};
  }
  int eval_index;
  float eval_factor;
  length_parameterize::sample_at_length(lengths, sample_length, eval_index, eval_factor);

  /* Find the segment index from the offset mapping. */
  const int *offset = std::upper_bound(bezier_offsets.begin(), bezier_offsets.end(), eval_index);
  const int left = offset - bezier_offsets.begin() - 1;
  const int right = left == last_index ? 0 : left + 1;

  const int prev_offset = bezier_offsets[left];
  const float offset_in_segment = eval_factor + (eval_index - prev_offset);
  const int segment_resolution = bezier_offsets[left + 1] - prev_offset;
  const float parameter = std::clamp(offset_in_segment / segment_resolution, 0.0f, 1.0f);

  return {{left, right}, parameter};
}

static bke::curves::CurvePoint lookup_point_bezier(
    const bke::CurvesGeometry &src_curves,
    const OffsetIndices<int> evaluated_points_by_curve,
    const int64_t curve_index,
    const Span<float> accumulated_lengths,
    const float sample_length,
    const bool cyclic,
    const int resolution,
    const int num_curve_points)
{
  if (bke::curves::bezier::has_vector_handles(
          num_curve_points, evaluated_points_by_curve[curve_index].size(), cyclic, resolution))
  {
    const Span<int> bezier_offsets = src_curves.bezier_evaluated_offsets_for_curve(curve_index);
    return lookup_point_bezier(
        bezier_offsets, accumulated_lengths, sample_length, cyclic, num_curve_points);
  }
  return lookup_point_uniform_spacing(
      accumulated_lengths, sample_length, cyclic, resolution, num_curve_points);
}

static bke::curves::CurvePoint lookup_curve_point(
    const bke::CurvesGeometry &src_curves,
    const OffsetIndices<int> evaluated_points_by_curve,
    const CurveType curve_type,
    const int64_t curve_index,
    const Span<float> accumulated_lengths,
    const float sample_length,
    const bool cyclic,
    const int resolution,
    const int num_curve_points)
{
  if (num_curve_points == 1) {
    return {{0, 0}, 0.0f};
  }

  if (curve_type == CURVE_TYPE_CATMULL_ROM) {
    return lookup_point_uniform_spacing(
        accumulated_lengths, sample_length, cyclic, resolution, num_curve_points);
  }
  if (curve_type == CURVE_TYPE_BEZIER) {
    return lookup_point_bezier(src_curves,
                               evaluated_points_by_curve,
                               curve_index,
                               accumulated_lengths,
                               sample_length,
                               cyclic,
                               resolution,
                               num_curve_points);
  }
  if (curve_type == CURVE_TYPE_POLY) {
    return lookup_point_polygonal(accumulated_lengths, sample_length, cyclic, num_curve_points);
  }
  /* Handle evaluated curve. */
  BLI_assert(resolution > 0);
  return lookup_point_polygonal(
      accumulated_lengths, sample_length, cyclic, evaluated_points_by_curve[curve_index].size());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utility Functions
 * \{ */

static void fill_bezier_data(bke::CurvesGeometry &dst_curves, const IndexMask &selection)
{
  if (!dst_curves.has_curve_with_type(CURVE_TYPE_BEZIER)) {
    return;
  }
  const OffsetIndices dst_points_by_curve = dst_curves.points_by_curve();
  MutableSpan<float3> handle_positions_left = dst_curves.handle_positions_left_for_write();
  MutableSpan<float3> handle_positions_right = dst_curves.handle_positions_right_for_write();
  MutableSpan<int8_t> handle_types_left = dst_curves.handle_types_left_for_write();
  MutableSpan<int8_t> handle_types_right = dst_curves.handle_types_right_for_write();

  selection.foreach_index(GrainSize(4096), [&](const int curve_i) {
    const IndexRange points = dst_points_by_curve[curve_i];
    handle_types_right.slice(points).fill(int8_t(BEZIER_HANDLE_FREE));
    handle_types_left.slice(points).fill(int8_t(BEZIER_HANDLE_FREE));
    handle_positions_left.slice(points).fill({0.0f, 0.0f, 0.0f});
    handle_positions_right.slice(points).fill({0.0f, 0.0f, 0.0f});
  });
}
static void fill_nurbs_data(bke::CurvesGeometry &dst_curves, const IndexMask &selection)
{
  if (!dst_curves.has_curve_with_type(CURVE_TYPE_NURBS)) {
    return;
  }
  bke::curves::fill_points(
      dst_curves.points_by_curve(), selection, 0.0f, dst_curves.nurbs_weights_for_write());
}

template<typename T>
static int64_t copy_point_data_between_endpoints(const Span<T> src_data,
                                                 MutableSpan<T> dst_data,
                                                 const bke::curves::IndexRangeCyclic src_range,
                                                 int64_t dst_index)
{
  int64_t increment;
  if (src_range.cycles()) {
    increment = src_range.size_before_loop();
    dst_data.slice(dst_index, increment).copy_from(src_data.slice(src_range.first(), increment));
    dst_index += increment;

    increment = src_range.size_after_loop();
    dst_data.slice(dst_index, increment)
        .copy_from(src_data.slice(src_range.curve_range().first(), increment));
    dst_index += increment;
  }
  else {
    increment = src_range.one_after_last() - int64_t(src_range.first());
    dst_data.slice(dst_index, increment).copy_from(src_data.slice(src_range.first(), increment));
    dst_index += increment;
  }
  return dst_index;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sampling Utilities
 * \{ */

template<typename T>
static T interpolate_catmull_rom(const Span<T> src_data,
                                 const bke::curves::CurvePoint insertion_point,
                                 const bool src_cyclic)
{
  BLI_assert(insertion_point.index >= 0 && insertion_point.next_index < src_data.size());
  int i0;
  if (insertion_point.index == 0) {
    i0 = src_cyclic ? src_data.size() - 1 : insertion_point.index;
  }
  else {
    i0 = insertion_point.index - 1;
  }
  int i3 = insertion_point.next_index + 1;
  if (i3 == src_data.size()) {
    i3 = src_cyclic ? 0 : insertion_point.next_index;
  }
  return bke::curves::catmull_rom::interpolate<T>(src_data[i0],
                                                  src_data[insertion_point.index],
                                                  src_data[insertion_point.next_index],
                                                  src_data[i3],
                                                  insertion_point.parameter);
}

static bke::curves::bezier::Insertion knot_insert_bezier(
    const Span<float3> positions,
    const Span<float3> handles_left,
    const Span<float3> handles_right,
    const bke::curves::CurvePoint insertion_point)
{
  BLI_assert(
      insertion_point.index + 1 == insertion_point.next_index ||
      (insertion_point.next_index >= 0 && insertion_point.next_index < insertion_point.index));
  return bke::curves::bezier::insert(positions[insertion_point.index],
                                     handles_right[insertion_point.index],
                                     handles_left[insertion_point.next_index],
                                     positions[insertion_point.next_index],
                                     insertion_point.parameter);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sample Curve Interval (Trim)
 * \{ */

/**
 * Sample source curve data in the interval defined by the points [start_point, end_point].
 * Uses linear interpolation to compute the endpoints.
 *
 * \tparam include_start_point: If False, the 'start_point' point sample will not be copied
 * and not accounted for in the destination range.
 * \param src_data: Source to sample from.
 * \param dst_data: Destination to write samples to.
 * \param src_range: Interval within [start_point, end_point] to copy from the source point domain.
 * \param dst_range: Interval to copy point data to in the destination buffer.
 * \param start_point: Point on the source curve to start sampling from.
 * \param end_point: Last point to sample in the source curve.
 */
template<typename T, bool include_start_point = true>
static void sample_interval_linear(const Span<T> src_data,
                                   MutableSpan<T> dst_data,
                                   bke::curves::IndexRangeCyclic src_range,
                                   const IndexRange dst_range,
                                   const bke::curves::CurvePoint start_point,
                                   const bke::curves::CurvePoint end_point)
{
  int64_t dst_index = dst_range.first();

  if (start_point.is_controlpoint()) {
    /* 'start_point' is included in the copy iteration. */
    if constexpr (!include_start_point) {
      /* Skip first. */
      src_range = src_range.drop_front();
    }
  }
  else if constexpr (!include_start_point) {
    /* Do nothing (excluded). */
  }
  else {
    /* General case, sample 'start_point' */
    dst_data[dst_index] = bke::attribute_math::mix2(
        start_point.parameter, src_data[start_point.index], src_data[start_point.next_index]);
    ++dst_index;
  }

  dst_index = copy_point_data_between_endpoints(src_data, dst_data, src_range, dst_index);
  if (dst_range.size() == 1) {
    BLI_assert(dst_index == dst_range.one_after_last());
    return;
  }

  /* Handle last case */
  if (end_point.is_controlpoint()) {
    /* 'end_point' is included in the copy iteration. */
  }
  else {
    dst_data[dst_index] = bke::attribute_math::mix2(
        end_point.parameter, src_data[end_point.index], src_data[end_point.next_index]);
#ifndef NDEBUG
    ++dst_index;
#endif
  }
  BLI_assert(dst_index == dst_range.one_after_last());
}

template<typename T>
static void sample_interval_catmull_rom(const Span<T> src_data,
                                        MutableSpan<T> dst_data,
                                        bke::curves::IndexRangeCyclic src_range,
                                        const IndexRange dst_range,
                                        const bke::curves::CurvePoint start_point,
                                        const bke::curves::CurvePoint end_point,
                                        const bool src_cyclic)
{
  int64_t dst_index = dst_range.first();

  if (start_point.is_controlpoint()) {
  }
  else {
    /* General case, sample 'start_point' */
    dst_data[dst_index] = interpolate_catmull_rom(src_data, start_point, src_cyclic);
    ++dst_index;
  }

  dst_index = copy_point_data_between_endpoints(src_data, dst_data, src_range, dst_index);
  if (dst_range.size() == 1) {
    BLI_assert(dst_index == dst_range.one_after_last());
    return;
  }

  /* Handle last case */
  if (end_point.is_controlpoint()) {
    /* 'end_point' is included in the copy iteration. */
  }
  else {
    dst_data[dst_index] = interpolate_catmull_rom(src_data, end_point, src_cyclic);
#ifndef NDEBUG
    ++dst_index;
#endif
  }
  BLI_assert(dst_index == dst_range.one_after_last());
}

template<bool include_start_point = true>
static void sample_interval_bezier(const Span<float3> src_positions,
                                   const Span<float3> src_handles_l,
                                   const Span<float3> src_handles_r,
                                   const Span<int8_t> src_types_l,
                                   const Span<int8_t> src_types_r,
                                   MutableSpan<float3> dst_positions,
                                   MutableSpan<float3> dst_handles_l,
                                   MutableSpan<float3> dst_handles_r,
                                   MutableSpan<int8_t> dst_types_l,
                                   MutableSpan<int8_t> dst_types_r,
                                   bke::curves::IndexRangeCyclic src_range,
                                   const IndexRange dst_range,
                                   const bke::curves::CurvePoint start_point,
                                   const bke::curves::CurvePoint end_point)
{
  bke::curves::bezier::Insertion start_point_insert;
  int64_t dst_index = dst_range.first();

  bool start_point_trimmed = false;
  if (start_point.is_controlpoint()) {
    /* The 'start_point' control point is included in the copy iteration. */
    if constexpr (!include_start_point) {
      src_range = src_range.drop_front();
    }
  }
  else if constexpr (!include_start_point) {
    /* Do nothing, 'start_point' is excluded. */
  }
  else {
    /* General case, sample 'start_point'. */
    start_point_insert = knot_insert_bezier(
        src_positions, src_handles_l, src_handles_r, start_point);
    dst_positions[dst_range.first()] = start_point_insert.position;
    dst_handles_l[dst_range.first()] = start_point_insert.left_handle;
    dst_handles_r[dst_range.first()] = start_point_insert.right_handle;
    dst_types_l[dst_range.first()] = src_types_l[start_point.index];
    dst_types_r[dst_range.first()] = src_types_r[start_point.index];

    start_point_trimmed = true;
    ++dst_index;
  }

  /* Copy point data between the 'start_point' and 'end_point'. */
  int64_t increment = src_range.cycles() ? src_range.size_before_loop() :
                                           src_range.one_after_last() - src_range.first();

  const IndexRange dst_range_to_end(dst_index, increment);
  const IndexRange src_range_to_end(src_range.first(), increment);
  dst_positions.slice(dst_range_to_end).copy_from(src_positions.slice(src_range_to_end));
  dst_handles_l.slice(dst_range_to_end).copy_from(src_handles_l.slice(src_range_to_end));
  dst_handles_r.slice(dst_range_to_end).copy_from(src_handles_r.slice(src_range_to_end));
  dst_types_l.slice(dst_range_to_end).copy_from(src_types_l.slice(src_range_to_end));
  dst_types_r.slice(dst_range_to_end).copy_from(src_types_r.slice(src_range_to_end));
  dst_index += increment;

  if (dst_range.size() == 1) {
    BLI_assert(dst_index == dst_range.one_after_last());
    return;
  }

  increment = src_range.size_after_loop();
  if (src_range.cycles() && increment > 0) {
    const IndexRange dst_range_looped(dst_index, increment);
    const IndexRange src_range_looped(src_range.curve_range().first(), increment);
    dst_positions.slice(dst_range_looped).copy_from(src_positions.slice(src_range_looped));
    dst_handles_l.slice(dst_range_looped).copy_from(src_handles_l.slice(src_range_looped));
    dst_handles_r.slice(dst_range_looped).copy_from(src_handles_r.slice(src_range_looped));
    dst_types_l.slice(dst_range_looped).copy_from(src_types_l.slice(src_range_looped));
    dst_types_r.slice(dst_range_looped).copy_from(src_types_r.slice(src_range_looped));
    dst_index += increment;
  }

  if (start_point_trimmed) {
    dst_handles_l[dst_range.first() + 1] = start_point_insert.handle_next;
    /* No need to change handle type (remains the same). */
  }

  /* Handle 'end_point' */
  bke::curves::bezier::Insertion end_point_insert;
  if (end_point.parameter == 0.0f) {
    if (end_point.index == start_point.index) {
      /* Start point is same point or in the same segment. */
      if (start_point.parameter == 0.0f) {
        /* Same point. */
        BLI_assert(dst_range.size() == 1LL + src_range.size_range());
        dst_handles_l[dst_range.first()] = dst_positions[dst_range.first()];
        dst_handles_r[dst_range.last()] = dst_positions[dst_range.first()];
      }
      else if (start_point.parameter == 1.0f) {
        /* Start is next controlpoint, do nothing. */
      }
      else {
        /* Within the segment. */
        BLI_assert(dst_range.size() == 1LL + src_range.size_range() || dst_range.size() == 2);
        dst_handles_r[dst_range.last()] = start_point_insert.handle_prev;
      }
    }
    /* Start point is considered 'before' the endpoint and ignored. */
  }
  else if (end_point.parameter == 1.0f) {
    if (end_point.next_index == start_point.index) {
      /* Start point is same or in 'next' segment. */
      if (start_point.parameter == 0.0f) {
        /* Same point */
        BLI_assert(dst_range.size() == 1LL + src_range.size_range());
        dst_handles_l[dst_range.first()] = dst_positions[dst_range.first()];
        dst_handles_r[dst_range.last()] = dst_positions[dst_range.first()];
      }
      else if (start_point.parameter == 1.0f) {
        /* Start is next controlpoint, do nothing. */
      }
      else {
        /* In next segment. */
        BLI_assert(dst_range.size() == 1LL + src_range.size_range() || dst_range.size() == 2);
        dst_handles_r[dst_range.last()] = start_point_insert.handle_prev;
      }
    }
  }
  else {
    /* Trimmed in both ends within the same (and only) segment! Ensure both end points is not a
     * loop. */
    if (start_point.index == end_point.index && start_point.parameter < 1.0f) {
      BLI_assert(dst_range.size() == 2 || dst_range.size() == 2ll + src_range.size_range() ||
                 dst_range.size() == 1LL + src_range.size_range());

      if (start_point.parameter > end_point.parameter && start_point.parameter < 1.0f) {
        /* Start point comes after the endpoint within the segment. */
        BLI_assert(end_point.parameter >= 0.0f);

        const float parameter = end_point.parameter / start_point.parameter;
        end_point_insert = bke::curves::bezier::insert(dst_positions[dst_index - 1],
                                                       start_point_insert.handle_prev,
                                                       start_point_insert.left_handle,
                                                       start_point_insert.position,
                                                       parameter);

        /* Update start-point handle. */
        dst_handles_l[dst_range.first()] = end_point_insert.handle_next;
      }
      else {
        /* Start point lies before the endpoint within the segment. */

        const float parameter = (end_point.parameter - start_point.parameter) /
                                (1.0f - start_point.parameter);
        /* Unused only when parameter == 0.0f! */
        const float3 handle_next = start_point.parameter == 0.0f ?
                                       src_handles_l[end_point.next_index] :
                                       start_point_insert.handle_next;
        end_point_insert = bke::curves::bezier::insert(dst_positions[dst_index - 1],
                                                       dst_handles_r[dst_index - 1],
                                                       handle_next,
                                                       src_positions[end_point.next_index],
                                                       parameter);
      }
    }
    else {
      /* General case, compute the insertion point. */
      end_point_insert = knot_insert_bezier(
          src_positions, src_handles_l, src_handles_r, end_point);

      if ((start_point.parameter >= end_point.parameter && end_point.index == start_point.index) ||
          (start_point.parameter == 0.0f && end_point.next_index == start_point.index))
      {
        /* Start point is next controlpoint. */
        dst_handles_l[dst_range.first()] = end_point_insert.handle_next;
        /* No need to change handle type (remains the same). */
      }
    }

    dst_handles_r[dst_index - 1] = end_point_insert.handle_prev;
    dst_types_r[dst_index - 1] = src_types_l[end_point.index];

    dst_handles_l[dst_index] = end_point_insert.left_handle;
    dst_handles_r[dst_index] = end_point_insert.right_handle;
    dst_positions[dst_index] = end_point_insert.position;
    dst_types_l[dst_index] = src_types_l[end_point.next_index];
    dst_types_r[dst_index] = src_types_r[end_point.next_index];
#ifndef NDEBUG
    ++dst_index;
#endif
  }
  BLI_assert(dst_index == dst_range.one_after_last());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Trim Curves
 * \{ */

static void trim_attribute_linear(const bke::CurvesGeometry &src_curves,
                                  bke::CurvesGeometry &dst_curves,
                                  const IndexMask &selection,
                                  const Span<bke::curves::CurvePoint> start_points,
                                  const Span<bke::curves::CurvePoint> end_points,
                                  const Span<bke::curves::IndexRangeCyclic> src_ranges,
                                  MutableSpan<bke::AttributeTransferData> transfer_attributes)
{
  const OffsetIndices src_points_by_curve = src_curves.points_by_curve();
  const OffsetIndices dst_points_by_curve = dst_curves.points_by_curve();
  for (bke::AttributeTransferData &attribute : transfer_attributes) {
    bke::attribute_math::convert_to_static_type(attribute.meta_data.data_type, [&](auto dummy) {
      using T = decltype(dummy);

      selection.foreach_index(GrainSize(512), [&](const int curve_i) {
        const IndexRange src_points = src_points_by_curve[curve_i];
        sample_interval_linear<T>(attribute.src.template typed<T>().slice(src_points),
                                  attribute.dst.span.typed<T>(),
                                  src_ranges[curve_i],
                                  dst_points_by_curve[curve_i],
                                  start_points[curve_i],
                                  end_points[curve_i]);
      });
    });
  }
}

static void trim_polygonal_curves(const bke::CurvesGeometry &src_curves,
                                  bke::CurvesGeometry &dst_curves,
                                  const IndexMask &selection,
                                  const Span<bke::curves::CurvePoint> start_points,
                                  const Span<bke::curves::CurvePoint> end_points,
                                  const Span<bke::curves::IndexRangeCyclic> src_ranges,
                                  MutableSpan<bke::AttributeTransferData> transfer_attributes)
{
  const OffsetIndices src_points_by_curve = src_curves.points_by_curve();
  const OffsetIndices dst_points_by_curve = dst_curves.points_by_curve();
  const Span<float3> src_positions = src_curves.positions();
  MutableSpan<float3> dst_positions = dst_curves.positions_for_write();

  selection.foreach_index(GrainSize(512), [&](const int curve_i) {
    const IndexRange src_points = src_points_by_curve[curve_i];
    const IndexRange dst_points = dst_points_by_curve[curve_i];

    sample_interval_linear<float3>(src_positions.slice(src_points),
                                   dst_positions,
                                   src_ranges[curve_i],
                                   dst_points,
                                   start_points[curve_i],
                                   end_points[curve_i]);
  });
  fill_bezier_data(dst_curves, selection);
  fill_nurbs_data(dst_curves, selection);
  trim_attribute_linear(src_curves,
                        dst_curves,
                        selection,
                        start_points,
                        end_points,
                        src_ranges,
                        transfer_attributes);
}

static void trim_catmull_rom_curves(const bke::CurvesGeometry &src_curves,
                                    bke::CurvesGeometry &dst_curves,
                                    const IndexMask &selection,
                                    const Span<bke::curves::CurvePoint> start_points,
                                    const Span<bke::curves::CurvePoint> end_points,
                                    const Span<bke::curves::IndexRangeCyclic> src_ranges,
                                    MutableSpan<bke::AttributeTransferData> transfer_attributes)
{
  const OffsetIndices src_points_by_curve = src_curves.points_by_curve();
  const OffsetIndices dst_points_by_curve = dst_curves.points_by_curve();
  const Span<float3> src_positions = src_curves.positions();
  const VArray<bool> src_cyclic = src_curves.cyclic();
  MutableSpan<float3> dst_positions = dst_curves.positions_for_write();

  selection.foreach_index(GrainSize(512), [&](const int curve_i) {
    const IndexRange src_points = src_points_by_curve[curve_i];
    const IndexRange dst_points = dst_points_by_curve[curve_i];

    sample_interval_catmull_rom<float3>(src_positions.slice(src_points),
                                        dst_positions,
                                        src_ranges[curve_i],
                                        dst_points,
                                        start_points[curve_i],
                                        end_points[curve_i],
                                        src_cyclic[curve_i]);
  });
  fill_bezier_data(dst_curves, selection);
  fill_nurbs_data(dst_curves, selection);

  for (bke::AttributeTransferData &attribute : transfer_attributes) {
    bke::attribute_math::convert_to_static_type(attribute.meta_data.data_type, [&](auto dummy) {
      using T = decltype(dummy);

      selection.foreach_index(GrainSize(512), [&](const int curve_i) {
        const IndexRange src_points = src_points_by_curve[curve_i];
        const IndexRange dst_points = dst_points_by_curve[curve_i];

        sample_interval_catmull_rom<T>(attribute.src.template typed<T>().slice(src_points),
                                       attribute.dst.span.typed<T>(),
                                       src_ranges[curve_i],
                                       dst_points,
                                       start_points[curve_i],
                                       end_points[curve_i],
                                       src_cyclic[curve_i]);
      });
    });
  }
}

static void trim_bezier_curves(const bke::CurvesGeometry &src_curves,
                               bke::CurvesGeometry &dst_curves,
                               const IndexMask &selection,
                               const Span<bke::curves::CurvePoint> start_points,
                               const Span<bke::curves::CurvePoint> end_points,
                               const Span<bke::curves::IndexRangeCyclic> src_ranges,
                               MutableSpan<bke::AttributeTransferData> transfer_attributes)
{
  const OffsetIndices src_points_by_curve = src_curves.points_by_curve();
  const Span<float3> src_positions = src_curves.positions();
  const VArraySpan<int8_t> src_types_l{src_curves.handle_types_left()};
  const VArraySpan<int8_t> src_types_r{src_curves.handle_types_right()};
  const Span<float3> src_handles_l = *src_curves.handle_positions_left();
  const Span<float3> src_handles_r = *src_curves.handle_positions_right();

  const OffsetIndices dst_points_by_curve = dst_curves.points_by_curve();
  MutableSpan<float3> dst_positions = dst_curves.positions_for_write();
  MutableSpan<int8_t> dst_types_l = dst_curves.handle_types_left_for_write();
  MutableSpan<int8_t> dst_types_r = dst_curves.handle_types_right_for_write();
  MutableSpan<float3> dst_handles_l = dst_curves.handle_positions_left_for_write();
  MutableSpan<float3> dst_handles_r = dst_curves.handle_positions_right_for_write();

  selection.foreach_index(GrainSize(512), [&](const int curve_i) {
    const IndexRange src_points = src_points_by_curve[curve_i];
    const IndexRange dst_points = dst_points_by_curve[curve_i];

    sample_interval_bezier(src_positions.slice(src_points),
                           src_handles_l.slice(src_points),
                           src_handles_r.slice(src_points),
                           src_types_l.slice(src_points),
                           src_types_r.slice(src_points),
                           dst_positions,
                           dst_handles_l,
                           dst_handles_r,
                           dst_types_l,
                           dst_types_r,
                           src_ranges[curve_i],
                           dst_points,
                           start_points[curve_i],
                           end_points[curve_i]);
  });
  fill_nurbs_data(dst_curves, selection);
  trim_attribute_linear(src_curves,
                        dst_curves,
                        selection,
                        start_points,
                        end_points,
                        src_ranges,
                        transfer_attributes);
}

static void trim_evaluated_curves(const bke::CurvesGeometry &src_curves,
                                  bke::CurvesGeometry &dst_curves,
                                  const IndexMask &selection,
                                  const Span<bke::curves::CurvePoint> start_points,
                                  const Span<bke::curves::CurvePoint> end_points,
                                  const Span<bke::curves::IndexRangeCyclic> src_ranges,
                                  MutableSpan<bke::AttributeTransferData> transfer_attributes)
{
  const OffsetIndices src_points_by_curve = src_curves.points_by_curve();
  const OffsetIndices src_evaluated_points_by_curve = src_curves.evaluated_points_by_curve();
  const OffsetIndices dst_points_by_curve = dst_curves.points_by_curve();
  const Span<float3> src_eval_positions = src_curves.evaluated_positions();
  MutableSpan<float3> dst_positions = dst_curves.positions_for_write();

  selection.foreach_index(GrainSize(512), [&](const int curve_i) {
    const IndexRange src_evaluated_points = src_evaluated_points_by_curve[curve_i];
    const IndexRange dst_points = dst_points_by_curve[curve_i];
    sample_interval_linear<float3>(src_eval_positions.slice(src_evaluated_points),
                                   dst_positions,
                                   src_ranges[curve_i],
                                   dst_points,
                                   start_points[curve_i],
                                   end_points[curve_i]);
  });
  fill_bezier_data(dst_curves, selection);
  fill_nurbs_data(dst_curves, selection);

  for (bke::AttributeTransferData &attribute : transfer_attributes) {
    bke::attribute_math::convert_to_static_type(attribute.meta_data.data_type, [&](auto dummy) {
      using T = decltype(dummy);

      selection.foreach_segment(GrainSize(512), [&](const IndexMaskSegment segment) {
        Vector<std::byte> evaluated_buffer;
        for (const int64_t curve_i : segment) {
          const IndexRange src_points = src_points_by_curve[curve_i];

          /* Interpolate onto the evaluated point domain and sample the evaluated domain. */
          evaluated_buffer.reinitialize(sizeof(T) * src_evaluated_points_by_curve[curve_i].size());
          MutableSpan<T> evaluated = evaluated_buffer.as_mutable_span().cast<T>();
          src_curves.interpolate_to_evaluated(curve_i, attribute.src.slice(src_points), evaluated);
          sample_interval_linear<T>(evaluated,
                                    attribute.dst.span.typed<T>(),
                                    src_ranges[curve_i],
                                    dst_points_by_curve[curve_i],
                                    start_points[curve_i],
                                    end_points[curve_i]);
        }
      });
    });
  }
}

/* -------------------------------------------------------------------- */
/** \name Compute trim parameters
 * \{ */

static float trim_sample_length(const Span<float> accumulated_lengths,
                                const float sample_length,
                                const GeometryNodeCurveSampleMode mode)
{
  float length = mode == GEO_NODE_CURVE_SAMPLE_FACTOR ?
                     sample_length * accumulated_lengths.last() :
                     sample_length;
  return std::clamp(length, 0.0f, accumulated_lengths.last());
}

/**
 * Compute the selected range of points for every selected curve.
 */
static void compute_curve_trim_parameters(const bke::CurvesGeometry &curves,
                                          const IndexMask &selection,
                                          const VArray<float> &starts,
                                          const VArray<float> &ends,
                                          const GeometryNodeCurveSampleMode mode,
                                          MutableSpan<int> dst_curve_size,
                                          MutableSpan<bke::curves::CurvePoint> start_points,
                                          MutableSpan<bke::curves::CurvePoint> end_points,
                                          MutableSpan<bke::curves::IndexRangeCyclic> src_ranges)
{
  const OffsetIndices points_by_curve = curves.points_by_curve();
  const OffsetIndices evaluated_points_by_curve = curves.evaluated_points_by_curve();
  const VArray<bool> src_cyclic = curves.cyclic();
  const VArray<int> resolution = curves.resolution();
  const VArray<int8_t> curve_types = curves.curve_types();
  curves.ensure_can_interpolate_to_evaluated();

  selection.foreach_index(GrainSize(128), [&](const int curve_i) {
    CurveType curve_type = CurveType(curve_types[curve_i]);

    int point_count;
    if (curve_type == CURVE_TYPE_NURBS) {
      /* The result curve is a poly curve. */
      point_count = evaluated_points_by_curve[curve_i].size();
    }
    else {
      point_count = points_by_curve[curve_i].size();
    }
    if (point_count == 1) {
      /* Single point. */
      dst_curve_size[curve_i] = 1;
      src_ranges[curve_i] = bke::curves::IndexRangeCyclic(0, 0, 1, 1);
      start_points[curve_i] = {{0, 0}, 0.0f};
      end_points[curve_i] = {{0, 0}, 0.0f};
      return;
    }

    const bool cyclic = src_cyclic[curve_i];
    const Span<float> lengths = curves.evaluated_lengths_for_curve(curve_i, cyclic);
    BLI_assert(lengths.size() > 0);

    const float start_length = trim_sample_length(lengths, starts[curve_i], mode);
    float end_length;

    bool equal_sample_point;
    if (cyclic) {
      end_length = trim_sample_length(lengths, ends[curve_i], mode);
      const float cyclic_start = start_length == lengths.last() ? 0.0f : start_length;
      const float cyclic_end = end_length == lengths.last() ? 0.0f : end_length;
      equal_sample_point = cyclic_start == cyclic_end;
    }
    else {
      end_length = ends[curve_i] <= starts[curve_i] ?
                       start_length :
                       trim_sample_length(lengths, ends[curve_i], mode);
      equal_sample_point = start_length == end_length;
    }

    start_points[curve_i] = lookup_curve_point(curves,
                                               evaluated_points_by_curve,
                                               curve_type,
                                               curve_i,
                                               lengths,
                                               start_length,
                                               cyclic,
                                               resolution[curve_i],
                                               point_count);
    if (equal_sample_point) {
      end_points[curve_i] = start_points[curve_i];
      if (end_length <= start_length) {
        /* Single point. */
        dst_curve_size[curve_i] = 1;
        if (start_points[curve_i].is_controlpoint()) {
          /* Only iterate if control point. */
          const int single_point_index = start_points[curve_i].parameter == 1.0f ?
                                             start_points[curve_i].next_index :
                                             start_points[curve_i].index;
          src_ranges[curve_i] = bke::curves::IndexRangeCyclic::get_range_from_size(
              single_point_index, 1, point_count);
        }
        /* else: leave empty range */
      }
      else {
        /* Split. */
        src_ranges[curve_i] = bke::curves::IndexRangeCyclic::get_range_between_endpoints(
                                  start_points[curve_i], end_points[curve_i], point_count)
                                  .push_loop();
        const int count = 1 + !start_points[curve_i].is_controlpoint() + point_count;
        BLI_assert(count > 1);
        dst_curve_size[curve_i] = count;
      }
    }
    else {
      /* General case. */
      end_points[curve_i] = lookup_curve_point(curves,
                                               evaluated_points_by_curve,
                                               curve_type,
                                               curve_i,
                                               lengths,
                                               end_length,
                                               cyclic,
                                               resolution[curve_i],
                                               point_count);

      src_ranges[curve_i] = bke::curves::IndexRangeCyclic::get_range_between_endpoints(
          start_points[curve_i], end_points[curve_i], point_count);
      const int count = src_ranges[curve_i].size() + !start_points[curve_i].is_controlpoint() +
                        !end_points[curve_i].is_controlpoint();
      BLI_assert(count > 1);
      dst_curve_size[curve_i] = count;
    }
    BLI_assert(dst_curve_size[curve_i] > 0);
  });
}

/** \} */

bke::CurvesGeometry trim_curves(const bke::CurvesGeometry &src_curves,
                                const IndexMask &selection,
                                const VArray<float> &starts,
                                const VArray<float> &ends,
                                const GeometryNodeCurveSampleMode mode,
                                const bke::AttributeFilter &attribute_filter)
{
  const OffsetIndices src_points_by_curve = src_curves.points_by_curve();
  IndexMaskMemory memory;
  const IndexMask unselected = selection.complement(src_curves.curves_range(), memory);

  BLI_assert(selection.size() > 0);
  BLI_assert(selection.last() <= src_curves.curves_num());
  BLI_assert(starts.size() == src_curves.curves_num());
  BLI_assert(starts.size() == ends.size());
  src_curves.ensure_evaluated_lengths();

  bke::CurvesGeometry dst_curves = bke::curves::copy_only_curve_domain(src_curves);
  MutableSpan<int> dst_curve_offsets = dst_curves.offsets_for_write();
  Array<bke::curves::CurvePoint, 16> start_points(src_curves.curves_num());
  Array<bke::curves::CurvePoint, 16> end_points(src_curves.curves_num());
  Array<bke::curves::IndexRangeCyclic, 16> src_ranges(src_curves.curves_num());
  compute_curve_trim_parameters(src_curves,
                                selection,
                                starts,
                                ends,
                                mode,
                                dst_curve_offsets,
                                start_points,
                                end_points,
                                src_ranges);
  offset_indices::copy_group_sizes(src_points_by_curve, unselected, dst_curve_offsets);
  offset_indices::accumulate_counts_to_offsets(dst_curve_offsets);
  const OffsetIndices dst_points_by_curve = dst_curves.points_by_curve();
  dst_curves.resize(dst_curves.offsets().last(), dst_curves.curves_num());

  /* Populate curve domain. */
  const bke::AttributeAccessor src_attributes = src_curves.attributes();
  bke::MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();
  Set<std::string> transfer_curve_skip = {"cyclic", "curve_type", "nurbs_order", "knots_mode"};
  if (dst_curves.has_curve_with_type(CURVE_TYPE_NURBS)) {
    /* If a NURBS curve is copied keep */
    transfer_curve_skip.remove("nurbs_order");
    transfer_curve_skip.remove("knots_mode");
  }

  /* Fetch custom point domain attributes for transfer (copy). */
  Vector<bke::AttributeTransferData> transfer_attributes = bke::retrieve_attributes_for_transfer(
      src_attributes,
      dst_attributes,
      {bke::AttrDomain::Point},
      bke::attribute_filter_with_skip_ref(attribute_filter,
                                          {"position",
                                           "handle_left",
                                           "handle_right",
                                           "handle_type_left",
                                           "handle_type_right",
                                           "nurbs_weight"}));

  auto trim_catmull = [&](const IndexMask &selection) {
    trim_catmull_rom_curves(src_curves,
                            dst_curves,
                            selection,
                            start_points,
                            end_points,
                            src_ranges,
                            transfer_attributes);
  };
  auto trim_poly = [&](const IndexMask &selection) {
    trim_polygonal_curves(src_curves,
                          dst_curves,
                          selection,
                          start_points,
                          end_points,
                          src_ranges,
                          transfer_attributes);
  };
  auto trim_bezier = [&](const IndexMask &selection) {
    trim_bezier_curves(src_curves,
                       dst_curves,
                       selection,
                       start_points,
                       end_points,
                       src_ranges,
                       transfer_attributes);
  };
  auto trim_evaluated = [&](const IndexMask &selection) {
    dst_curves.fill_curve_types(selection, CURVE_TYPE_POLY);
    /* Ensure evaluated positions are available. */
    src_curves.evaluated_positions();
    trim_evaluated_curves(src_curves,
                          dst_curves,
                          selection,
                          start_points,
                          end_points,
                          src_ranges,
                          transfer_attributes);
  };

  /* Populate point domain. */
  bke::curves::foreach_curve_by_type(src_curves.curve_types(),
                                     src_curves.curve_type_counts(),
                                     selection,
                                     trim_catmull,
                                     trim_poly,
                                     trim_bezier,
                                     trim_evaluated);

  /* Cleanup/close context */
  for (bke::AttributeTransferData &attribute : transfer_attributes) {
    attribute.dst.finish();
  }

  /* Copy unselected */
  if (unselected.is_empty()) {
    /* Since all curves were trimmed, none of them are cyclic and the attribute can be removed. */
    dst_curves.attributes_for_write().remove("cyclic");
  }
  else {
    /* Only trimmed curves are no longer cyclic. */
    if (bke::SpanAttributeWriter cyclic = dst_attributes.lookup_for_write_span<bool>("cyclic")) {
      index_mask::masked_fill(cyclic.span, false, selection);
      cyclic.finish();
    }

    Set<std::string> copy_point_skip;
    if (!dst_curves.has_curve_with_type(CURVE_TYPE_NURBS) &&
        src_curves.has_curve_with_type(CURVE_TYPE_NURBS))
    {
      copy_point_skip.add("nurbs_weight");
    }

    bke::copy_attributes_group_to_group(
        src_attributes,
        bke::AttrDomain::Point,
        bke::AttrDomain::Point,
        bke::attribute_filter_with_skip_ref(attribute_filter, copy_point_skip),
        src_points_by_curve,
        dst_points_by_curve,
        unselected,
        dst_attributes);
  }

  dst_curves.remove_attributes_based_on_types();
  dst_curves.tag_topology_changed();
  if (src_curves.nurbs_has_custom_knots()) {
    bke::curves::nurbs::update_custom_knot_modes(
        dst_curves.curves_range(), NURBS_KNOT_MODE_NORMAL, NURBS_KNOT_MODE_NORMAL, dst_curves);
  }
  return dst_curves;
}

/** \} */

}  // namespace blender::geometry
