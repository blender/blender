/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include <algorithm>
#include <functional>

#include "BLI_array.hh"
#include "BLI_array_utils.hh"
#include "BLI_bounds.hh"
#include "BLI_lasso_2d.hh"
#include "BLI_math_base.hh"
#include "BLI_math_geom.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_offset_indices.hh"
#include "BLI_sort.hh"
#include "BLI_task.hh"
#include "BLI_vector.hh"

#include "BKE_curves.hh"
#include "BKE_grease_pencil_fills.hh"

#include "ED_grease_pencil.hh"
#include "ED_view3d.hh"

#include "grease_pencil_segments_intern.hh"

namespace blender::ed::greasepencil::segment {

bool Segment::is_loop() const
{
  return full_wrap_loop;
}

bool Segment::has_intersection(const Side side) const
{
  return intersection_index[side] != -1;
}

int2 Segment::edge(const Side side) const
{
  return int2(points[side], this->wrap_index(points[side] + 1));
}

int Segment::wrap_index(const int i) const
{
  return math::mod_periodic(i - src_points.first(), src_points.size()) + src_points.first();
}

IndexRange Segment::point_range() const
{
  if (this->is_loop()) {
    return src_points;
  }

  if (!this->has_intersection(Side::Start) && this->has_intersection(Side::End)) {
    return IndexRange::from_begin_end_inclusive(src_points.first(), points[Side::End]);
  }

  if (!this->has_intersection(Side::Start) && !this->has_intersection(Side::End)) {
    return src_points;
  }

  /* If both intersection points are on the same edge, there's ether no points between or
   * all of the points are. */
  if (points[Side::Start] == points[Side::End]) {

    /* If both intersections points are the same, either the segment as nothing or the full
     * range. */
    if (intersection_factor[Side::Start] == intersection_factor[Side::End]) {
      if (this->is_loop()) {
        return src_points.shift(points[Side::Start] - src_points.first() + 1);
      }
      return IndexRange(0);
    }

    if (intersection_factor[Side::Start] > intersection_factor[Side::End]) {
      return src_points.shift(points[Side::Start] - src_points.first() + 1);
    }
    return IndexRange(0);
  }

  if (points[Side::Start] > points[Side::End]) {
    return IndexRange::from_begin_end_inclusive(points[Side::Start] + 1,
                                                points[Side::End] + src_points.size());
  }

  return IndexRange::from_begin_end_inclusive(points[Side::Start] + 1, points[Side::End]);
}

int Segment::points_num() const
{
  return this->point_range().size();
}

Segment Segment::from_curve(const int curve_i, const IndexRange points, const bool cyclic)
{
  Segment segment;
  segment.curve = curve_i;
  segment.src_points = points;

  segment.points[Side::Start] = points.first();
  segment.points[Side::End] = points.last();

  segment.intersection_factor[Side::Start] = 0.0f;
  segment.intersection_factor[Side::End] = cyclic ? 1.0f : 0.0f;

  segment.full_wrap_loop = cyclic;

  return segment;
}

Segment Segment::from_intersections(const int curve_i,
                                    const IndexRange points,
                                    const std::optional<int> point_start,
                                    const std::optional<int> point_end,
                                    const std::optional<float> factor_start,
                                    const std::optional<float> factor_end,
                                    const std::optional<int> inter_index_start,
                                    const std::optional<int> inter_index_end)
{
  Segment segment;
  segment.curve = curve_i;
  segment.src_points = points;

  if (point_start) {
    segment.points[Side::Start] = *point_start;
    segment.intersection_factor[Side::Start] = *factor_start;
  }
  else {
    segment.points[Side::Start] = points.first();
    segment.intersection_factor[Side::Start] = 0.0f;
  }

  if (inter_index_start) {
    segment.intersection_index[Side::Start] = *inter_index_start;
  }

  if (point_end) {
    segment.points[Side::End] = *point_end;
    segment.intersection_factor[Side::End] = *factor_end;
  }
  else {
    segment.points[Side::End] = points.last();
    segment.intersection_factor[Side::End] = 0.0f;
  }

  if (inter_index_end) {
    segment.intersection_index[Side::End] = *inter_index_end;
  }

  BLI_assert(points.contains(segment.points[Side::Start]));
  BLI_assert(points.contains(segment.points[Side::End]));

  return segment;
}

/**
 * Get the intersection distance of two line segments a-b and c-d.
 * The intersection distance is defined as the normalized distance (0..1)
 * from point a to the intersection point of a-b and c-d.
 */
static float get_intersection_distance_of_segments(const float2 &co_a,
                                                   const float2 &co_b,
                                                   const float2 &co_c,
                                                   const float2 &co_d)
{
  /* Get intersection point. */
  const float a1 = co_b[1] - co_a[1];
  const float b1 = co_a[0] - co_b[0];
  const float c1 = a1 * co_a[0] + b1 * co_a[1];

  const float a2 = co_d[1] - co_c[1];
  const float b2 = co_c[0] - co_d[0];
  const float c2 = a2 * co_c[0] + b2 * co_c[1];

  const float det = (a1 * b2 - a2 * b1);
  if (det == 0.0f) {
    return 0.0f;
  }

  const float2 isect((b2 * c1 - b1 * c2) / det, (a1 * c2 - a2 * c1) / det);

  /* Get normalized distance from point a to intersection point. */
  const float length_ab = math::distance(co_b, co_a);
  const float distance = math::safe_divide(math::distance(isect, co_a), length_ab);

  /* Snap to the ends if very close. */
  if (math::abs(distance) < 0.0001f) {
    return 0.0f;
  }
  if (math::abs(distance - 1.0f) < 0.0001f) {
    return 1.0f;
  }

  return distance;
}

IntersectionPoint create_intersection(const int point_i,
                                      const int point_j,
                                      const float factor_i,
                                      const float factor_j,
                                      const int curve_i,
                                      const int curve_j)
{
  IntersectionPoint inter_point;
  inter_point.point_i = point_i;
  inter_point.point_j = point_j;
  inter_point.factor_i = factor_i;
  inter_point.factor_j = factor_j;
  inter_point.curve_i = curve_i;
  inter_point.curve_j = curve_j;

  return inter_point;
}

static void find_intersections_between_curve_and_curves(
    const Span<float2> screen_space_positions,
    const Span<Bounds<float2>> screen_space_bbox,
    const OffsetIndices<int> points_by_curve,
    const VArray<bool> &cyclic,
    const IndexMask &visible_curves,
    const int curve_i,
    Array<Vector<int>> &r_inters_per_curves,
    Vector<IntersectionPoint> &r_intersections)
{
  const bool cyclic_i = cyclic[curve_i];
  const IndexRange curve_points_i = points_by_curve[curve_i];

  for (const int i : curve_points_i.index_range().drop_back(cyclic_i ? 0 : 1)) {
    const int point_i1 = curve_points_i[i];
    const int point_i2 = curve_points_i[(i + 1) % curve_points_i.size()];

    const float2 co_i1 = screen_space_positions[point_i1];
    const float2 co_i2 = screen_space_positions[point_i2];

    Bounds<float2> bbox_i{math::min(co_i1, co_i2), math::max(co_i1, co_i2)};
    bbox_i.pad(BBOX_PADDING);

    /* Add some padding to the line segment i1-i2, otherwise we could just miss an
     * intersection. */
    const float2 padding_i = math::normalize(co_i2 - co_i1);
    const float2 padded_i1 = co_i1 - padding_i;
    const float2 padded_i2 = co_i2 + padding_i;

    visible_curves.foreach_index([&](const int curve_j) {
      /* Because intersecting the curves i with j and j with i, we skip one half to avoid
       * duplicating all the points. */
      if (curve_i > curve_j) {
        return;
      }

      /* Bounding box check: Skip curves that don't overlap segment i1-i2. */
      if (!bounds::intersect(bbox_i, screen_space_bbox[curve_j]).has_value()) {
        return;
      }

      const bool cyclic_j = cyclic[curve_j];
      const IndexRange curve_points_j = points_by_curve[curve_j];

      for (const int j : curve_points_j.index_range().drop_back(cyclic_j ? 0 : 1)) {
        const int point_j1 = curve_points_j[j];
        const int point_j2 = curve_points_j[(j + 1) % curve_points_j.size()];

        /* Don't self check. */
        if (curve_i == curve_j && (point_i1 == point_j1 || point_i1 == point_j2 ||
                                   point_i2 == point_j1 || point_i2 == point_j2))
        {
          continue;
        }

        const float2 co_j1 = screen_space_positions[point_j1];
        const float2 co_j2 = screen_space_positions[point_j2];

        Bounds<float2> bbox_j{math::min(co_j1, co_j2), math::max(co_j1, co_j2)};
        bbox_j.pad(BBOX_PADDING);

        /* Skip when bounding boxes of i1-i2 and j1-j2 don't overlap. */
        if (!bounds::intersect(bbox_i, bbox_j).has_value()) {
          continue;
        }

        /* Add some padding to the line segment j1-j2, otherwise we could just miss an
         * intersection. */
        const float2 padding_j = math::normalize(co_j2 - co_j1);
        const float2 padded_j1 = co_j1 - padding_j;
        const float2 padded_j2 = co_j2 + padding_j;

        /* Check for intersection. */
        const auto isect = math::isect_seg_seg(padded_i1, padded_i2, padded_j1, padded_j2);
        if (ELEM(isect.kind, isect.LINE_LINE_CROSS, isect.LINE_LINE_EXACT)) {
          const float factor_i = get_intersection_distance_of_segments(co_i1, co_i2, co_j1, co_j2);
          const float factor_j = get_intersection_distance_of_segments(co_j1, co_j2, co_i1, co_i2);

          /* If the intersection is outside of the edge, skip it. Note that exactly on the edge is
           * accepted. */
          if (factor_i < 0.0f || factor_i > 1.0f || factor_j < 0.0f || factor_j > 1.0f) {
            continue;
          }

          r_inters_per_curves[curve_i].append(r_intersections.size());
          r_inters_per_curves[curve_j].append(r_intersections.size());
          r_intersections.append(
              create_intersection(point_i1, point_j1, factor_i, factor_j, curve_i, curve_j));
        }
      }
    });
  }
}

/* TODO: This method of finding intersections is O(N^2) and should replaced with something faster.
 */
void find_intersections_between_all_curves(const Span<float2> screen_space_positions,
                                           const Span<Bounds<float2>> screen_space_bbox,
                                           const OffsetIndices<int> points_by_curve,
                                           const VArray<bool> &cyclic,
                                           const IndexMask &visible_curves,
                                           Array<Vector<int>> &r_inters_per_curves,
                                           Vector<IntersectionPoint> &r_intersections)
{
  visible_curves.foreach_index([&](const int curve_i) {
    find_intersections_between_curve_and_curves(screen_space_positions,
                                                screen_space_bbox,
                                                points_by_curve,
                                                cyclic,
                                                visible_curves,
                                                curve_i,
                                                r_inters_per_curves,
                                                r_intersections);
  });
}

void store_segment_map_on_intersections(const Span<Segment> all_segments,
                                        MutableSpan<IntersectionPoint> intersections)
{
  for (const int seg_i : all_segments.index_range()) {
    const Segment &segment = all_segments[seg_i];
    const int curve_i = segment.curve;

    if (segment.has_intersection(Side::Start)) {
      IntersectionPoint &inter_start = intersections[segment.intersection_index[Side::Start]];
      if (curve_i == inter_start.curve_i) {
        inter_start.segment_index_i[Side::End] = seg_i;
      }
      else {
        inter_start.segment_index_j[Side::End] = seg_i;
      }
    }

    if (segment.has_intersection(Side::End)) {
      IntersectionPoint &inter_end = intersections[segment.intersection_index[Side::End]];
      if (curve_i == inter_end.curve_i) {
        inter_end.segment_index_i[Side::Start] = seg_i;
      }
      else {
        inter_end.segment_index_j[Side::Start] = seg_i;
      }
    }
  }
}

int create_segments_from_intersections_single_curve(const int curve_k,
                                                    const Span<Vector<int>> inters_per_curves,
                                                    const OffsetIndices<int> points_by_curve,
                                                    const Span<IntersectionPoint> &intersections,
                                                    const VArray<bool> &cyclic,
                                                    Vector<Segment> &all_segments)
{
  const IndexRange points_k = points_by_curve[curve_k];
  const Span<int> inters = inters_per_curves[curve_k];

  const int start_size = all_segments.size();

  if (inters.size() == 0) {
    all_segments.append(Segment::from_curve(curve_k, points_k, cyclic[curve_k]));
    return 1;
  }

  if (inters.size() == 1 && cyclic[curve_k]) {
    const int int_p = inters.first();

    const IntersectionPoint &inter = intersections[int_p];

    all_segments.append(Segment::from_intersections(curve_k,
                                                    points_k,
                                                    inter.point_for_curve(curve_k),
                                                    inter.point_for_curve(curve_k),
                                                    inter.factor_for_curve(curve_k),
                                                    inter.factor_for_curve(curve_k),
                                                    int_p,
                                                    int_p));
    all_segments.last().full_wrap_loop = true;
    return 1;
  }

  Array<int> inter_sorted_ids = Array<int>(inters.size());
  array_utils::fill_index_range<int>(inter_sorted_ids);

  parallel_sort(inter_sorted_ids.begin(), inter_sorted_ids.end(), [&](int i1, int i2) {
    const IntersectionPoint &inter1 = intersections[inters[i1]];
    const IntersectionPoint &inter2 = intersections[inters[i2]];
    return inter1.parameter_for_curve(curve_k) < inter2.parameter_for_curve(curve_k);
  });

  if (cyclic[curve_k]) {
    const int int_p_1 = inters[inter_sorted_ids.first()];
    const int int_p_2 = inters[inter_sorted_ids.last()];

    const IntersectionPoint &inter_first = intersections[int_p_1];
    const IntersectionPoint &inter_last = intersections[int_p_2];

    all_segments.append(Segment::from_intersections(curve_k,
                                                    points_k,
                                                    inter_last.point_for_curve(curve_k),
                                                    inter_first.point_for_curve(curve_k),
                                                    inter_last.factor_for_curve(curve_k),
                                                    inter_first.factor_for_curve(curve_k),
                                                    int_p_2,
                                                    int_p_1));
  }
  else {
    const int int_p_1 = inters[inter_sorted_ids.first()];
    const IntersectionPoint &inter_first = intersections[int_p_1];

    if (inter_first.parameter_for_curve(curve_k) != float(points_k.first())) {
      all_segments.append(Segment::from_intersections(curve_k,
                                                      points_k,
                                                      std::nullopt,
                                                      inter_first.point_for_curve(curve_k),
                                                      std::nullopt,
                                                      inter_first.factor_for_curve(curve_k),
                                                      std::nullopt,
                                                      int_p_1));
    }
  }

  for (const int inter_id : inter_sorted_ids.index_range().drop_back(1)) {
    const int int_p_1 = inters[inter_sorted_ids[inter_id]];
    const int int_p_2 = inters[inter_sorted_ids[inter_id + 1]];

    const IntersectionPoint &inter_first = intersections[int_p_1];
    const IntersectionPoint &inter_last = intersections[int_p_2];

    if (inter_first.parameter_for_curve(curve_k) != inter_last.parameter_for_curve(curve_k)) {
      all_segments.append(Segment::from_intersections(curve_k,
                                                      points_k,
                                                      inter_first.point_for_curve(curve_k),
                                                      inter_last.point_for_curve(curve_k),
                                                      inter_first.factor_for_curve(curve_k),
                                                      inter_last.factor_for_curve(curve_k),
                                                      int_p_1,
                                                      int_p_2));
    }
  }

  if (!cyclic[curve_k]) {
    const int int_p_2 = inters[inter_sorted_ids.last()];
    const IntersectionPoint &inter_last = intersections[int_p_2];

    if (inter_last.parameter_for_curve(curve_k) != float(points_k.last())) {
      all_segments.append(Segment::from_intersections(curve_k,
                                                      points_k,
                                                      inter_last.point_for_curve(curve_k),
                                                      std::nullopt,
                                                      inter_last.factor_for_curve(curve_k),
                                                      std::nullopt,
                                                      int_p_2,
                                                      std::nullopt));
    }
  }

  return all_segments.size() - start_size;
}

void create_segments_from_intersections(const Span<Vector<int>> inters_per_curves,
                                        const OffsetIndices<int> points_by_curve,
                                        const Span<IntersectionPoint> &intersections,
                                        const VArray<bool> &cyclic,
                                        Vector<Segment> &all_segments,
                                        MutableSpan<int> segments_num_per_curve)
{
  for (const int curve_k : points_by_curve.index_range()) {
    segments_num_per_curve[curve_k] = create_segments_from_intersections_single_curve(
        curve_k, inters_per_curves, points_by_curve, intersections, cyclic, all_segments);
  }
}

static bool check_and_join_segments(Segment &first, const Segment &second)
{
  if (first.curve != second.curve) {
    return false;
  }

  const float parameter_first_start = first.points[Side::Start] +
                                      first.intersection_factor[Side::Start];
  const float parameter_first_end = first.points[Side::End] + first.intersection_factor[Side::End];
  const float parameter_second_start = second.points[Side::Start] +
                                       second.intersection_factor[Side::Start];
  const float parameter_second_end = second.points[Side::End] +
                                     second.intersection_factor[Side::End];

  if ((parameter_first_end == parameter_second_start) ||
      (first.intersection_index[Side::End] == second.intersection_index[Side::Start] &&
       first.intersection_index[Side::End] != -1))
  {
    first.points[Side::End] = second.points[Side::End];
    first.intersection_factor[Side::End] = second.intersection_factor[Side::End];
    first.intersection_index[Side::End] = second.intersection_index[Side::End];

    return true;
  }
  if ((parameter_first_start == parameter_second_end) ||
      (first.intersection_index[Side::Start] == second.intersection_index[Side::End] &&
       first.intersection_index[Side::Start] != -1))
  {
    first.points[Side::Start] = second.points[Side::Start];
    first.intersection_factor[Side::Start] = second.intersection_factor[Side::Start];
    first.intersection_index[Side::Start] = second.intersection_index[Side::Start];

    return true;
  }

  return false;
}

void cut_caps(bke::CurvesGeometry &dst,
              const Span<Segment> segments,
              const Span<bool> segment_reversed,
              const Span<bool> cyclic,
              const OffsetIndices<int> segment_offsets)
{
  bke::MutableAttributeAccessor dst_attributes = dst.attributes_for_write();

  bke::SpanAttributeWriter dst_start_caps = dst_attributes.lookup_or_add_for_write_span<int8_t>(
      "start_cap", bke::AttrDomain::Curve);
  bke::SpanAttributeWriter dst_end_caps = dst_attributes.lookup_or_add_for_write_span<int8_t>(
      "end_cap", bke::AttrDomain::Curve);

  threading::parallel_for(segment_offsets.index_range(), 4096, [&](const IndexRange curves) {
    for (const int curve_i : curves) {
      /* If the curve is cyclic, don't cut it. */
      if (cyclic[curve_i]) {
        continue;
      }

      const IndexRange segment_range = segment_offsets[curve_i];

      const int segment_index_first = segment_range.first();
      const bool reversed_first = segment_reversed[segment_index_first];
      const Segment &segment_first = segments[segment_index_first];
      const Side direction_first = reversed_first ? Side::End : Side::Start;
      const int inter_index_first = segment_first.intersection_index[direction_first];

      const int segment_index_last = segment_range.last();
      const bool reversed_last = segment_reversed[segment_index_last];
      const Segment &segment_last = segments[segment_index_last];
      const Side direction_last = reversed_last ? Side::Start : Side::End;
      const int inter_index_last = segment_last.intersection_index[direction_last];

      /* Check if there is a intersection and therefore the curve should be cut. */
      if (inter_index_first != -1) {
        dst_start_caps.span[curve_i] = GP_STROKE_CAP_TYPE_FLAT;
      }
      if (inter_index_last != -1) {
        dst_end_caps.span[curve_i] = GP_STROKE_CAP_TYPE_FLAT;
      }
    }
  });

  dst_start_caps.finish();
  dst_end_caps.finish();
}

/* We store the side as sign, but because a segment with index zero is valid, we shift by one. */
EncodedConnection encode_index_and_side(const int index, const Side side)
{
  return side == Side::Start ? index + 1 : -(index + 1);
}

int decode_index(const EncodedConnection encoded)
{
  return math::abs(encoded) - 1;
}

Side decode_side(const EncodedConnection encoded)
{
  return encoded < 0 ? Side::End : Side::Start;
}

void create_connections_from_curves(const OffsetIndices<int> segments_by_curve,
                                    const Span<bool> segments_to_keep,
                                    const VArray<bool> &is_cyclic,
                                    MutableSpan<SegmentConnections> segment_connections)
{

  threading::parallel_for(segments_by_curve.index_range(), 4096, [&](const IndexRange curves) {
    for (const int curve_i : curves) {
      const IndexRange segment_range = segments_by_curve[curve_i];

      if (segment_range.size() == 1) {
        if (segments_to_keep[segment_range.first()]) {
          segment_connections[segment_range.first()][Side::Start] = SEGMENT_CONNECTION_NULL;
          segment_connections[segment_range.first()][Side::End] = SEGMENT_CONNECTION_NULL;
        }
        continue;
      }

      for (const int segment_i : segment_range.drop_back(1)) {
        if (!segments_to_keep[segment_i]) {
          continue;
        }

        if (segments_to_keep[segment_i + 1]) {
          segment_connections[segment_i][Side::End] = encode_index_and_side(segment_i + 1,
                                                                            Side::Start);
          segment_connections[segment_i + 1][Side::Start] = encode_index_and_side(segment_i,
                                                                                  Side::End);
        }
        else {
          segment_connections[segment_i][Side::End] = SEGMENT_CONNECTION_NULL;
        }
      }

      if (!segments_to_keep[segment_range.last()]) {
        continue;
      }

      if (!is_cyclic[curve_i]) {
        segment_connections[segment_range.first()][Side::Start] = SEGMENT_CONNECTION_NULL;
        segment_connections[segment_range.last()][Side::End] = SEGMENT_CONNECTION_NULL;
        continue;
      }

      if (segments_to_keep[segment_range.first()]) {
        segment_connections[segment_range.first()][Side::Start] = encode_index_and_side(
            segment_range.last(), Side::End);
        segment_connections[segment_range.last()][Side::End] = encode_index_and_side(
            segment_range.first(), Side::Start);
      }
      else {
        segment_connections[segment_range.last()][Side::End] = SEGMENT_CONNECTION_NULL;
      }
    }
  });
}

void follow_segment_connections(const Span<Segment> all_segments,
                                const Span<bool> segments_to_keep,
                                const Span<SegmentConnections> segment_connections,
                                Vector<Segment> &segments,
                                Vector<int> &segment_offset_data,
                                Vector<bool> &segment_reversed,
                                Vector<bool> &cyclic)
{
  BLI_assert(all_segments.size() == segments_to_keep.size());
  BLI_assert(all_segments.size() == segment_connections.size());

  segment_offset_data.append(0);

  Array<bool> processed_segments(all_segments.size(), false);
  int start_segment = 0;

  auto get_next_unprocessed_segment = [&]() {
    /* All segment before `start_segment` are guaranteed to be processed, so skip search them.
     * This optimization make the algorithm `O(N)` instead of `O(N^2)`.*/
    const int empty_num = start_segment;
    const int first_segment = processed_segments.as_span().drop_front(empty_num).first_index_try(
        false);

    if (first_segment == -1) {
      return -1;
    }
    return first_segment + empty_num;
  };

  /* Mark all segments that are not to keep as processed. */
  for (const int seg_i : all_segments.index_range()) {
    if (!segments_to_keep[seg_i]) {
      processed_segments[seg_i] = true;
    }
  }

  start_segment = get_next_unprocessed_segment();

  /* Follow each segment until it loops or ends. */
  while (start_segment != -1) {
    Vector<Segment> curve_segments;
    Vector<bool> curve_segment_reversed;

    auto append_segment = [&](const Segment &current_segment, const bool current_backwards) {
      if (curve_segments.size() == 0) {
        curve_segments.append(current_segment);
        curve_segment_reversed.append(current_backwards);
        return;
      }
      /* Check if the last segment can be joined with this one. */
      if (!check_and_join_segments(curve_segments.last(), current_segment)) {
        curve_segments.append(current_segment);
        curve_segment_reversed.append(current_backwards);
      }
    };

    /* Loop backwards to find the first segment. */
    bool current_backwards = true;
    int current_i = start_segment;
    bool curve_done = false;
    while (!curve_done) {
      const EncodedConnection next_encoded =
          segment_connections[current_i][current_backwards ? Side::Start : Side::End];

      if (next_encoded == SEGMENT_CONNECTION_NULL) {
        curve_done = true;
        break;
      }

      const int next_segment = decode_index(next_encoded);
      const Side next_side = decode_side(next_encoded);

      current_i = next_segment;
      current_backwards = next_side == Side::End;

      if (next_segment == start_segment) {
        curve_done = true;
        break;
      }
    }

    /* Reverse the direction. */
    current_backwards = !current_backwards;
    const int first_segment = current_i;

    /* Loop through forwards, adding segments until ending or looping. */
    curve_done = false;
    bool curve_closed = false;
    while (!curve_done) {
      if (processed_segments[current_i] == true) {
        BLI_assert_unreachable();
        break;
      }

      const Segment &current_segment = all_segments[current_i];
      processed_segments[current_i] = true;
      append_segment(current_segment, current_backwards);

      const EncodedConnection next_encoded =
          segment_connections[current_i][current_backwards ? Side::Start : Side::End];

      if (next_encoded == SEGMENT_CONNECTION_NULL) {
        curve_done = true;
        curve_closed = false;
        if (curve_segments.size() == 1) {
          curve_closed = curve_segments.last().is_loop();
        }
        break;
      }

      const int next_segment = decode_index(next_encoded);
      const Side next_side = decode_side(next_encoded);

      /* Check if we are back to the start. */
      if (next_segment == first_segment) {
        curve_done = true;
        curve_closed = true;

        BLI_assert(next_side == Side::Start);

        /* Check if the last segment can be joined to the first one. */
        if (curve_segments.size() == 1) {
          Segment &segment = curve_segments.last();

          const float parameter_start = segment.points[Side::Start] +
                                        segment.intersection_factor[Side::Start];
          const float parameter_end = segment.points[Side::End] +
                                      segment.intersection_factor[Side::End];

          if ((parameter_end == parameter_start) ||
              (segment.intersection_index[Side::End] == segment.intersection_index[Side::Start] &&
               segment.intersection_index[Side::End] != -1))
          {
            if (segment.intersection_factor[Side::Start] == segment.intersection_factor[Side::End])
            {
              segment.full_wrap_loop = true;
            }
          }
          break;
        }
        if (check_and_join_segments(curve_segments.first(), curve_segments.last())) {
          curve_segments.remove_last();
          curve_segment_reversed.remove_last();
        }

        break;
      }

      BLI_assert(segments_to_keep[next_segment]);
      BLI_assert(!processed_segments[next_segment]);

      current_i = next_segment;
      current_backwards = next_side == Side::End;
    }

    segments.extend(curve_segments);
    segment_reversed.extend(curve_segment_reversed);

    segment_offset_data.append(segments.size());
    cyclic.append(curve_closed);

    start_segment = get_next_unprocessed_segment();
  }
}

void compute_bounding_boxes(const OffsetIndices<int> src_points_by_curve,
                            const Span<float2> screen_space_positions,
                            MutableSpan<Bounds<float2>> screen_space_bbox)
{
  threading::parallel_for(
      src_points_by_curve.index_range(), 512, [&](const IndexRange src_curves) {
        for (const int src_curve : src_curves) {
          Bounds<float2> &bbox = screen_space_bbox[src_curve];

          const IndexRange src_points = src_points_by_curve[src_curve];
          bbox = *bounds::min_max(screen_space_positions.slice(src_points));

          /* Add some padding, otherwise we could just miss intersections. */
          bbox.pad(BBOX_PADDING);
        }
      });
}

void compute_bounding_boxes(const OffsetIndices<int> src_points_by_curve,
                            const GroupedSpan<int> shapes,
                            const Span<float2> screen_space_positions,
                            MutableSpan<Bounds<float2>> screen_space_bbox)
{
  threading::parallel_for(shapes.index_range(), 512, [&](const IndexRange shape_ids) {
    for (const int shape_id : shape_ids) {
      std::optional<Bounds<float2>> bbox;

      const Span<int> shape = shapes[shape_id];
      for (const int curve_i : shape) {
        const IndexRange src_points = src_points_by_curve[curve_i];
        bbox = bounds::merge(bbox, *bounds::min_max(screen_space_positions.slice(src_points)));
      }

      /* Add some padding, otherwise we could just miss intersections. */
      bbox->pad(BBOX_PADDING);

      screen_space_bbox[shape_id] = *bbox;
    }
  });
}

}  // namespace blender::ed::greasepencil::segment
