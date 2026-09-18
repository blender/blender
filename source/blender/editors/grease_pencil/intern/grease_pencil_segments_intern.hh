/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BKE_grease_pencil.hh"

namespace blender::ed::greasepencil::segment {

/* When looking for intersections, we need a little padding, otherwise we could miss curves
 * that intersect for the eye, but not in hard numbers. */
static constexpr int BBOX_PADDING = 2;

enum Side : uint8_t { Start = 0, End = 1 };

/**
 * Structure describing a curve segment (a point range in a curve) with end intersection points.
 * A Segment can go past the end of the source curve and loop back to the start.
 */
class Segment {
 public:
  /* Curve index. */
  int curve = -1;

  /* The start and end of the original curve is stored, because this segment may go past the end
   * and have to loop. */
  IndexRange src_points;

  /* Point range of the segment: Starting point and end point. Matches the point offsets
   * in a CurvesGeometry. */
  int points[2] = {-1, -1};

  /* If this segment is a full cyclical segment, note that the segment can start and end at some
   * intersection point. */
  bool full_wrap_loop = false;

  /* The normalized distance where the trim segment is intersected by another curve.
   * For the outer ends of the trim segment the intersection distance is given between:
   * - [start point] and [start point + 1]
   * - [end point] and [end point + 1]
   */
  float intersection_factor[2] = {0.0f, 0.0f};

  int intersection_index[2] = {-1, -1};

  constexpr Segment() = default;

  bool is_loop() const;

  bool has_intersection(Side side) const;
  int2 edge(Side side) const;

  int wrap_index(int i) const;

  IndexRange point_range() const;
  int points_num() const;

  template<typename Fn> void foreach_point(Fn &&fn) const;

  static Segment from_curve(int curve_i, IndexRange points, bool cyclic);
  static Segment from_intersections(int curve_i,
                                    IndexRange points,
                                    const std::optional<int> point_start,
                                    const std::optional<int> point_end,
                                    const std::optional<float> factor_start,
                                    const std::optional<float> factor_end,
                                    const std::optional<int> inter_index_start,
                                    const std::optional<int> inter_index_end);
};

template<typename Fn> void Segment::foreach_point(Fn &&fn) const
{
  const IndexRange point_range = this->point_range();

  for (const int64_t pos : point_range.index_range()) {
    const int i = this->wrap_index(point_range[pos]);

    if constexpr (std::is_invocable_r_v<void, Fn, int64_t, int64_t>) {
      fn(i, pos);
    }
    else {
      fn(i);
    }
  }
}

struct IntersectionPoint {
  int point_i = -1;
  int point_j = -1;
  float factor_i = -1.0f;
  float factor_j = -1.0f;
  int curve_i = -1;
  int curve_j = -1;

  int segment_index_i[2] = {-1, -1};
  int segment_index_j[2] = {-1, -1};

  constexpr IntersectionPoint() = default;

  float point_for_curve(const int curve) const
  {
    BLI_assert(curve == curve_i || curve == curve_j);
    return curve == curve_i ? point_i : point_j;
  }

  float factor_for_curve(const int curve) const
  {
    BLI_assert(curve == curve_i || curve == curve_j);
    return curve == curve_i ? factor_i : factor_j;
  }

  float parameter_for_curve(const int curve) const
  {
    return this->point_for_curve(curve) + this->factor_for_curve(curve);
  }

  int other_curve(const int curve) const
  {
    BLI_assert(curve == curve_i || curve == curve_j);
    return curve == curve_i ? curve_j : curve_i;
  }
};

IntersectionPoint create_intersection(const int point_i,
                                      const int point_j,
                                      const float factor_i,
                                      const float factor_j,
                                      const int curve_i,
                                      const int curve_j);

using EncodedConnection = int;
constexpr EncodedConnection SEGMENT_CONNECTION_NULL = 0;

/* We store the side as sign, but because a segment with index zero is valid, we shift by one. */
EncodedConnection encode_index_and_side(const int index, const Side side);
int decode_index(const EncodedConnection encoded);
Side decode_side(const EncodedConnection encoded);

/* Both the start and end of every segment is connected to two other segments or null. */
using SegmentConnections = VecBase<EncodedConnection, 2>;

/* Compute bounding boxes of curves in screen space. The bounding boxes are used to speed
 * up the search for intersecting curves. */
void compute_bounding_boxes(const OffsetIndices<int> src_points_by_curve,
                            const Span<float2> screen_space_positions,
                            MutableSpan<Bounds<float2>> screen_space_bbox);
void compute_bounding_boxes(const OffsetIndices<int> src_points_by_curve,
                            const GroupedSpan<int> shapes,
                            const Span<float2> screen_space_positions,
                            MutableSpan<Bounds<float2>> screen_space_bbox);

void cut_caps(bke::CurvesGeometry &dst,
              const Span<Segment> segments,
              const Span<bool> segment_reversed,
              const Span<bool> cyclic,
              const OffsetIndices<int> segment_offsets);

void find_intersections_between_all_curves(const Span<float2> screen_space_positions,
                                           const Span<Bounds<float2>> screen_space_bbox,
                                           const OffsetIndices<int> points_by_curve,
                                           const VArray<bool> &cyclic,
                                           const IndexMask &visible_curves,
                                           Array<Vector<int>> &r_inters_per_curves,
                                           Vector<IntersectionPoint> &r_intersections);

void store_segment_map_on_intersections(const Span<Segment> all_segments,
                                        MutableSpan<IntersectionPoint> intersections);
int create_segments_from_intersections_single_curve(const int curve_k,
                                                    const Span<Vector<int>> inters_per_curves,
                                                    const OffsetIndices<int> points_by_curve,
                                                    const Span<IntersectionPoint> &intersections,
                                                    const VArray<bool> &cyclic,
                                                    Vector<Segment> &all_segments);
void create_segments_from_intersections(const Span<Vector<int>> inters_per_curves,
                                        const OffsetIndices<int> points_by_curve,
                                        const Span<IntersectionPoint> &intersections,
                                        const VArray<bool> &cyclic,
                                        Vector<Segment> &all_segments,
                                        MutableSpan<int> segments_num_per_curve);
void create_connections_from_curves(const OffsetIndices<int> segments_by_curve,
                                    const Span<bool> segments_to_keep,
                                    const VArray<bool> &is_cyclic,
                                    MutableSpan<SegmentConnections> segment_connections);
void follow_segment_connections(const Span<Segment> all_segments,
                                const Span<bool> segments_to_keep,
                                const Span<SegmentConnections> segment_connections,
                                Vector<Segment> &segments,
                                Vector<int> &segment_offset_data,
                                Vector<bool> &segment_reversed,
                                Vector<bool> &cyclic);

}  // namespace blender::ed::greasepencil::segment
