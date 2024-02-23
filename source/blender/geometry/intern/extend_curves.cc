/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BLI_array_utils.hh"
#include "BLI_length_parameterize.hh"
#include "BLI_math_axis_angle.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_quaternion.hh"
#include "BLI_math_rotation.hh"
#include "BLI_math_vector.hh"

#include "BKE_attribute.hh"
#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"
#include "BKE_curves_utils.hh"
#include "BKE_geometry_set.hh"

#include "GEO_extend_curves.hh"

namespace blender::geometry {

static void extend_curves_straight(const float used_percent_length,
                                   const float new_size,
                                   const Span<int> start_points,
                                   const Span<int> end_points,
                                   const int curve,
                                   const IndexRange new_curve,
                                   const Span<float> use_start_lengths,
                                   const Span<float> use_end_lengths,
                                   MutableSpan<float3> positions)
{
  float overshoot_point_param = used_percent_length * (new_size - 1);
  if (start_points[curve]) {
    /** Here we use the vector between two adjacent points around #overshoot_point_param as
     * our reference for the direction of extension, however to have better tolerance for jitter,
     * using the vector (a_few_points_back - end_point) might be a better solution in the future.
     */
    int index1 = math::floor(overshoot_point_param);
    int index2 = math::ceil(overshoot_point_param);

    /* When #overshoot_point_param is zero */
    if (index2 == 0) {
      index2 = 1;
    }
    float3 result = math::interpolate(positions[new_curve[index1]],
                                      positions[new_curve[index2]],
                                      fmodf(overshoot_point_param, 1.0f));
    result -= positions[new_curve.first()];
    if (UNLIKELY(math::is_zero(result))) {
      result = positions[new_curve[1]] - positions[new_curve[0]];
    }
    positions[new_curve[0]] += result * (-use_start_lengths[curve] / math::length(result));
  }

  if (end_points[curve]) {
    int index1 = new_size - 1 - math::floor(overshoot_point_param);
    int index2 = new_size - 1 - math::ceil(overshoot_point_param);
    float3 result = math::interpolate(positions[new_curve[index1]],
                                      positions[new_curve[index2]],
                                      fmodf(overshoot_point_param, 1.0f));
    result -= positions[new_curve.last()];
    if (UNLIKELY(math::is_zero(result))) {
      result = positions[new_curve[new_size - 2]] - positions[new_curve[new_size - 1]];
    }
    positions[new_curve[new_size - 1]] += result *
                                          (-use_end_lengths[curve] / math::length(result));
  }
}

static void extend_curves_curved(const float used_percent_length,
                                 const Span<int> start_points,
                                 const Span<int> end_points,
                                 const OffsetIndices<int> points_by_curve,
                                 const int curve,
                                 const IndexRange new_curve,
                                 const Span<float> use_start_lengths,
                                 const Span<float> use_end_lengths,
                                 const float max_angle,
                                 const float segment_influence,
                                 const bool invert_curvature,
                                 MutableSpan<float3> positions)
{
  /* Curvature calculation. */
  const int first_old_index = start_points[curve] ? start_points[curve] : 0;
  const int last_old_index = points_by_curve[curve].size() - 1 + first_old_index;
  const int orig_totpoints = points_by_curve[curve].size();

  /* The fractional amount of points to query when calculating the average curvature of the
   * strokes. */
  const float overshoot_parameter = used_percent_length * (orig_totpoints - 2);
  int overshoot_pointcount = math::ceil(overshoot_parameter);
  overshoot_pointcount = math::clamp(overshoot_pointcount, 1, orig_totpoints - 2);

  /* Do for both sides without code duplication. */
  float3 vec1, total_angle;
  for (int k = 0; k < 2; k++) {
    if ((k == 0 && !start_points[curve]) || (k == 1 && !end_points[curve])) {
      continue;
    }

    const int start_i = k == 0 ? first_old_index : last_old_index;
    const int dir_i = 1 - k * 2;

    vec1 = positions[new_curve[start_i + dir_i]] - positions[new_curve[start_i]];
    total_angle = float3({0, 0, 0});

    float segment_length;
    vec1 = math::normalize_and_get_length(vec1, segment_length);

    float overshoot_length = 0.0f;

    /* Accumulate rotation angle and length. */
    int j = 0;
    float3 no, vec2;
    for (int i = start_i; j < overshoot_pointcount; i += dir_i, j++) {
      /* Don't fully add last segment to get continuity in overshoot_fac. */
      float fac = math::min(overshoot_parameter - j, 1.0f);

      /* Read segments. */
      vec2 = vec1;
      vec1 = positions[new_curve[i + dir_i * 2]] - positions[new_curve[i + dir_i]];

      float len;
      vec1 = math::normalize_and_get_length(vec1, len);
      float angle = math::angle_between(vec1, vec2).radian() * fac;

      /* Add half of both adjacent legs of the current angle. */
      const float added_len = (segment_length + len) * 0.5f * fac;
      overshoot_length += added_len;
      segment_length = len;

      if (angle > max_angle) {
        continue;
      }
      if (angle > M_PI * 0.995f) {
        continue;
      }

      angle *= math::pow(added_len, segment_influence);

      no = math::cross(vec1, vec2);
      no = math::normalize(no) * angle;
      total_angle += no;
    }

    if (UNLIKELY(overshoot_length == 0.0f)) {
      /* Don't do a proper extension if the used points are all in the same position. */
      continue;
    }

    vec1 = positions[new_curve[start_i]] - positions[new_curve[start_i + dir_i]];
    /* In general curvature = 1/radius. For the case without the
     * weights introduced by #segment_influence, the calculation is:
     * `curvature = delta angle/delta arclength = len_v3(total_angle) / overshoot_length` */
    float curvature = normalize_v3(total_angle) / overshoot_length;
    /* Compensate for the weights powf(added_len, segment_influence). */
    curvature /= math::pow(overshoot_length / math::min(overshoot_parameter, float(j)),
                           segment_influence);
    if (invert_curvature) {
      curvature = -curvature;
    }
    const float dist = k == 0 ? use_start_lengths[curve] : use_end_lengths[curve];
    const int extra_point_count = k == 0 ? start_points[curve] : end_points[curve];
    const float angle_step = curvature * dist / extra_point_count;
    float step_length = dist / extra_point_count;
    if (math::abs(angle_step) > FLT_EPSILON) {
      /* Make a direct step length from the assigned arc step length. */
      step_length *= sin(angle_step * 0.5f) / (angle_step * 0.5f);
    }
    else {
      total_angle = float3({0, 0, 0});
    }
    float prev_length;
    vec1 = math::normalize_and_get_length(vec1, prev_length);
    vec1 *= step_length;

    /* Build rotation matrix here to get best performance. */
    math::AxisAngle axis_base(total_angle, angle_step);
    math::Quaternion q = math::to_quaternion(axis_base);
    float3x3 rot = math::from_rotation<float3x3>(q);

    /* Rotate the starting direction to account for change in edge lengths. */
    math::AxisAngle step_base(total_angle,
                              math::max(0.0f, 1.0f - math::abs(segment_influence)) *
                                  (curvature * prev_length - angle_step) / 2.0f);
    q = math::to_quaternion(step_base);
    vec1 = math::transform_point(q, vec1);

    /* Now iteratively accumulate the segments with a rotating added direction. */
    for (int i = start_i - dir_i, j = 0; j < extra_point_count; i -= dir_i, j++) {
      vec1 = rot * vec1;
      positions[new_curve[i]] = vec1 + positions[new_curve[i + dir_i]];
    }
  }
}

bke::CurvesGeometry extend_curves(bke::CurvesGeometry &src_curves,
                                  const IndexMask &selection,
                                  const VArray<float> &start_lengths,
                                  const VArray<float> &end_lengths,
                                  const float overshoot_fac,
                                  const bool follow_curvature,
                                  const float point_density,
                                  const float segment_influence,
                                  const float max_angle,
                                  const bool invert_curvature,
                                  const GeometryNodeCurveSampleMode sample_mode,
                                  const bke::AnonymousAttributePropagationInfo &propagation_info)
{
  if (src_curves.points_num() < 2) {
    return src_curves;
  }

  const int src_curves_num = src_curves.curves_num();
  Array<int> start_points(src_curves_num);
  Array<int> end_points(src_curves_num);
  Array<float> use_start_lengths(src_curves_num);
  Array<float> use_end_lengths(src_curves_num);

  const OffsetIndices<int> points_by_curve = src_curves.points_by_curve();

  src_curves.ensure_evaluated_lengths();
  selection.foreach_index([&](const int curve) {
    use_start_lengths[curve] = start_lengths[curve];
    use_end_lengths[curve] = end_lengths[curve];
    if (sample_mode == GEO_NODE_CURVE_SAMPLE_FACTOR) {
      float total_length = src_curves.evaluated_length_total_for_curve(curve, false);
      use_start_lengths[curve] *= total_length;
      use_end_lengths[curve] *= total_length;
      start_points[curve] = 1;
      end_points[curve] = 1;
    }
  });

  bke::CurvesGeometry dst_curves;

  /* Use the old curves when extending straight when no new points are added.  */
  if (!follow_curvature) {
    dst_curves = std::move(src_curves);
  }
  else {
    /* Copy only curves domain since we are not changing the number of curves here. */
    dst_curves = bke::curves::copy_only_curve_domain(src_curves);
    /* Count how many points we need. */
    MutableSpan<int> dst_points_by_curve = dst_curves.offsets_for_write();
    selection.foreach_index([&](const int curve) {
      int point_count = points_by_curve[curve].size();
      dst_points_by_curve[curve] = point_count;
      /* Curve not suitable for stretching... */
      if (point_count <= 2) {
        return;
      }

      const int count_start = (use_start_lengths[curve] > 0) ?
                                  (math::ceil(use_start_lengths[curve] * point_density)) :
                                  0;
      const int count_end = (use_end_lengths[curve] > 0) ?
                                (math::ceil(use_end_lengths[curve] * point_density)) :
                                0;
      dst_points_by_curve[curve] += count_start;
      dst_points_by_curve[curve] += count_end;
      start_points[curve] = count_start;
      end_points[curve] = count_end;
    });

    OffsetIndices dst_indices = offset_indices::accumulate_counts_to_offsets(dst_points_by_curve);
    int target_point_count = dst_points_by_curve.last();

    /* Make destination to source map for points. */
    Array<int> dst_to_src_point(target_point_count);
    for (const int curve : src_curves.curves_range()) {
      const int point_count = points_by_curve[curve].size();
      int local_front = 0;
      MutableSpan<int> new_points_by_curve = dst_to_src_point.as_mutable_span().slice(
          dst_indices[curve]);
      if (start_points[curve]) {
        MutableSpan<int> starts = new_points_by_curve.slice(0, start_points[curve]);
        starts.fill(points_by_curve[curve].first());
        local_front = start_points[curve];
      }
      if (end_points[curve]) {
        MutableSpan<int> ends = new_points_by_curve.slice(
            new_points_by_curve.size() - end_points[curve], end_points[curve]);
        ends.fill(points_by_curve[curve].last());
      }
      MutableSpan<int> original_points = new_points_by_curve.slice(local_front, point_count);
      for (const int point_i : original_points.index_range()) {
        original_points[point_i] = points_by_curve[curve][point_i];
      }
    }

    dst_curves.resize(target_point_count, src_curves_num);

    const bke::AttributeAccessor src_attributes = src_curves.attributes();
    bke::MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();

    /* Transfer point attributes. */
    gather_attributes(src_attributes,
                      bke::AttrDomain::Point,
                      propagation_info,
                      {},
                      dst_to_src_point,
                      dst_attributes);
  }

  MutableSpan<float3> positions = dst_curves.positions_for_write();

  const OffsetIndices<int> new_points_by_curve = dst_curves.points_by_curve();
  threading::parallel_for(dst_curves.curves_range(), 512, [&](const IndexRange curves_range) {
    for (const int curve : curves_range) {
      if (!start_points[curve] && !end_points[curve]) {
        /* Curves should not be touched if they didn't generate extra points before. */
        return;
      }
      const IndexRange new_curve = new_points_by_curve[curve];
      int new_size = new_curve.size();

      /* #used_percent_length must always be finite and non-zero. */
      const float used_percent_length = math::clamp(
          isfinite(overshoot_fac) ? overshoot_fac : 0.1f, 1e-4f, 1.0f);

      if (!follow_curvature) {
        extend_curves_straight(used_percent_length,
                               new_size,
                               start_points.as_span(),
                               end_points.as_span(),
                               curve,
                               new_curve,
                               use_start_lengths.as_span(),
                               use_end_lengths.as_span(),
                               positions);
      }
      else {
        extend_curves_curved(used_percent_length,
                             start_points.as_span(),
                             end_points.as_span(),
                             points_by_curve,
                             curve,
                             new_curve,
                             use_start_lengths.as_span(),
                             use_end_lengths.as_span(),
                             max_angle,
                             segment_influence,
                             invert_curvature,
                             positions);
      }
    }
  });

  return dst_curves;
}

}  // namespace blender::geometry
