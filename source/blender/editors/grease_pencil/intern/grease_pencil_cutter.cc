/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "BLI_array.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_lasso_2d.hh"
#include "BLI_math_geom.h"
#include "BLI_rect.h"
#include "BLI_task.hh"

#include "BKE_brush.hh"
#include "BKE_context.hh"
#include "BKE_crazyspace.hh"
#include "BKE_curves.hh"
#include "BKE_paint.hh"
#include "BKE_report.hh"

#include "DEG_depsgraph_query.hh"

#include "ED_grease_pencil.hh"
#include "ED_view3d.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"

namespace blender::ed::greasepencil {

enum Side : uint8_t { Start = 0, End = 1 };
enum Distance : uint8_t { Min = 0, Max = 1 };

/**
 * Structure describing a curve segment (a point range in a curve) that needs to be removed from
 * the curve.
 */
struct CutterSegment {
  /* Curve index. */
  int curve;

  /* Point range of the segment: starting point and end point. Matches the point offsets
   * in a CurvesGeometry. */
  int point_range[2];

  /* The normalized distance where the cutter segment is intersected by another curve.
   * For the outer ends of the cutter segment the intersection distance is given between:
   * - [start point - 1] and [start point]
   * - [end point] and [end point + 1]
   */
  float intersection_distance[2];

  /* Intersection flag: true if the start/end point of the segment is the result of an
   * intersection, false if the point is the outer end of a curve. */
  bool is_intersected[2];
};

/**
 * Structure describing:
 * - A collection of cutter segments.
 */
struct CutterSegments {
  /* Collection of cutter segments: parts of curves between other curves, to be removed from the
   * geometry. */
  Vector<CutterSegment> segments;

  /* Create an initial cutter segment with a point range of one point. */
  CutterSegment *create_segment(const int curve, const int point)
  {
    CutterSegment segment{};
    segment.curve = curve;
    segment.point_range[Side::Start] = point;
    segment.point_range[Side::End] = point;

    this->segments.append(std::move(segment));

    return &this->segments.last();
  }

  /* Merge cutter segments that are next to each other. */
  void merge_adjacent_segments()
  {
    Vector<CutterSegment> merged_segments;

    /* Note on performance: we deal with small numbers here, so we can afford the double loop. */
    while (!this->segments.is_empty()) {
      CutterSegment a = this->segments.pop_last();

      bool merged = false;
      for (CutterSegment &b : merged_segments) {
        if (a.curve != b.curve) {
          continue;
        }
        /* The segments overlap when the points ranges have overlap or are exactly adjacent. */
        if ((a.point_range[Side::Start] <= b.point_range[Side::End] &&
             a.point_range[Side::End] >= b.point_range[Side::Start]) ||
            (a.point_range[Side::End] == b.point_range[Side::Start] - 1) ||
            (b.point_range[Side::End] == a.point_range[Side::Start] - 1))
        {
          /* Merge the point ranges and related intersection data. */
          const bool take_start_a = a.point_range[Side::Start] < b.point_range[Side::Start];
          const bool take_end_a = a.point_range[Side::End] > b.point_range[Side::End];
          b.point_range[Side::Start] = take_start_a ? a.point_range[Side::Start] :
                                                      b.point_range[Side::Start];
          b.point_range[Side::End] = take_end_a ? a.point_range[Side::End] :
                                                  b.point_range[Side::End];
          b.is_intersected[Side::Start] = take_start_a ? a.is_intersected[Side::Start] :
                                                         b.is_intersected[Side::Start];
          b.is_intersected[Side::End] = take_end_a ? a.is_intersected[Side::End] :
                                                     b.is_intersected[Side::End];
          b.intersection_distance[Side::Start] = take_start_a ?
                                                     a.intersection_distance[Side::Start] :
                                                     b.intersection_distance[Side::Start];
          b.intersection_distance[Side::End] = take_end_a ? a.intersection_distance[Side::End] :
                                                            b.intersection_distance[Side::End];
          merged = true;
          break;
        }
      }
      if (!merged) {
        merged_segments.append(std::move(a));
      }
    }

    this->segments = merged_segments;
  }
};

/* When looking for intersections, we need a little padding, otherwise we could miss curves
 * that intersect for the eye, but not in hard numbers. */
static constexpr int BBOX_PADDING = 2;

/* When creating new intersection points, we don't want them too close to their neighbour,
 * because that clutters the geometry. This threshold defines what 'too close' is. */
static constexpr float DISTANCE_FACTOR_THRESHOLD = 0.01f;

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

  const float det = float(a1 * b2 - a2 * b1);
  if (det == 0.0f) {
    return 0.0f;
  }

  float2 isect((b2 * c1 - b1 * c2) / det, (a1 * c2 - a2 * c1) / det);

  /* Get normalized distance from point a to intersection point. */
  const float length_ab = math::length(co_b - co_a);
  float distance = (length_ab == 0.0f ?
                        0.0f :
                        math::clamp(math::length(isect - co_a) / length_ab, 0.0f, 1.0f));

  return distance;
}

/**
 * For a curve, find all intersections with other curves.
 */
static void get_intersections_of_curve_with_curves(const int src_curve,
                                                   const bke::CurvesGeometry &src,
                                                   const Span<float2> screen_space_positions,
                                                   const Span<rcti> screen_space_bbox,
                                                   MutableSpan<bool> r_is_intersected_after_point,
                                                   MutableSpan<float2> r_intersection_distance)
{
  const OffsetIndices<int> points_by_curve = src.points_by_curve();
  const VArray<bool> is_cyclic = src.cyclic();

  /* Edge case: skip curve with only one point. */
  if (points_by_curve[src_curve].size() < 2) {
    return;
  }

  /* Loop all curve points and check for intersections between point a and point a + 1. */
  const IndexRange src_curve_points = points_by_curve[src_curve].drop_back(
      is_cyclic[src_curve] ? 0 : 1);
  for (const int point_a : src_curve_points) {
    const int point_b = (point_a == points_by_curve[src_curve].last()) ? src_curve_points.first() :
                                                                         point_a + 1;

    /* Get coordinates of segment a-b. */
    const float2 co_a = screen_space_positions[point_a];
    const float2 co_b = screen_space_positions[point_b];
    rcti bbox_ab;
    BLI_rcti_init_minmax(&bbox_ab);
    BLI_rcti_do_minmax_v(&bbox_ab, int2(co_a));
    BLI_rcti_do_minmax_v(&bbox_ab, int2(co_b));
    BLI_rcti_pad(&bbox_ab, BBOX_PADDING, BBOX_PADDING);

    float intersection_distance_min = FLT_MAX;
    float intersection_distance_max = -FLT_MAX;

    /* Loop all curves, looking for intersecting segments. */
    for (const int curve : src.curves_range()) {
      /* Only process curves with at least two points. */
      if (points_by_curve[curve].size() < 2) {
        continue;
      }

      /* Bounding box check: skip curves that don't overlap segment a-b. */
      if (!BLI_rcti_isect(&bbox_ab, &screen_space_bbox[curve], nullptr)) {
        continue;
      }

      /* Find intersecting curve segments. */
      const IndexRange points = points_by_curve[curve].drop_back(is_cyclic[curve] ? 0 : 1);
      for (const int point_c : points) {
        const int point_d = (point_c == points_by_curve[curve].last()) ? points.first() :
                                                                         (point_c + 1);

        /* Don't self check. */
        if (curve == src_curve &&
            (point_a == point_c || point_a == point_d || point_b == point_c || point_b == point_d))
        {
          continue;
        }

        /* Skip when bounding boxes of a-b and c-d don't overlap. */
        const float2 co_c = screen_space_positions[point_c];
        const float2 co_d = screen_space_positions[point_d];
        rcti bbox_cd;
        BLI_rcti_init_minmax(&bbox_cd);
        BLI_rcti_do_minmax_v(&bbox_cd, int2(co_c));
        BLI_rcti_do_minmax_v(&bbox_cd, int2(co_d));
        BLI_rcti_pad(&bbox_cd, BBOX_PADDING, BBOX_PADDING);
        if (!BLI_rcti_isect(&bbox_ab, &bbox_cd, nullptr)) {
          continue;
        }

        /* Add some padding to the line segment c-d, otherwise we could just miss an
         * intersection. */
        const float2 padding_cd = math::normalize(co_d - co_c);
        const float2 padded_c = co_c - padding_cd;
        const float2 padded_d = co_d + padding_cd;

        /* Check for intersection. */
        const auto isect = math::isect_seg_seg(co_a, co_b, padded_c, padded_d);
        if (ELEM(isect.kind, isect.LINE_LINE_CROSS, isect.LINE_LINE_EXACT)) {
          /* We found an intersection, set the intersection flag for segment a-b. */
          r_is_intersected_after_point[point_a] = true;

          /* Calculate the intersection factor. This is the normalized distance (0..1) of the
           * intersection point on line segment a-b, measured from point a. */
          const float normalized_distance = get_intersection_distance_of_segments(
              co_a, co_b, co_c, co_d);
          intersection_distance_min = math::min(normalized_distance, intersection_distance_min);
          intersection_distance_max = math::max(normalized_distance, intersection_distance_max);
        }
      }
    }

    if (r_is_intersected_after_point[point_a]) {
      r_intersection_distance[point_a][Distance::Min] = intersection_distance_min;
      r_intersection_distance[point_a][Distance::Max] = intersection_distance_max;
    }
  }
}

/**
 * Expand a cutter segment by walking along the curve in forward or backward direction.
 * A cutter segments ends at an intersection with another curve, or at the outer end of the curve.
 */
static void expand_cutter_segment_direction(CutterSegment &segment,
                                            const int direction,
                                            const bke::CurvesGeometry &src,
                                            const Span<bool> is_intersected_after_point,
                                            const Span<float2> intersection_distance,
                                            MutableSpan<bool> point_is_in_segment)
{
  const OffsetIndices<int> points_by_curve = src.points_by_curve();
  const int point_first = points_by_curve[segment.curve].first();
  const int point_last = points_by_curve[segment.curve].last();

  const Side segment_side = (direction == 1) ? Side::End : Side::Start;
  int point_a = segment.point_range[segment_side];

  bool intersected = false;
  segment.is_intersected[segment_side] = false;

  /* Walk along the curve points. */
  while ((direction == 1 && point_a < point_last) || (direction == -1 && point_a > point_first)) {
    const int point_b = point_a + direction;
    const bool at_end_of_curve = (direction == -1 && point_b == point_first) ||
                                 (direction == 1 && point_b == point_last);

    /* Expand segment point range. */
    segment.point_range[segment_side] = point_a;
    point_is_in_segment[point_a] = true;

    /* Check for intersections with other curves. The intersections were established in ascending
     * point order, so in forward direction we look at line segment a-b, in backward direction we
     * look at line segment b-a. */
    const int intersection_point = direction == 1 ? point_a : point_b;
    intersected = is_intersected_after_point[intersection_point];

    /* Avoid orphaned points at the end of a curve. */
    if (at_end_of_curve &&
        ((direction == -1 &&
          intersection_distance[intersection_point][Distance::Max] < DISTANCE_FACTOR_THRESHOLD) ||
         (direction == 1 && intersection_distance[intersection_point][Distance::Min] >
                                (1.0f - DISTANCE_FACTOR_THRESHOLD))))
    {
      intersected = false;
      break;
    }

    /* When we hit an intersection, store the intersection distance. Potentially, line segment
     * a-b can be intersected by multiple curves, so we want to fetch the first intersection
     * point we bumped into. In forward direction this is the minimum distance, in backward
     * direction the maximum. */
    if (intersected) {
      segment.is_intersected[segment_side] = true;
      segment.intersection_distance[segment_side] =
          (direction == 1) ? intersection_distance[intersection_point][Distance::Min] :
                             intersection_distance[intersection_point][Distance::Max];
      break;
    }

    /* Keep walking along curve. */
    point_a += direction;
  }

  /* Adjust point range at curve ends. */
  if (!intersected) {
    if (direction == -1) {
      segment.point_range[Side::Start] = point_first;
      point_is_in_segment[point_first] = true;
    }
    else {
      segment.point_range[Side::End] = point_last;
      point_is_in_segment[point_last] = true;
    }
  }
}

/**
 * Expand a cutter segment of one point by walking along the curve in both directions.
 */
static void expand_cutter_segment(CutterSegment &segment,
                                  const bke::CurvesGeometry &src,
                                  const Span<bool> is_intersected_after_point,
                                  const Span<float2> intersection_distance,
                                  MutableSpan<bool> point_is_in_segment)
{
  const int8_t directions[2] = {-1, 1};
  for (const int8_t direction : directions) {
    expand_cutter_segment_direction(segment,
                                    direction,
                                    src,
                                    is_intersected_after_point,
                                    intersection_distance,
                                    point_is_in_segment);
  }
}

/**
 * Find curve points within the lasso area, expand them to segments between other curves and
 * delete them from the geometry.
 */
static std::optional<bke::CurvesGeometry> stroke_cutter_find_and_remove_segments(
    const bke::CurvesGeometry &src,
    const Span<int2> mcoords,
    const Span<float2> screen_space_positions,
    const Span<rcti> screen_space_bbox,
    const bool keep_caps)
{
  const OffsetIndices<int> src_points_by_curve = src.points_by_curve();
  rcti bbox_lasso;
  BLI_lasso_boundbox(&bbox_lasso, mcoords);

  /* Collect curves and curve points inside the lasso area. */
  Vector<int> selected_curves;
  Vector<Vector<int>> selected_points_in_curves;
  for (const int src_curve : src.curves_range()) {
    /* To speed things up: do a bounding box check on the curve and the lasso area. */
    if (!BLI_rcti_isect(&bbox_lasso, &screen_space_bbox[src_curve], nullptr)) {
      continue;
    }

    /* Look for curve points inside the lasso area. */
    Vector<int> selected_points;
    for (const int src_point : src_points_by_curve[src_curve]) {
      /* Check if point is inside the lasso area. */
      if (BLI_rcti_isect_pt_v(&bbox_lasso, int2(screen_space_positions[src_point])) &&
          BLI_lasso_is_point_inside(mcoords,
                                    int(screen_space_positions[src_point].x),
                                    int(screen_space_positions[src_point].y),
                                    IS_CLIPPED))
      {
        if (selected_points.is_empty()) {
          selected_curves.append(src_curve);
        }
        selected_points.append(src_point);
      }
    }
    if (!selected_points.is_empty()) {
      selected_points_in_curves.append(std::move(selected_points));
    }
  }

  /* Abort when the lasso area is empty. */
  if (selected_curves.is_empty()) {
    return std::nullopt;
  }

  /* For the selected curves, find all the intersections with other curves. */
  const int src_points_num = src.points_num();
  Array<bool> is_intersected_after_point(src_points_num, false);
  Array<float2> intersection_distance(src_points_num);
  threading::parallel_for(selected_curves.index_range(), 1, [&](const IndexRange curve_range) {
    for (const int selected_curve : curve_range) {
      const int src_curve = selected_curves[selected_curve];
      get_intersections_of_curve_with_curves(src_curve,
                                             src,
                                             screen_space_positions,
                                             screen_space_bbox,
                                             is_intersected_after_point,
                                             intersection_distance);
    }
  });

  /* Expand the selected curve points to cutter segments (the part of the curve between two
   * intersections). */
  const VArray<bool> is_cyclic = src.cyclic();
  Array<bool> point_is_in_segment(src_points_num, false);
  threading::EnumerableThreadSpecific<CutterSegments> cutter_segments_by_thread;

  threading::parallel_for(selected_curves.index_range(), 1, [&](const IndexRange curve_range) {
    for (const int selected_curve : curve_range) {
      CutterSegments &thread_segments = cutter_segments_by_thread.local();
      const int src_curve = selected_curves[selected_curve];

      for (const int selected_point : selected_points_in_curves[selected_curve]) {
        /* Skip point when it is already part of a cutter segment. */
        if (point_is_in_segment[selected_point]) {
          continue;
        }

        /* Create new cutter segment. */
        CutterSegment *segment = thread_segments.create_segment(src_curve, selected_point);

        /* Expand the cutter segment in both directions until an intersection is found or the
         * end of the curve is reached. */
        expand_cutter_segment(
            *segment, src, is_intersected_after_point, intersection_distance, point_is_in_segment);

        /* When the end of a curve is reached and the curve is cyclic, we add an extra cutter
         * segment for the cyclic second part. */
        if (is_cyclic[src_curve] &&
            (!segment->is_intersected[Side::Start] || !segment->is_intersected[Side::End]) &&
            !(!segment->is_intersected[Side::Start] && !segment->is_intersected[Side::End]))
        {
          const int cyclic_outer_point = !segment->is_intersected[Side::Start] ?
                                             src_points_by_curve[src_curve].last() :
                                             src_points_by_curve[src_curve].first();
          segment = thread_segments.create_segment(src_curve, cyclic_outer_point);

          /* Expand this second segment. */
          expand_cutter_segment(*segment,
                                src,
                                is_intersected_after_point,
                                intersection_distance,
                                point_is_in_segment);
        }
      }
    }
  });
  CutterSegments cutter_segments;
  for (CutterSegments &thread_segments : cutter_segments_by_thread) {
    cutter_segments.segments.extend(thread_segments.segments);
  }

  /* Abort when no cutter segments are found in the lasso area. */
  if (cutter_segments.segments.is_empty()) {
    return std::nullopt;
  }

  /* Merge adjacent cutter segments. E.g. two point ranges of 0-10 and 11-20 will be merged
   * to one range of 0-20. */
  cutter_segments.merge_adjacent_segments();

  /* Create the point transfer data, for converting the source geometry into the new geometry.
   * First, add all curve points not affected by the cutter tool. */
  Array<Vector<PointTransferData>> src_to_dst_points(src_points_num);
  for (const int src_curve : src.curves_range()) {
    const IndexRange src_points = src_points_by_curve[src_curve];
    for (const int src_point : src_points) {
      Vector<PointTransferData> &dst_points = src_to_dst_points[src_point];
      const int src_next_point = (src_point == src_points.last()) ? src_points.first() :
                                                                    (src_point + 1);

      /* Add the source point only if it does not lie inside a cutter segment. */
      if (!point_is_in_segment[src_point]) {
        dst_points.append({src_point, src_next_point, 0.0f, true, false});
      }
    }
  }

  /* Add new curve points at the intersection points of the cutter segments.
   *
   *                               a                 b
   *  source curve    o--------o---*---o--------o----*---o--------o
   *                               ^                 ^
   *  cutter segment               |-----------------|
   *
   *  o = existing curve point
   *  * = newly created curve point
   *
   *  The curve points between *a and *b will be deleted.
   *  The source curve will be cut in two:
   *  - the first curve ends at *a
   *  - the second curve starts at *b
   *
   * We avoid inserting a new point very close to the adjacent one, because that's just adding
   * clutter to the geometry.
   */
  for (const CutterSegment &cutter_segment : cutter_segments.segments) {
    /* Intersection at cutter segment start. */
    if (cutter_segment.is_intersected[Side::Start] &&
        cutter_segment.intersection_distance[Side::Start] > DISTANCE_FACTOR_THRESHOLD)
    {
      const int src_point = cutter_segment.point_range[Side::Start] - 1;
      Vector<PointTransferData> &dst_points = src_to_dst_points[src_point];
      dst_points.append({src_point,
                         src_point + 1,
                         cutter_segment.intersection_distance[Side::Start],
                         false,
                         false});
    }
    /* Intersection at cutter segment end. */
    if (cutter_segment.is_intersected[Side::End]) {
      const int src_point = cutter_segment.point_range[Side::End];
      if (cutter_segment.intersection_distance[Side::End] < (1.0f - DISTANCE_FACTOR_THRESHOLD)) {
        Vector<PointTransferData> &dst_points = src_to_dst_points[src_point];
        dst_points.append({src_point,
                           src_point + 1,
                           cutter_segment.intersection_distance[Side::End],
                           false,
                           true});
      }
      else {
        /* Mark the 'is_cut' flag on the next point, because a new curve is starting here after
         * the removed cutter segment. */
        Vector<PointTransferData> &dst_points = src_to_dst_points[src_point + 1];
        for (PointTransferData &dst_point : dst_points) {
          if (dst_point.is_src_point) {
            dst_point.is_cut = true;
          }
        }
      }
    }
  }

  /* Create the new curves geometry. */
  bke::CurvesGeometry dst;
  compute_topology_change(src, dst, src_to_dst_points, keep_caps);

  return dst;
}

/**
 * Apply the stroke cutter to a drawing.
 */
static bool execute_cutter_on_drawing(const int layer_index,
                                      const int frame_number,
                                      const Object &ob_eval,
                                      const Object &obact,
                                      const ARegion &region,
                                      const float4x4 &projection,
                                      const Span<int2> mcoords,
                                      const bool keep_caps,
                                      bke::greasepencil::Drawing &drawing)
{
  const bke::CurvesGeometry &src = drawing.strokes();

  /* Get evaluated geometry. */
  bke::crazyspace::GeometryDeformation deformation =
      bke::crazyspace::get_evaluated_grease_pencil_drawing_deformation(
          &ob_eval, obact, layer_index, frame_number);

  /* Compute screen space positions. */
  Array<float2> screen_space_positions(src.points_num());
  threading::parallel_for(src.points_range(), 4096, [&](const IndexRange src_points) {
    for (const int src_point : src_points) {
      screen_space_positions[src_point] = ED_view3d_project_float_v2_m4(
          &region, deformation.positions[src_point], projection);
    }
  });

  /* Compute bounding boxes of curves in screen space. The bounding boxes are used to speed
   * up the search for intersecting curves. */
  Array<rcti> screen_space_bbox(src.curves_num());
  const OffsetIndices<int> src_points_by_curve = src.points_by_curve();
  threading::parallel_for(src.curves_range(), 512, [&](const IndexRange src_curves) {
    for (const int src_curve : src_curves) {
      rcti *bbox = &screen_space_bbox[src_curve];
      BLI_rcti_init_minmax(bbox);

      const IndexRange src_points = src_points_by_curve[src_curve];
      for (const int src_point : src_points) {
        BLI_rcti_do_minmax_v(bbox, int2(screen_space_positions[src_point]));
      }

      /* Add some padding, otherwise we could just miss intersections. */
      BLI_rcti_pad(bbox, BBOX_PADDING, BBOX_PADDING);
    }
  });

  /* Apply cutter. */
  std::optional<bke::CurvesGeometry> cut_strokes = stroke_cutter_find_and_remove_segments(
      src, mcoords, screen_space_positions, screen_space_bbox, keep_caps);

  if (cut_strokes.has_value()) {
    /* Set the new geometry. */
    drawing.geometry.wrap() = std::move(cut_strokes.value());
    drawing.tag_topology_changed();
  }

  return cut_strokes.has_value();
}

/**
 * Apply the stroke cutter to all layers.
 */
static int stroke_cutter_execute(const bContext *C, const Span<int2> mcoords)
{
  const Scene *scene = CTX_data_scene(C);
  const ARegion *region = CTX_wm_region(C);
  const RegionView3D *rv3d = CTX_wm_region_view3d(C);
  const Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *obact = CTX_data_active_object(C);
  Object *ob_eval = DEG_get_evaluated_object(depsgraph, obact);

  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(obact->data);

  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);
  if (brush->gpencil_settings == nullptr) {
    BKE_brush_init_gpencil_settings(brush);
  }
  const bool keep_caps = (brush->gpencil_settings->flag & GP_BRUSH_ERASER_KEEP_CAPS) != 0;
  const bool active_layer_only = (brush->gpencil_settings->flag & GP_BRUSH_ACTIVE_LAYER_ONLY) != 0;
  std::atomic<bool> changed = false;

  if (active_layer_only) {
    /* Apply cutter on drawings of active layer. */
    if (!grease_pencil.has_active_layer()) {
      return OPERATOR_CANCELLED;
    }
    const bke::greasepencil::Layer &layer = *grease_pencil.get_active_layer();
    const float4x4 layer_to_world = layer.to_world_space(*ob_eval);
    const float4x4 projection = ED_view3d_ob_project_mat_get_from_obmat(rv3d, layer_to_world);
    const Vector<ed::greasepencil::MutableDrawingInfo> drawings =
        ed::greasepencil::retrieve_editable_drawings_from_layer(*scene, grease_pencil, layer);
    threading::parallel_for_each(drawings, [&](const ed::greasepencil::MutableDrawingInfo &info) {
      if (execute_cutter_on_drawing(info.layer_index,
                                    info.frame_number,
                                    *ob_eval,
                                    *obact,
                                    *region,
                                    projection,
                                    mcoords,
                                    keep_caps,
                                    info.drawing))
      {
        changed = true;
      }
    });
  }
  else {
    /* Apply cutter on every editable drawing. */
    const Vector<ed::greasepencil::MutableDrawingInfo> drawings =
        ed::greasepencil::retrieve_editable_drawings(*scene, grease_pencil);
    threading::parallel_for_each(drawings, [&](const ed::greasepencil::MutableDrawingInfo &info) {
      const bke::greasepencil::Layer &layer = *grease_pencil.layers()[info.layer_index];
      const float4x4 layer_to_world = layer.to_world_space(*ob_eval);
      const float4x4 projection = ED_view3d_ob_project_mat_get_from_obmat(rv3d, layer_to_world);
      if (execute_cutter_on_drawing(info.layer_index,
                                    info.frame_number,
                                    *ob_eval,
                                    *obact,
                                    *region,
                                    projection,
                                    mcoords,
                                    keep_caps,
                                    info.drawing))
      {
        changed = true;
      }
    });
  }

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }

  return OPERATOR_FINISHED;
}

static int grease_pencil_stroke_cutter(bContext *C, wmOperator *op)
{
  const Array<int2> mcoords = WM_gesture_lasso_path_to_array(C, op);

  if (mcoords.is_empty()) {
    return OPERATOR_PASS_THROUGH;
  }

  return stroke_cutter_execute(C, mcoords);
}

}  // namespace blender::ed::greasepencil

void GREASE_PENCIL_OT_stroke_cutter(wmOperatorType *ot)
{
  using namespace blender::ed::greasepencil;

  ot->name = "Grease Pencil Cutter";
  ot->idname = "GREASE_PENCIL_OT_stroke_cutter";
  ot->description = "Delete stroke points in between intersecting strokes";

  ot->invoke = WM_gesture_lasso_invoke;
  ot->modal = WM_gesture_lasso_modal;
  ot->exec = grease_pencil_stroke_cutter;
  ot->poll = grease_pencil_painting_poll;
  ot->cancel = WM_gesture_lasso_cancel;

  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;

  WM_operator_properties_gesture_lasso(ot);
}
