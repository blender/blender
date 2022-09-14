/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BLI_array_utils.hh"
#include "BLI_length_parameterize.hh"

#include "BKE_attribute.hh"
#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"
#include "BKE_curves_utils.hh"
#include "BKE_geometry_set.hh"

#include "GEO_trim_curves.hh"

namespace blender::geometry {

/* -------------------------------------------------------------------- */
/** \name Curve Enums
 * \{ */

#define CURVE_TYPE_AS_MASK(curve_type) ((CurveTypeMask)((1 << (int)(curve_type))))

typedef enum CurveTypeMask {
  CURVE_TYPE_MASK_CATMULL_ROM = (1 << 0),
  CURVE_TYPE_MASK_POLY = (1 << 1),
  CURVE_TYPE_MASK_BEZIER = (1 << 2),
  CURVE_TYPE_MASK_NURBS = (1 << 3),
  CURVE_TYPE_MASK_ALL = (1 << 4) - 1
} CurveTypeMask;

/** \} */

/* -------------------------------------------------------------------- */
/** \name #IndexRangeCyclic Utilities
 * \{ */

/**
 * Create a cyclical iterator for all control points within the interval [start_point, end_point]
 * including any control point at the start or end point.
 *
 * \param start_point Point on the curve that define the starting point of the interval.
 * \param end_point Point on the curve that define the end point of the interval (included).
 * \param points IndexRange for the curve points.
 */
static bke::curves::IndexRangeCyclic get_range_between_endpoints(
    const bke::curves::CurvePoint start_point,
    const bke::curves::CurvePoint end_point,
    const IndexRange points)
{
  const int64_t start_index = start_point.parameter == 0.0 ? start_point.index :
                                                             start_point.next_index;
  int64_t end_index = end_point.parameter == 0.0 ? end_point.index : end_point.next_index;
  int64_t cycles;

  if (end_point.is_controlpoint()) {
    ++end_index;
    if (end_index > points.last()) {
      end_index = points.one_after_last();
    }
    /* end_point < start_point but parameter is irrelevant (end_point is controlpoint), and loop
     * when equal due to increment. */
    cycles = end_index <= start_index;
  }
  else {
    cycles = end_point < start_point || end_index < start_index;
  }
  return bke::curves::IndexRangeCyclic(start_index, end_index, points, cycles);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lookup Curve Points
 * \{ */

/**
 * Find the point on the curve defined by the distance along the curve. Assumes curve resolution is
 * constant for all curve segments and evaluated curve points are uniformly spaced between the
 * segment endpoints in relation to the curve parameter.
 *
 * \param lengths: Accumulated lenght for the evaluated curve.
 * \param sample_length: Distance along the curve to determine the CurvePoint for.
 * \param cyclic: If curve is cyclic.
 * \param resolution: Curve resolution (number of evaluated points per segment).
 * \param num_curve_points: Total number of control points in the curve.
 * \return: Point on the piecewise segment matching the given distance.
 */
static bke::curves::CurvePoint lookup_curve_point(const Span<float> lengths,
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
static bke::curves::CurvePoint lookup_evaluated_point(const Span<float> lengths,
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
static bke::curves::CurvePoint lookup_bezier_point(const Span<int> bezier_offsets,
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
  const int left = offset - bezier_offsets.begin();
  const int right = left == last_index ? 0 : left + 1;

  const int prev_offset = left == 0 ? 0 : bezier_offsets[(int64_t)left - 1];
  const float offset_in_segment = eval_factor + eval_index - prev_offset;
  const int segment_resolution = bezier_offsets[left] - prev_offset;
  const float parameter = std::clamp(offset_in_segment / segment_resolution, 0.0f, 1.0f);

  return {{left, right}, parameter};
}

Array<bke::curves::CurvePoint, 12> lookup_curve_points(const bke::CurvesGeometry &curves,
                                                       const Span<float> lengths,
                                                       const Span<int64_t> curve_indices,
                                                       const bool normalized_factors)
{
  BLI_assert(lengths.size() == curve_indices.size());
  BLI_assert(*std::max_element(curve_indices.begin(), curve_indices.end()) < curves.curves_num());

  const VArray<bool> cyclic = curves.cyclic();
  const VArray<int> resolution = curves.resolution();
  const VArray<int8_t> curve_types = curves.curve_types();

  /* Compute curve lenghts! */
  curves.ensure_evaluated_lengths();
  curves.ensure_evaluated_offsets();

  /* Find the curve points referenced by the input! */
  Array<bke::curves::CurvePoint, 12> lookups(curve_indices.size());
  threading::parallel_for(curve_indices.index_range(), 128, [&](const IndexRange range) {
    for (const int64_t lookup_index : range) {
      const int64_t curve_i = curve_indices[lookup_index];

      const int point_count = curves.points_num_for_curve(curve_i);
      if (curve_i < 0 || point_count == 1) {
        lookups[lookup_index] = {{0, 0}, 0.0f};
        continue;
      }

      const Span<float> accumulated_lengths = curves.evaluated_lengths_for_curve(curve_i,
                                                                                 cyclic[curve_i]);
      BLI_assert(accumulated_lengths.size() > 0);

      const float sample_length = normalized_factors ?
                                      lengths[lookup_index] * accumulated_lengths.last() :
                                      lengths[lookup_index];

      const CurveType curve_type = (CurveType)curve_types[curve_i];

      switch (curve_type) {
        case CURVE_TYPE_BEZIER: {
          if (bke::curves::bezier::has_vector_handles(
                  point_count,
                  curves.evaluated_points_for_curve(curve_i).size(),
                  cyclic[curve_i],
                  resolution[curve_i])) {
            const Span<int> bezier_offsets = curves.bezier_evaluated_offsets_for_curve(curve_i);
            lookups[lookup_index] = lookup_bezier_point(
                bezier_offsets, accumulated_lengths, sample_length, cyclic[curve_i], point_count);
          }
          else {
            lookups[lookup_index] = lookup_curve_point(accumulated_lengths,
                                                       sample_length,
                                                       cyclic[curve_i],
                                                       resolution[curve_i],
                                                       point_count);
          }
          break;
        }
        case CURVE_TYPE_CATMULL_ROM: {
          lookups[lookup_index] = lookup_curve_point(accumulated_lengths,
                                                     sample_length,
                                                     cyclic[curve_i],
                                                     resolution[curve_i],
                                                     point_count);
          break;
        }
        case CURVE_TYPE_NURBS:
        case CURVE_TYPE_POLY:
        default: {
          /* Handle general case as an "evaluated" or polygonal curve. */
          BLI_assert(resolution[curve_i] > 0);
          lookups[lookup_index] = lookup_evaluated_point(
              accumulated_lengths,
              sample_length,
              cyclic[curve_i],
              curves.evaluated_points_for_curve(curve_i).size());
          break;
        }
      }
    }
  });
  return lookups;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transfer Curve Domain
 * \{ */

/**
 * Determine curve type(s) for the copied curves given the supported set of types and knot modes.
 * If a curve type is not supported the default type is set.
 */
static void determine_copyable_curve_types(const bke::CurvesGeometry &src_curves,
                                           bke::CurvesGeometry &dst_curves,
                                           const IndexMask selection,
                                           const IndexMask selection_inverse,
                                           const CurveTypeMask supported_curve_type_mask,
                                           const int8_t default_curve_type = (int8_t)
                                               CURVE_TYPE_POLY)
{
  const VArray<int8_t> src_curve_types = src_curves.curve_types();
  const VArray<int8_t> src_knot_modes = src_curves.nurbs_knots_modes();
  MutableSpan<int8_t> dst_curve_types = dst_curves.curve_types_for_write();

  threading::parallel_for(selection.index_range(), 4096, [&](const IndexRange selection_range) {
    for (const int64_t curve_i : selection.slice(selection_range)) {
      if (supported_curve_type_mask & CURVE_TYPE_AS_MASK(src_curve_types[curve_i])) {
        dst_curve_types[curve_i] = src_curve_types[curve_i];
      }
      else {
        dst_curve_types[curve_i] = default_curve_type;
      }
    }
  });

  array_utils::copy(src_curve_types, selection_inverse, dst_curve_types);
}

/**
 * Determine if a curve is treated as an evaluated curve. Curves which inheretly do not support
 * trimming are discretized (e.g. NURBS).
 */
static bool copy_as_evaluated_curve(const int8_t src_type, const int8_t dst_type)
{
  return src_type != CURVE_TYPE_POLY && dst_type == CURVE_TYPE_POLY;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Specialized Curve Constructors
 * \{ */

static void compute_trim_result_offsets(const bke::CurvesGeometry &src_curves,
                                        const IndexMask selection,
                                        const IndexMask inverse_selection,
                                        const Span<bke::curves::CurvePoint> start_points,
                                        const Span<bke::curves::CurvePoint> end_points,
                                        const VArray<int8_t> dst_curve_types,
                                        MutableSpan<int> dst_curve_offsets,
                                        Vector<int64_t> &r_curve_indices,
                                        Vector<int64_t> &r_point_curve_indices)
{
  BLI_assert(r_curve_indices.size() == 0);
  BLI_assert(r_point_curve_indices.size() == 0);
  const VArray<bool> cyclic = src_curves.cyclic();
  const VArray<int8_t> curve_types = src_curves.curve_types();
  r_curve_indices.reserve(selection.size());

  for (const int64_t curve_i : selection) {

    int64_t src_point_count;

    if (copy_as_evaluated_curve(curve_types[curve_i], dst_curve_types[curve_i])) {
      src_point_count = src_curves.evaluated_points_for_curve(curve_i).size();
    }
    else {
      src_point_count = (int64_t)src_curves.points_num_for_curve(curve_i);
    }
    BLI_assert(src_point_count > 0);

    if (start_points[curve_i] == end_points[curve_i]) {
      dst_curve_offsets[curve_i] = 1;
      r_point_curve_indices.append(curve_i);
    }
    else {
      const bke::curves::IndexRangeCyclic point_range = get_range_between_endpoints(
          start_points[curve_i], end_points[curve_i], {0, src_point_count});
      const int count = point_range.size() + !start_points[curve_i].is_controlpoint() +
                        !end_points[curve_i].is_controlpoint();
      dst_curve_offsets[curve_i] = count;
      r_curve_indices.append(curve_i);
    }
    BLI_assert(dst_curve_offsets[curve_i] > 0);
  }
  threading::parallel_for(
      inverse_selection.index_range(), 4096, [&](const IndexRange selection_range) {
        for (const int64_t curve_i : inverse_selection.slice(selection_range)) {
          dst_curve_offsets[curve_i] = src_curves.points_num_for_curve(curve_i);
        }
      });
  bke::curves::accumulate_counts_to_offsets(dst_curve_offsets);
}

/* --------------------------------------------------------------------
 * Utility functions.
 */

static void fill_bezier_data(bke::CurvesGeometry &dst_curves, const IndexMask selection)
{
  if (dst_curves.has_curve_with_type(CURVE_TYPE_BEZIER)) {
    MutableSpan<float3> handle_positions_left = dst_curves.handle_positions_left_for_write();
    MutableSpan<float3> handle_positions_right = dst_curves.handle_positions_right_for_write();
    MutableSpan<int8_t> handle_types_left = dst_curves.handle_types_left_for_write();
    MutableSpan<int8_t> handle_types_right = dst_curves.handle_types_right_for_write();

    threading::parallel_for(selection.index_range(), 4096, [&](const IndexRange range) {
      for (const int64_t curve_i : selection.slice(range)) {
        const IndexRange points = dst_curves.points_for_curve(curve_i);
        handle_types_right.slice(points).fill((int8_t)BEZIER_HANDLE_FREE);
        handle_types_left.slice(points).fill((int8_t)BEZIER_HANDLE_FREE);
        handle_positions_left.slice(points).fill({0.0f, 0.0f, 0.0f});
        handle_positions_right.slice(points).fill({0.0f, 0.0f, 0.0f});
      }
    });
  }
}
static void fill_nurbs_data(bke::CurvesGeometry &dst_curves, const IndexMask selection)
{
  if (dst_curves.has_curve_with_type(CURVE_TYPE_NURBS)) {
    bke::curves::fill_points(dst_curves, selection, 0.0f, dst_curves.nurbs_weights_for_write());
  }
}

template<typename T>
static int64_t copy_point_data_between_endpoints(const Span<T> src_data,
                                                 MutableSpan<T> dst_data,
                                                 const bke::curves::IndexRangeCyclic src_range,
                                                 const int64_t src_index,
                                                 int64_t dst_index)
{
  int64_t increment;
  if (src_range.cycles()) {
    increment = src_range.size_before_loop();
    dst_data.slice(dst_index, increment).copy_from(src_data.slice(src_index, increment));
    dst_index += increment;

    increment = src_range.size_after_loop();
    dst_data.slice(dst_index, increment)
        .copy_from(src_data.slice(src_range.curve_range().first(), increment));
    dst_index += increment;
  }
  else {
    increment = src_range.one_after_last() - src_range.first();
    dst_data.slice(dst_index, increment).copy_from(src_data.slice(src_index, increment));
    dst_index += increment;
  }
  return dst_index;
}

/* --------------------------------------------------------------------
 * Sampling utilities.
 */

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

/* --------------------------------------------------------------------
 * Sample single point.
 */

template<typename T>
static void sample_linear(const Span<T> src_data,
                          MutableSpan<T> dst_data,
                          const IndexRange dst_range,
                          const bke::curves::CurvePoint sample_point)
{
  BLI_assert(dst_range.size() == 1);
  if (sample_point.is_controlpoint()) {
    /* Resolves cases where the source curve consist of a single control point. */
    const int index = sample_point.parameter == 1.0 ? sample_point.next_index : sample_point.index;
    dst_data[dst_range.first()] = src_data[index];
  }
  else {
    dst_data[dst_range.first()] = attribute_math::mix2(
        sample_point.parameter, src_data[sample_point.index], src_data[sample_point.next_index]);
  }
}

template<typename T>
static void sample_catmull_rom(const Span<T> src_data,
                               MutableSpan<T> dst_data,
                               const IndexRange dst_range,
                               const bke::curves::CurvePoint sample_point,
                               const bool src_cyclic)
{
  BLI_assert(dst_range.size() == 1);
  if (sample_point.is_controlpoint()) {
    /* Resolves cases where the source curve consist of a single control point. */
    const int index = sample_point.parameter == 1.0 ? sample_point.next_index : sample_point.index;
    dst_data[dst_range.first()] = src_data[index];
  }
  else {
    dst_data[dst_range.first()] = interpolate_catmull_rom(src_data, sample_point, src_cyclic);
  }
}

static void sample_bezier(const Span<float3> src_positions,
                          const Span<float3> src_handles_l,
                          const Span<float3> src_handles_r,
                          const Span<int8_t> src_types_l,
                          const Span<int8_t> src_types_r,
                          MutableSpan<float3> dst_positions,
                          MutableSpan<float3> dst_handles_l,
                          MutableSpan<float3> dst_handles_r,
                          MutableSpan<int8_t> dst_types_l,
                          MutableSpan<int8_t> dst_types_r,
                          const IndexRange dst_range,
                          const bke::curves::CurvePoint sample_point)
{
  BLI_assert(dst_range.size() == 1);
  if (sample_point.is_controlpoint()) {
    /* Resolves cases where the source curve consist of a single control point. */
    const int index = sample_point.parameter == 1.0 ? sample_point.next_index : sample_point.index;
    dst_positions[dst_range.first()] = src_positions[index];
    dst_handles_l[dst_range.first()] = src_handles_l[index];
    dst_handles_r[dst_range.first()] = src_handles_r[index];
    dst_types_l[dst_range.first()] = src_types_l[index];
    dst_types_r[dst_range.first()] = src_types_r[index];
  }
  else {
    bke::curves::bezier::Insertion insertion_point = knot_insert_bezier(
        src_positions, src_handles_l, src_handles_r, sample_point);
    dst_positions[dst_range.first()] = insertion_point.position;
    dst_handles_l[dst_range.first()] = insertion_point.left_handle;
    dst_handles_r[dst_range.first()] = insertion_point.right_handle;
    dst_types_l[dst_range.first()] = BEZIER_HANDLE_FREE;
    dst_types_r[dst_range.first()] = BEZIER_HANDLE_FREE;
  }
}

/* --------------------------------------------------------------------
 * Sample curve interval (trim).
 */

/**
 * Sample source curve data in the interval defined by the points [start_point, end_point].
 * Uses linear interpolation to compute the endpoints.
 *
 * \tparam include_start_point If False, the 'start_point' point sample will not be copied
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
                                   const bke::curves::IndexRangeCyclic src_range,
                                   const IndexRange dst_range,
                                   const bke::curves::CurvePoint start_point,
                                   const bke::curves::CurvePoint end_point)
{
  int64_t src_index = src_range.first();
  int64_t dst_index = dst_range.first();

  if (start_point.is_controlpoint()) {
    /* 'start_point' is included in the copy iteration. */
    if constexpr (!include_start_point) {
      /* Skip first. */
      ++src_index;
    }
  }
  else if constexpr (!include_start_point) {
    /* Do nothing (excluded). */
  }
  else {
    /* General case, sample 'start_point' */
    dst_data[dst_index] = attribute_math::mix2(
        start_point.parameter, src_data[start_point.index], src_data[start_point.next_index]);
    ++dst_index;
  }

  dst_index = copy_point_data_between_endpoints(
      src_data, dst_data, src_range, src_index, dst_index);

  /* Handle last case */
  if (end_point.is_controlpoint()) {
    /* 'end_point' is included in the copy iteration. */
  }
  else {
    dst_data[dst_index] = attribute_math::mix2(
        end_point.parameter, src_data[end_point.index], src_data[end_point.next_index]);
#ifdef DEBUG
    ++dst_index;
#endif
  }
  BLI_assert(dst_index == dst_range.one_after_last());
}

template<typename T, bool include_start_point = true>
static void sample_interval_catmull_rom(const Span<T> src_data,
                                        MutableSpan<T> dst_data,
                                        const bke::curves::IndexRangeCyclic src_range,
                                        const IndexRange dst_range,
                                        const bke::curves::CurvePoint start_point,
                                        const bke::curves::CurvePoint end_point,
                                        const bool src_cyclic)
{
  int64_t src_index = src_range.first();
  int64_t dst_index = dst_range.first();

  if (start_point.is_controlpoint()) {
    /* 'start_point' is included in the copy iteration. */
    if constexpr (!include_start_point) {
      /* Skip first. */
      ++src_index;
    }
  }
  else if constexpr (!include_start_point) {
    /* Do nothing (excluded). */
  }
  else {
    /* General case, sample 'start_point' */
    dst_data[dst_index] = interpolate_catmull_rom(src_data, start_point, src_cyclic);
    ++dst_index;
  }

  dst_index = copy_point_data_between_endpoints(
      src_data, dst_data, src_range, src_index, dst_index);

  /* Handle last case */
  if (end_point.is_controlpoint()) {
    /* 'end_point' is included in the copy iteration. */
  }
  else {
    dst_data[dst_index] = interpolate_catmull_rom(src_data, end_point, src_cyclic);
#ifdef DEBUG
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
                                   const bke::curves::IndexRangeCyclic src_range,
                                   const IndexRange dst_range,
                                   const bke::curves::CurvePoint start_point,
                                   const bke::curves::CurvePoint end_point)
{
  bke::curves::bezier::Insertion start_point_insert;
  int64_t src_index = src_range.first();
  int64_t dst_index = dst_range.first();

  bool start_point_trimmed = false;
  if (start_point.is_controlpoint()) {
    /* The 'start_point' control point is included in the copy iteration. */
    if constexpr (!include_start_point) {
      ++src_index; /* Skip first! */
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
  const IndexRange src_range_to_end(src_index, increment);
  dst_positions.slice(dst_range_to_end).copy_from(src_positions.slice(src_range_to_end));
  dst_handles_l.slice(dst_range_to_end).copy_from(src_handles_l.slice(src_range_to_end));
  dst_handles_r.slice(dst_range_to_end).copy_from(src_handles_r.slice(src_range_to_end));
  dst_types_l.slice(dst_range_to_end).copy_from(src_types_l.slice(src_range_to_end));
  dst_types_r.slice(dst_range_to_end).copy_from(src_types_r.slice(src_range_to_end));
  dst_index += increment;

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
    /* No need to set handle type (remains the same)! */
  }

  /* Handle 'end_point' */
  bke::curves::bezier::Insertion end_point_insert;
  if (end_point.is_controlpoint()) {
    /* Do nothing, the 'end_point' control point is included in the copy iteration. */
  }
  else {
    /* Trimmed in both ends within the same (and only) segment! Ensure both end points is not a
     * loop.*/
    if (start_point_trimmed && start_point.index == end_point.index &&
        start_point.parameter <= end_point.parameter) {

      /* Copy following segment control point. */
      dst_positions[dst_index] = src_positions[end_point.next_index];
      dst_handles_r[dst_index] = src_handles_r[end_point.next_index];

      /* Compute interpolation in the result curve. */
      const float parameter = (end_point.parameter - start_point.parameter) /
                              (1.0f - start_point.parameter);
      end_point_insert = knot_insert_bezier(
          dst_positions,
          dst_handles_l,
          dst_handles_r,
          {{(int)dst_range.first(), (int)(dst_range.first() + 1)}, parameter});
    }
    else {
      /* General case, compute the insertion point.  */
      end_point_insert = knot_insert_bezier(
          src_positions, src_handles_l, src_handles_r, end_point);
    }

    dst_handles_r[dst_index - 1] = end_point_insert.handle_prev;
    dst_types_r[dst_index - 1] = src_types_l[end_point.index];

    dst_handles_l[dst_index] = end_point_insert.left_handle;
    dst_handles_r[dst_index] = end_point_insert.right_handle;
    dst_positions[dst_index] = end_point_insert.position;
    dst_types_l[dst_index] = src_types_l[end_point.next_index];
    dst_types_r[dst_index] = src_types_r[end_point.next_index];
#ifdef DEBUG
    ++dst_index;
#endif  // DEBUG
  }
  BLI_assert(dst_index == dst_range.one_after_last());
}

/* --------------------------------------------------------------------
 * Convert to point curves.
 */

static void convert_point_polygonal_curves(
    const bke::CurvesGeometry &src_curves,
    bke::CurvesGeometry &dst_curves,
    const IndexMask selection,
    const Span<bke::curves::CurvePoint> sample_points,
    MutableSpan<bke::AttributeTransferData> transfer_attributes)
{
  const Span<float3> src_positions = src_curves.positions();
  MutableSpan<float3> dst_positions = dst_curves.positions_for_write();

  threading::parallel_for(selection.index_range(), 4096, [&](const IndexRange range) {
    for (const int64_t curve_i : selection.slice(range)) {
      const IndexRange src_points = src_curves.points_for_curve(curve_i);
      const IndexRange dst_points = dst_curves.points_for_curve(curve_i);

      sample_linear<float3>(
          src_positions.slice(src_points), dst_positions, dst_points, sample_points[curve_i]);

      for (bke::AttributeTransferData &attribute : transfer_attributes) {
        attribute_math::convert_to_static_type(attribute.meta_data.data_type, [&](auto dummy) {
          using T = decltype(dummy);
          sample_linear<T>(attribute.src.template typed<T>().slice(src_points),
                           attribute.dst.span.typed<T>(),
                           dst_curves.points_for_curve(curve_i),
                           sample_points[curve_i]);
        });
      }
    }
  });

  fill_bezier_data(dst_curves, selection);
  fill_nurbs_data(dst_curves, selection);
}

static void convert_point_catmull_curves(
    const bke::CurvesGeometry &src_curves,
    bke::CurvesGeometry &dst_curves,
    const IndexMask selection,
    const Span<bke::curves::CurvePoint> sample_points,
    MutableSpan<bke::AttributeTransferData> transfer_attributes)
{
  const Span<float3> src_positions = src_curves.positions();
  const VArray<bool> src_cyclic = src_curves.cyclic();

  MutableSpan<float3> dst_positions = dst_curves.positions_for_write();

  threading::parallel_for(selection.index_range(), 4096, [&](const IndexRange range) {
    for (const int64_t curve_i : selection.slice(range)) {
      const IndexRange src_points = src_curves.points_for_curve(curve_i);
      const IndexRange dst_points = dst_curves.points_for_curve(curve_i);

      sample_catmull_rom<float3>(src_positions.slice(src_points),
                                 dst_positions,
                                 dst_points,
                                 sample_points[curve_i],
                                 src_cyclic[curve_i]);
      for (bke::AttributeTransferData &attribute : transfer_attributes) {
        attribute_math::convert_to_static_type(attribute.meta_data.data_type, [&](auto dummy) {
          using T = decltype(dummy);
          sample_catmull_rom<T>(attribute.src.template typed<T>().slice(src_points),
                                attribute.dst.span.typed<T>(),
                                dst_points,
                                sample_points[curve_i],
                                src_cyclic[curve_i]);
        });
      }
    }
  });
  fill_bezier_data(dst_curves, selection);
  fill_nurbs_data(dst_curves, selection);
}

static void convert_point_bezier_curves(
    const bke::CurvesGeometry &src_curves,
    bke::CurvesGeometry &dst_curves,
    const IndexMask selection,
    const Span<bke::curves::CurvePoint> sample_points,
    MutableSpan<bke::AttributeTransferData> transfer_attributes)
{
  const Span<float3> src_positions = src_curves.positions();
  const VArraySpan<int8_t> src_types_l{src_curves.handle_types_left()};
  const VArraySpan<int8_t> src_types_r{src_curves.handle_types_right()};
  const Span<float3> src_handles_l = src_curves.handle_positions_left();
  const Span<float3> src_handles_r = src_curves.handle_positions_right();

  MutableSpan<float3> dst_positions = dst_curves.positions_for_write();
  MutableSpan<int8_t> dst_types_l = dst_curves.handle_types_left_for_write();
  MutableSpan<int8_t> dst_types_r = dst_curves.handle_types_right_for_write();
  MutableSpan<float3> dst_handles_l = dst_curves.handle_positions_left_for_write();
  MutableSpan<float3> dst_handles_r = dst_curves.handle_positions_right_for_write();

  threading::parallel_for(selection.index_range(), 4096, [&](const IndexRange range) {
    for (const int64_t curve_i : selection.slice(range)) {
      const IndexRange src_points = src_curves.points_for_curve(curve_i);
      const IndexRange dst_points = dst_curves.points_for_curve(curve_i);

      sample_bezier(src_positions.slice(src_points),
                    src_handles_l.slice(src_points),
                    src_handles_r.slice(src_points),
                    src_types_l.slice(src_points),
                    src_types_r.slice(src_points),
                    dst_positions,
                    dst_handles_l,
                    dst_handles_r,
                    dst_types_l,
                    dst_types_r,
                    dst_points,
                    sample_points[curve_i]);

      for (bke::AttributeTransferData &attribute : transfer_attributes) {
        attribute_math::convert_to_static_type(attribute.meta_data.data_type, [&](auto dummy) {
          using T = decltype(dummy);
          sample_linear<T>(attribute.src.template typed<T>().slice(src_points),
                           attribute.dst.span.typed<T>(),
                           dst_points,
                           sample_points[curve_i]);
        });
      }
    }
  });
  fill_nurbs_data(dst_curves, selection);
}

static void convert_point_evaluated_curves(
    const bke::CurvesGeometry &src_curves,
    bke::CurvesGeometry &dst_curves,
    const IndexMask selection,
    const Span<bke::curves::CurvePoint> evaluated_sample_points,
    MutableSpan<bke::AttributeTransferData> transfer_attributes)
{
  const Span<float3> src_eval_positions = src_curves.evaluated_positions();
  MutableSpan<float3> dst_positions = dst_curves.positions_for_write();

  threading::parallel_for(selection.index_range(), 4096, [&](const IndexRange range) {
    for (const int64_t curve_i : selection.slice(range)) {
      const IndexRange dst_points = dst_curves.points_for_curve(curve_i);
      const IndexRange src_evaluated_points = src_curves.evaluated_points_for_curve(curve_i);

      sample_linear<float3>(src_eval_positions.slice(src_evaluated_points),
                            dst_positions,
                            dst_points,
                            evaluated_sample_points[curve_i]);

      for (bke::AttributeTransferData &attribute : transfer_attributes) {
        attribute_math::convert_to_static_type(attribute.meta_data.data_type, [&](auto dummy) {
          using T = decltype(dummy);
          GArray evaluated_data(CPPType::get<T>(), src_evaluated_points.size());
          GMutableSpan evaluated_span = evaluated_data.as_mutable_span();
          src_curves.interpolate_to_evaluated(
              curve_i, attribute.src.slice(src_curves.points_for_curve(curve_i)), evaluated_span);
          sample_linear<T>(evaluated_span.typed<T>(),
                           attribute.dst.span.typed<T>(),
                           dst_points,
                           evaluated_sample_points[curve_i]);
        });
      }
    }
  });
  fill_bezier_data(dst_curves, selection);
  fill_nurbs_data(dst_curves, selection);
}

/* --------------------------------------------------------------------
 * Trim curves.
 */

static void trim_attribute_linear(const bke::CurvesGeometry &src_curves,
                                  bke::CurvesGeometry &dst_curves,
                                  const IndexMask selection,
                                  const Span<bke::curves::CurvePoint> start_points,
                                  const Span<bke::curves::CurvePoint> end_points,
                                  MutableSpan<bke::AttributeTransferData> transfer_attributes)
{
  for (bke::AttributeTransferData &attribute : transfer_attributes) {
    attribute_math::convert_to_static_type(attribute.meta_data.data_type, [&](auto dummy) {
      using T = decltype(dummy);

      threading::parallel_for(selection.index_range(), 512, [&](const IndexRange range) {
        for (const int64_t curve_i : selection.slice(range)) {
          const IndexRange src_points = src_curves.points_for_curve(curve_i);

          bke::curves::IndexRangeCyclic src_sample_range = get_range_between_endpoints(
              start_points[curve_i], end_points[curve_i], {0, src_points.size()});
          sample_interval_linear<T>(attribute.src.template typed<T>().slice(src_points),
                                    attribute.dst.span.typed<T>(),
                                    src_sample_range,
                                    dst_curves.points_for_curve(curve_i),
                                    start_points[curve_i],
                                    end_points[curve_i]);
        }
      });
    });
  }
}

static void trim_polygonal_curves(const bke::CurvesGeometry &src_curves,
                                  bke::CurvesGeometry &dst_curves,
                                  const IndexMask selection,
                                  const Span<bke::curves::CurvePoint> start_points,
                                  const Span<bke::curves::CurvePoint> end_points,
                                  MutableSpan<bke::AttributeTransferData> transfer_attributes)
{
  const Span<float3> src_positions = src_curves.positions();
  MutableSpan<float3> dst_positions = dst_curves.positions_for_write();

  threading::parallel_for(selection.index_range(), 512, [&](const IndexRange range) {
    for (const int64_t curve_i : selection.slice(range)) {
      const IndexRange src_points = src_curves.points_for_curve(curve_i);
      const IndexRange dst_points = dst_curves.points_for_curve(curve_i);

      bke::curves::IndexRangeCyclic src_sample_range = get_range_between_endpoints(
          start_points[curve_i], end_points[curve_i], {0, src_points.size()});
      sample_interval_linear<float3>(src_positions.slice(src_points),
                                     dst_positions,
                                     src_sample_range,
                                     dst_points,
                                     start_points[curve_i],
                                     end_points[curve_i]);
    }
  });
  fill_bezier_data(dst_curves, selection);
  fill_nurbs_data(dst_curves, selection);
  trim_attribute_linear(
      src_curves, dst_curves, selection, start_points, end_points, transfer_attributes);
}

static void trim_catmull_rom_curves(const bke::CurvesGeometry &src_curves,
                                    bke::CurvesGeometry &dst_curves,
                                    const IndexMask selection,
                                    const Span<bke::curves::CurvePoint> start_points,
                                    const Span<bke::curves::CurvePoint> end_points,
                                    MutableSpan<bke::AttributeTransferData> transfer_attributes)
{
  const Span<float3> src_positions = src_curves.positions();
  const VArray<bool> src_cyclic = src_curves.cyclic();
  MutableSpan<float3> dst_positions = dst_curves.positions_for_write();

  threading::parallel_for(selection.index_range(), 512, [&](const IndexRange range) {
    for (const int64_t curve_i : selection.slice(range)) {
      const IndexRange src_points = src_curves.points_for_curve(curve_i);
      const IndexRange dst_points = dst_curves.points_for_curve(curve_i);

      bke::curves::IndexRangeCyclic src_sample_range = get_range_between_endpoints(
          start_points[curve_i], end_points[curve_i], {0, src_points.size()});
      sample_interval_catmull_rom<float3>(src_positions.slice(src_points),
                                          dst_positions,
                                          src_sample_range,
                                          dst_points,
                                          start_points[curve_i],
                                          end_points[curve_i],
                                          src_cyclic[curve_i]);
    }
  });
  fill_bezier_data(dst_curves, selection);
  fill_nurbs_data(dst_curves, selection);

  for (bke::AttributeTransferData &attribute : transfer_attributes) {
    attribute_math::convert_to_static_type(attribute.meta_data.data_type, [&](auto dummy) {
      using T = decltype(dummy);

      threading::parallel_for(selection.index_range(), 512, [&](const IndexRange range) {
        for (const int64_t curve_i : selection.slice(range)) {
          const IndexRange src_points = src_curves.points_for_curve(curve_i);
          const IndexRange dst_points = dst_curves.points_for_curve(curve_i);

          bke::curves::IndexRangeCyclic src_sample_range = get_range_between_endpoints(
              start_points[curve_i], end_points[curve_i], {0, src_points.size()});
          sample_interval_catmull_rom<T>(attribute.src.template typed<T>().slice(src_points),
                                         attribute.dst.span.typed<T>(),
                                         src_sample_range,
                                         dst_points,
                                         start_points[curve_i],
                                         end_points[curve_i],
                                         src_cyclic[curve_i]);
        }
      });
    });
  }
}

static void trim_bezier_curves(const bke::CurvesGeometry &src_curves,
                               bke::CurvesGeometry &dst_curves,
                               const IndexMask selection,
                               const Span<bke::curves::CurvePoint> start_points,
                               const Span<bke::curves::CurvePoint> end_points,
                               MutableSpan<bke::AttributeTransferData> transfer_attributes)
{
  const Span<float3> src_positions = src_curves.positions();
  const VArraySpan<int8_t> src_types_l{src_curves.handle_types_left()};
  const VArraySpan<int8_t> src_types_r{src_curves.handle_types_right()};
  const Span<float3> src_handles_l = src_curves.handle_positions_left();
  const Span<float3> src_handles_r = src_curves.handle_positions_right();

  MutableSpan<float3> dst_positions = dst_curves.positions_for_write();
  MutableSpan<int8_t> dst_types_l = dst_curves.handle_types_left_for_write();
  MutableSpan<int8_t> dst_types_r = dst_curves.handle_types_right_for_write();
  MutableSpan<float3> dst_handles_l = dst_curves.handle_positions_left_for_write();
  MutableSpan<float3> dst_handles_r = dst_curves.handle_positions_right_for_write();

  threading::parallel_for(selection.index_range(), 512, [&](const IndexRange range) {
    for (const int64_t curve_i : selection.slice(range)) {
      const IndexRange src_points = src_curves.points_for_curve(curve_i);
      const IndexRange dst_points = dst_curves.points_for_curve(curve_i);

      bke::curves::IndexRangeCyclic src_sample_range = get_range_between_endpoints(
          start_points[curve_i], end_points[curve_i], {0, src_points.size()});
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
                             src_sample_range,
                             dst_points,
                             start_points[curve_i],
                             end_points[curve_i]);
    }
  });
  fill_nurbs_data(dst_curves, selection);
  trim_attribute_linear(
      src_curves, dst_curves, selection, start_points, end_points, transfer_attributes);
}

static void trim_evaluated_curves(const bke::CurvesGeometry &src_curves,
                                  bke::CurvesGeometry &dst_curves,
                                  const IndexMask selection,
                                  const Span<bke::curves::CurvePoint> start_points,
                                  const Span<bke::curves::CurvePoint> end_points,
                                  MutableSpan<bke::AttributeTransferData> transfer_attributes)
{
  const Span<float3> src_eval_positions = src_curves.evaluated_positions();
  MutableSpan<float3> dst_positions = dst_curves.positions_for_write();

  threading::parallel_for(selection.index_range(), 512, [&](const IndexRange range) {
    for (const int64_t curve_i : selection.slice(range)) {
      const IndexRange dst_points = dst_curves.points_for_curve(curve_i);
      const IndexRange src_evaluated_points = src_curves.evaluated_points_for_curve(curve_i);

      bke::curves::IndexRangeCyclic src_sample_range = get_range_between_endpoints(
          start_points[curve_i], end_points[curve_i], {0, src_evaluated_points.size()});
      sample_interval_linear<float3>(src_eval_positions.slice(src_evaluated_points),
                                     dst_positions,
                                     src_sample_range,
                                     dst_points,
                                     start_points[curve_i],
                                     end_points[curve_i]);
    }
  });
  fill_bezier_data(dst_curves, selection);
  fill_nurbs_data(dst_curves, selection);

  for (bke::AttributeTransferData &attribute : transfer_attributes) {
    attribute_math::convert_to_static_type(attribute.meta_data.data_type, [&](auto dummy) {
      using T = decltype(dummy);

      threading::parallel_for(selection.index_range(), 512, [&](const IndexRange range) {
        for (const int64_t curve_i : selection.slice(range)) {
          /* Interpolate onto the evaluated point domain and sample the evaluated domain. */
          const IndexRange src_evaluated_points = src_curves.evaluated_points_for_curve(curve_i);
          GArray evaluated_data(CPPType::get<T>(), src_evaluated_points.size());
          GMutableSpan evaluated_span = evaluated_data.as_mutable_span();
          src_curves.interpolate_to_evaluated(
              curve_i, attribute.src.slice(src_curves.points_for_curve(curve_i)), evaluated_span);
          bke::curves::IndexRangeCyclic src_sample_range = get_range_between_endpoints(
              start_points[curve_i], end_points[curve_i], {0, src_evaluated_points.size()});
          sample_interval_linear<T>(evaluated_span.typed<T>(),
                                    attribute.dst.span.typed<T>(),
                                    src_sample_range,
                                    dst_curves.points_for_curve(curve_i),
                                    start_points[curve_i],
                                    end_points[curve_i]);
        }
      });
    });
  }
}

bke::CurvesGeometry trim_curves(const bke::CurvesGeometry &src_curves,
                                const IndexMask selection,
                                const Span<bke::curves::CurvePoint> start_points,
                                const Span<bke::curves::CurvePoint> end_points)
{
  BLI_assert(selection.size() > 0);
  BLI_assert(selection.last() <= start_points.size());
  BLI_assert(start_points.size() == end_points.size());

  src_curves.ensure_evaluated_offsets();
  Vector<int64_t> inverse_selection_indices;
  const IndexMask inverse_selection = selection.invert(src_curves.curves_range(),
                                                       inverse_selection_indices);

  /* Create trim curves. */
  bke::CurvesGeometry dst_curves(0, src_curves.curves_num());
  determine_copyable_curve_types(src_curves,
                                 dst_curves,
                                 selection,
                                 inverse_selection,
                                 (CurveTypeMask)(CURVE_TYPE_MASK_CATMULL_ROM |
                                                 CURVE_TYPE_MASK_POLY | CURVE_TYPE_MASK_BEZIER));

  Vector<int64_t> curve_indices;
  Vector<int64_t> point_curve_indices;
  compute_trim_result_offsets(src_curves,
                              selection,
                              inverse_selection,
                              start_points,
                              end_points,
                              dst_curves.curve_types(),
                              dst_curves.offsets_for_write(),
                              curve_indices,
                              point_curve_indices);
  /* Finalize by updating the geometry container. */
  dst_curves.resize(dst_curves.offsets().last(), dst_curves.curves_num());
  dst_curves.update_curve_types();

  /* Populate curve domain. */
  const bke::AttributeAccessor src_attributes = src_curves.attributes();
  bke::MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();
  bke::copy_attribute_domain(src_attributes,
                             dst_attributes,
                             selection,
                             ATTR_DOMAIN_CURVE,
                             {"cyclic", "curve_type", "nurbs_order", "knots_mode"});

  /* Fetch custom point domain attributes for transfer (copy). */
  Vector<bke::AttributeTransferData> transfer_attributes = bke::retrieve_attributes_for_transfer(
      src_attributes,
      dst_attributes,
      ATTR_DOMAIN_MASK_POINT,
      {"position",
       "handle_left",
       "handle_right",
       "handle_type_left",
       "handle_type_right",
       "nurbs_weight"});

  auto trim_catmull = [&](IndexMask selection) {
    trim_catmull_rom_curves(
        src_curves, dst_curves, selection, start_points, end_points, transfer_attributes);
  };
  auto trim_poly = [&](IndexMask selection) {
    trim_polygonal_curves(
        src_curves, dst_curves, selection, start_points, end_points, transfer_attributes);
  };
  auto trim_bezier = [&](IndexMask selection) {
    trim_bezier_curves(
        src_curves, dst_curves, selection, start_points, end_points, transfer_attributes);
  };
  auto trim_evaluated = [&](IndexMask selection) {
    /* Ensure evaluated positions are available. */
    src_curves.ensure_evaluated_offsets();
    src_curves.evaluated_positions();
    trim_evaluated_curves(
        src_curves, dst_curves, selection, start_points, end_points, transfer_attributes);
  };

  auto single_point_catmull = [&](IndexMask selection) {
    convert_point_catmull_curves(
        src_curves, dst_curves, selection, start_points, transfer_attributes);
  };
  auto single_point_poly = [&](IndexMask selection) {
    convert_point_polygonal_curves(
        src_curves, dst_curves, selection, start_points, transfer_attributes);
  };
  auto single_point_bezier = [&](IndexMask selection) {
    convert_point_bezier_curves(
        src_curves, dst_curves, selection, start_points, transfer_attributes);
  };
  auto single_point_evaluated = [&](IndexMask selection) {
    convert_point_evaluated_curves(
        src_curves, dst_curves, selection, start_points, transfer_attributes);
  };

  /* Populate point domain. */
  bke::curves::foreach_curve_by_type(src_curves.curve_types(),
                                     src_curves.curve_type_counts(),
                                     curve_indices.as_span(),
                                     trim_catmull,
                                     trim_poly,
                                     trim_bezier,
                                     trim_evaluated);

  if (point_curve_indices.size()) {
    bke::curves::foreach_curve_by_type(src_curves.curve_types(),
                                       src_curves.curve_type_counts(),
                                       point_curve_indices.as_span(),
                                       single_point_catmull,
                                       single_point_poly,
                                       single_point_bezier,
                                       single_point_evaluated);
  }
  /* Cleanup/close context */
  for (bke::AttributeTransferData &attribute : transfer_attributes) {
    attribute.dst.finish();
  }

  /* Copy unselected */
  if (!inverse_selection.is_empty()) {
    bke::copy_attribute_domain(
        src_attributes, dst_attributes, inverse_selection, ATTR_DOMAIN_CURVE);
    /* Trim curves are no longer cyclic. If all curves are trimmed, this will be set implicitly. */
    dst_curves.cyclic_for_write().fill_indices(selection, false);

    /* Copy point domain. */
    for (auto &attribute : bke::retrieve_attributes_for_transfer(
             src_attributes, dst_attributes, ATTR_DOMAIN_MASK_POINT)) {
      bke::curves::copy_point_data(
          src_curves, dst_curves, inverse_selection, attribute.src, attribute.dst.span);
      attribute.dst.finish();
    }
  }

  dst_curves.tag_topology_changed();
  return dst_curves;
}

}  // namespace blender::geometry
