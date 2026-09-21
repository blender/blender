/* SPDX-FileCopyrightText: 2026 Blender Authors
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

namespace blender::ed::greasepencil::boolean {

using namespace segment;

/* Calculate the winding order of a point when compared against a triangle. */
static float point_in_tri_winding(const float2 pt,
                                  const float2 v1,
                                  const float2 v2,
                                  const float2 v3)
{
  const float side12 = line_point_side_v2(v1, v2, pt);
  const float side23 = line_point_side_v2(v2, v3, pt);
  const float side31 = line_point_side_v2(v3, v1, pt);

  /* The point is on an edge. */
  if ((side12 == 0.0f && side23 >= 0.0f && side31 >= 0.0f) ||
      (side12 >= 0.0f && side23 == 0.0f && side31 >= 0.0f) ||
      (side12 >= 0.0f && side23 >= 0.0f && side31 == 0.0f))
  {
    return 0.5f;
  }
  if ((side12 == 0.0f && side23 <= 0.0f && side31 <= 0.0f) ||
      (side12 <= 0.0f && side23 == 0.0f && side31 <= 0.0f) ||
      (side12 <= 0.0f && side23 <= 0.0f && side31 == 0.0f))
  {
    return -0.5f;
  }

  /* The point is inside. */
  if (side12 >= 0.0f && side23 >= 0.0f && side31 >= 0.0f) {
    return 1.0f;
  }
  if (side12 <= 0.0f && side23 <= 0.0f && side31 <= 0.0f) {
    return -1.0f;
  }

  /* The point is outside. */
  return 0.0f;
}

/* Point must not be on a corner, but can be on an edge. */
static int point_in_polygon_winding_twice(const float2 &point, const Span<float2> poly)
{
  /* Double and store as a int to avoid float rounding. */
  int twice_winding = 0;
  const float2 &tri_p1 = poly[0];
  for (const int i : poly.index_range().drop_front(1).drop_back(1)) {
    const float2 &tri_p2 = poly[i];
    const float2 &tri_p3 = poly[i + 1];
    twice_winding += int(point_in_tri_winding(point, tri_p1, tri_p2, tri_p3) * 2);
  }
  return twice_winding;
}

/* Point must not be on a corner or edge. */
static int point_in_polygon_winding_int(const float2 &point, const Span<float2> poly)
{
  const int twice_winding = point_in_polygon_winding_twice(point, poly);
  return int(twice_winding / 2);
}

/**
 * The Winding State is a sparse way to store what curves a point is inside.
 */
class WindingState {
 private:
  /* Winding order of each curve. */
  Map<int, int> orders_per_curve_;

 public:
  void add_to_curve(const int curve_i, const int winding_i)
  {
    if (winding_i == 0) {
      return;
    }

    if (orders_per_curve_.contains(curve_i)) {
      orders_per_curve_.lookup(curve_i) += winding_i;
    }
    else {
      orders_per_curve_.add(curve_i, winding_i);
    }
  }

  void remove_unneeded_ids()
  {
    auto it_begin = orders_per_curve_.items().begin();
    auto it_end = orders_per_curve_.items().end();
    for (auto it = it_begin; it != it_end; it++) {
      if ((*it).value == 0) {
        orders_per_curve_.remove(it);
      }
    }
  }

  bool is_in_shape(const int shape_id, const GroupedSpan<int> &shapes) const
  {
    int winding = 0;

    const Span<int> shape = shapes[shape_id];
    for (const int curve_i : shape) {
      if (orders_per_curve_.contains(curve_i)) {
        winding += orders_per_curve_.lookup(curve_i);
      }
    }
    /* Odd-Even fill rule. */
    return winding % 2 != 0;
  }

  bool is_in_shapes(const IndexMask &shape_mask, const GroupedSpan<int> &shapes) const
  {
    if (orders_per_curve_.is_empty() || shape_mask.is_empty()) {
      return false;
    }

    return threading::parallel_reduce(
        shape_mask.index_range(),
        4096,
        false,
        [&](const IndexRange range, bool value) {
          if (value) {
            return value;
          }
          shape_mask.slice(range).foreach_index([&](const int shape_i) {
            if (this->is_in_shape(shape_i, shapes)) {
              value = true;
              return;
            }
          });
          return value;
        },
        std::logical_or());
  }

  /* Returns true if the point exists for this boolean operation. */
  bool is_contributing(const CurveBooleanOpParameters op_params,
                       const GroupedSpan<int> &shapes,
                       const int subject_shape,
                       const IndexMask &clipping_shapes) const
  {
    const bool subj = this->is_in_shape(subject_shape, shapes);
    const bool clip = this->is_in_shapes(clipping_shapes, shapes);

    switch (op_params.boolean_mode) {
      case Operation::Intersect: {
        return subj && clip;
      }
      case Operation::Difference: {
        return subj && !clip;
      }
      case Operation::Union: {
        return subj || clip;
      }
      default:
        BLI_assert_unreachable();
        break;
    }

    return false;
  }
};

/* The distance the left and right points will be placed from the segment.
 * A to much smaller will cause floating point problems, but anything much larger can cause
 * inaccuracies. */
constexpr float epsilon_distance = 0.01f;
/* When the segment is vary small the #epsilon_distance can be to large, instead use a factor of
 * the segment's length. */
constexpr float epsilon_factor = 0.05f;
/* Factor along the segment where the left and right points are placed.
 * A value of 0.5 can cause failure, so instead use a random number between 0.0 and 1.0 */
constexpr float mid_point_factor = 0.35421f;

/* Calculate the winding states for left and right of the segment. */
static std::pair<WindingState, WindingState> LR_states_from_segment(
    const Segment &segment,
    const Span<float2> all_positions,
    const OffsetIndices<int> points_by_curve,
    const GroupedSpan<int> shapes,
    const IndexMask &mask_shapes,
    const VArray<bool> &is_fill)
{
  WindingState state_L;
  WindingState state_R;

  const int curve_i = segment.curve;

  float2 point1;
  float2 point2;

  if (segment.has_intersection(Side::Start)) {
    point1 = math::interpolate(all_positions[segment.edge(Side::Start)[0]],
                               all_positions[segment.edge(Side::Start)[1]],
                               segment.intersection_factor[Side::Start]);
  }
  else {
    point1 = all_positions[segment.src_points.first()];
  }

  if (segment.edge(Side::Start)[0] == segment.edge(Side::End)[0] &&
      (segment.points_num() == 0 ||
       (segment.points_num() == 1 &&
        (!segment.has_intersection(Side::Start) && segment.has_intersection(Side::End)))))
  {
    point2 = math::interpolate(all_positions[segment.edge(Side::End)[0]],
                               all_positions[segment.edge(Side::End)[1]],
                               segment.intersection_factor[Side::End]);
  }
  else {
    const int first_end_index = segment.edge(Side::Start)[1];
    int end_index = first_end_index;

    point2 = all_positions[end_index];

    /* If the points are on top of each other, go to the next point. */
    while (math::distance(point1, point2) < 0.001f) {
      end_index = segment.wrap_index(end_index + 1);
      point2 = all_positions[end_index];

      /* Prevent looping forever. */
      if (end_index == first_end_index) {
        break;
      }
    }
  }

  const float2 line_dir = math::normalize(point2 - point1);
  /* Direction to the left. */
  const float2 tan_dir = float2(-line_dir.y, line_dir.x);

  const float2 mid_point = math::interpolate(point1, point2, mid_point_factor);

  const float eps_dis = math::min(epsilon_factor * math::distance(point1, point2),
                                  epsilon_distance);

  const float2 l_point = mid_point + tan_dir * eps_dis;
  const float2 r_point = mid_point - tan_dir * eps_dis;

  mask_shapes.foreach_index([&](const int shape_j_index) {
    const Span<int> shape_j = shapes[shape_j_index];
    for (const int curve_j : shape_j) {
      if (curve_j == curve_i) {
        return;
      }

      const IndexRange points_j = points_by_curve[curve_j];

      const Span<float2> poly_j = all_positions.slice(points_j);

      const int l_winding_i = point_in_polygon_winding_int(l_point, poly_j);
      const int r_winding_i = point_in_polygon_winding_int(r_point, poly_j);

      state_L.add_to_curve(curve_j, l_winding_i);
      state_R.add_to_curve(curve_j, r_winding_i);
    }
  });

  /* Self check. */
  if (is_fill[curve_i]) {
    const IndexRange points_i = points_by_curve[curve_i];

    const Span<float2> poly_i = all_positions.slice(points_i);

    const int l_winding_i = point_in_polygon_winding_int(l_point, poly_i);
    const int r_winding_i = point_in_polygon_winding_int(r_point, poly_i);

    state_L.add_to_curve(curve_i, l_winding_i);
    state_R.add_to_curve(curve_i, r_winding_i);
  }

  state_L.remove_unneeded_ids();
  state_R.remove_unneeded_ids();

  return {state_L, state_R};
}

/* Check the left and right of a curve `k` */
static void get_inside_states_for_segments(const CurveBooleanOpParameters &op_params,
                                           const int curve_k,
                                           const bool is_subj,
                                           const int subj_shape_i,
                                           const Span<float2> points,
                                           const GroupedSpan<int> shapes,
                                           const OffsetIndices<int> points_by_curve,
                                           const IndexMask &clipping_shapes,
                                           const Span<Segment> all_segments,
                                           const OffsetIndices<int> segments_by_curve,
                                           const VArray<bool> &is_fill,
                                           MutableSpan<bool> all_inside_left,
                                           MutableSpan<bool> all_inside_right)
{
  const IndexRange segments = segments_by_curve[curve_k];

  if (segments.is_empty()) {
    return;
  }

  const IndexMask &mask_shapes = is_subj ? clipping_shapes :
                                           (!is_fill[shapes[subj_shape_i].first()] ?
                                                IndexRange(0) :
                                                IndexRange::from_single(subj_shape_i));

  for (const int seg_i : segments) {
    const Segment &this_segment = all_segments[seg_i];

    auto [state_L, state_R] = LR_states_from_segment(
        this_segment, points, points_by_curve, shapes, mask_shapes, is_fill);

    if (is_fill[curve_k]) {
      all_inside_left[seg_i] = state_L.is_contributing(
          op_params, shapes, subj_shape_i, clipping_shapes);
      all_inside_right[seg_i] = state_R.is_contributing(
          op_params, shapes, subj_shape_i, clipping_shapes);
    }
    else {
      all_inside_left[seg_i] = state_L.is_in_shapes(clipping_shapes, shapes);
      all_inside_right[seg_i] = state_R.is_in_shapes(clipping_shapes, shapes);
    }
  }
}

/**
 * Separate disjoint fill islands.
 *
 * Note: Nested islands of 3 or more curves will not be split, (i.e. an island inside a lake inside
 * a island, will stay the same fill id as the upper most parent island.)
 *
 * If a curve as been changed and it is not a child then update it's and it's children's fill ids.
 *
 * Note: Previously unchanged curves may have their fill id changed, if their parent curve id is
 * changed.
 */
static void separate_fill_islands(bke::CurvesGeometry &dst,
                                  const Span<bool> unchanged_curves,
                                  const ProjectionFunc project_fn)
{
  const OffsetIndices<int> points_by_curve = dst.points_by_curve();
  bke::MutableAttributeAccessor attributes = dst.attributes_for_write();
  bke::SpanAttributeWriter<int> fill_ids = attributes.lookup_for_write_span<int>("fill_id");

  Array<float2> dst_positions_2d(dst.points_num());
  const Span<float3> positions = dst.positions();
  for (const int i : dst.points_range()) {
    dst_positions_2d[i] = project_fn(positions[i]);
  }

  /* Returns if #curve_i in inside of #curve_j, only valid if the curves do not intersect. */
  auto is_curve_i_in_j = [&](const int curve_i, const int curve_j) {
    const IndexRange points_i = points_by_curve[curve_i];
    const IndexRange points_j = points_by_curve[curve_j];

    const Span<float2> poly_i = dst_positions_2d.as_span().slice(points_i);
    const Span<float2> poly_j = dst_positions_2d.as_span().slice(points_j);

    const int winding = point_in_polygon_winding_int(poly_i.first(), poly_j);

    return winding % 2 != 0;
  };

  Array<bool> is_curve_child(points_by_curve.size(), false);
  Array<Vector<int>> curve_children(points_by_curve.size());

  for (const int curve_i : points_by_curve.index_range()) {
    /* Skip curves without fill. */
    if (fill_ids.span[curve_i] == 0) {
      continue;
    }

    for (const int curve_j : points_by_curve.index_range()) {
      if (curve_j == curve_i) {
        continue;
      }

      if (fill_ids.span[curve_j] != fill_ids.span[curve_i]) {
        continue;
      }

      if (is_curve_i_in_j(curve_i, curve_j)) {
        is_curve_child[curve_i] = true;
        curve_children[curve_j].append(curve_i);
      }
    }
  }

  int new_fill_id = bke::greasepencil::get_next_available_fill_id(fill_ids.span.varray());

  for (const int curve_i : points_by_curve.index_range()) {
    if (is_curve_child[curve_i]) {
      continue;
    }

    if (fill_ids.span[curve_i] == 0 || unchanged_curves[curve_i]) {
      continue;
    }

    fill_ids.span[curve_i] = new_fill_id;

    const Span<int> curve_i_children = curve_children[curve_i];
    for (const int curve_j : curve_i_children) {
      fill_ids.span[curve_j] = new_fill_id;
    }

    new_fill_id++;
  }

  fill_ids.finish();
}

/**
 * This is a heavily modified implementation of the Greiner-Hormann clipping algorithm.
 *
 * Greiner, Günther; Kai Hormann (1998). "Efficient clipping of arbitrary polygons". ACM
 * Transactions on Graphics. 17 (2): 71-83.
 *
 * The original Greiner-Hormann algorithm works in three phases:
 *  1: Find all intersections and sort them.
 *  2: Set the direction of all intersection point (the paper call it `entry_exit`)
 *  3: Create all polygons by following the direction of each intersection point until it
 * loops.
 *
 * This implementation adds the following:
 *  1: Groups of curves, called `fills`. This allows for input geometry with holes.
 *  2: Curves can have no fill, so they will get cut.
 *
 * This implementation works by:
 *  1: Break one subject fill and all clipping fills into segments and store their intersections.
 *  2: Remove all segments that are not contributing.
 *  3: Follow each segment until it loops or terminates.
 *  4: Repeat for every `subject` fill.
 */

static int intersect(const float2 &P1,
                     const float2 &P2,
                     const float2 &Q1,
                     const float2 &Q2,
                     float *r_alpha_P,
                     float *r_alpha_Q)
{
  double r_lambda;
  double r_mu;
  const int val = isect_seg_seg_v2_lambda_mu_db(
      double2(P1), double2(P2), double2(Q1), double2(Q2), &r_lambda, &r_mu);

  *r_alpha_P = r_lambda;
  *r_alpha_Q = r_mu;

  return val;
}

struct BooleanResult {
  Vector<Segment> segments;
  Vector<bool> segment_reversed;
  Vector<int> segment_offsets;
  Vector<bool> cyclic;
  Vector<int> subj_first_curves;
  Array<bool> unchanged_curves;
  Array<bool> is_segments_clipping;
  Array<int> dst_to_src_curves;

  void append_result(const BooleanResult &other_result, const int subj_first_curve)
  {
    if (other_result.segments.is_empty()) {
      return;
    }

    for (const int i : other_result.segment_offsets.index_range().drop_front(1)) {
      segment_offsets.append(other_result.segment_offsets[i] + segments.size());
    }
    cyclic.extend(other_result.cyclic);
    segment_reversed.extend(other_result.segment_reversed);
    subj_first_curves.append_n_times(subj_first_curve, other_result.cyclic.size());

    for (const int i : other_result.segments.index_range()) {
      segments.append(std::move(other_result.segments[i]));
    }
  }
};

static void find_intersections_between_curves(const Span<float2> points_i,
                                              const Span<float2> points_j,
                                              const int curve_i,
                                              const int curve_j,
                                              const bool cyclic_i,
                                              const bool cyclic_j,
                                              const int point_offset_i,
                                              const int point_offset_j,
                                              Array<Vector<int>> &r_inters_per_curves,
                                              Vector<IntersectionPoint> &r_intersections)
{
  for (const int i : points_i.index_range().drop_back(cyclic_i ? 0 : 1)) {
    for (const int j : points_j.index_range().drop_back(cyclic_j ? 0 : 1)) {
      float alpha_a, alpha_b;
      const int val = intersect(points_i[i],
                                points_i[(i + 1) % points_i.size()],
                                points_j[j],
                                points_j[(j + 1) % points_j.size()],
                                &alpha_a,
                                &alpha_b);
      if (val == ISECT_LINE_LINE_CROSS || val == ISECT_LINE_LINE_EXACT) {
        r_inters_per_curves[curve_i].append(r_intersections.size());
        r_inters_per_curves[curve_j].append(r_intersections.size());
        r_intersections.append(create_intersection(
            i + point_offset_i, j + point_offset_j, alpha_a, alpha_b, curve_i, curve_j));
      }
    }
  }
}

/* TODO: This method of finding intersections is O(N^2) and should replaced with something faster.
 * This should be optimized with a BVH. */
static void find_intersections_between_shapes(const Span<float2> points,
                                              const GroupedSpan<int> shapes,
                                              const IndexMask &shapes_i,
                                              const IndexMask &shapes_j,
                                              const OffsetIndices<int> points_by_curve,
                                              const VArray<bool> &cyclic,
                                              const bool self_intersection,
                                              Array<Vector<int>> &r_inters_per_curves,
                                              Vector<IntersectionPoint> &r_intersections)
{
  shapes_i.foreach_index([&](const int shape_i) {
    const Span<int> curves_i = shapes[shape_i];
    for (const int curve_i : curves_i) {

      const IndexRange points_i = points_by_curve[curve_i];
      const bool cyclic_i = cyclic[curve_i];

      shapes_j.foreach_index([&](const int shape_j) {
        const Span<int> curves_j = shapes[shape_j];
        for (const int curve_j : curves_j) {
          if (self_intersection && curve_i >= curve_j) {
            return;
          }

          const IndexRange points_j = points_by_curve[curve_j];
          const bool cyclic_j = cyclic[curve_j];

          find_intersections_between_curves(points.slice(points_i),
                                            points.slice(points_j),
                                            curve_i,
                                            curve_j,
                                            cyclic_i,
                                            cyclic_j,
                                            points_i.first(),
                                            points_j.first(),
                                            r_inters_per_curves,
                                            r_intersections);
        }
      });
    }
  });
}

static void create_connections_from_intersection_points(
    const Span<IntersectionPoint> &intersections,
    const Span<bool> &segments_to_keep,
    MutableSpan<SegmentConnections> segment_connections)
{
  auto connect = [&](const EncodedConnection point_1, const EncodedConnection point_2) {
    segment_connections[decode_index(point_1)][decode_side(point_1)] = encode_index_and_side(
        decode_index(point_2), decode_side(point_2));
    segment_connections[decode_index(point_2)][decode_side(point_2)] = encode_index_and_side(
        decode_index(point_1), decode_side(point_1));
  };

  for (const int inter_id : intersections.index_range()) {
    const IntersectionPoint &inter = intersections[inter_id];

    const EncodedConnection start_a = encode_index_and_side(inter.segment_index_i[Side::Start],
                                                            Side::End);
    const EncodedConnection end_a = encode_index_and_side(inter.segment_index_i[Side::End],
                                                          Side::Start);
    const EncodedConnection start_b = encode_index_and_side(inter.segment_index_j[Side::Start],
                                                            Side::End);
    const EncodedConnection end_b = encode_index_and_side(inter.segment_index_j[Side::End],
                                                          Side::Start);
    const bool is_start_a = start_a == SEGMENT_CONNECTION_NULL ?
                                false :
                                segments_to_keep[decode_index(start_a)];
    const bool is_end_a = end_a == SEGMENT_CONNECTION_NULL ? false :
                                                             segments_to_keep[decode_index(end_a)];
    const bool is_start_b = start_b == SEGMENT_CONNECTION_NULL ?
                                false :
                                segments_to_keep[decode_index(start_b)];
    const bool is_end_b = end_b == SEGMENT_CONNECTION_NULL ? false :
                                                             segments_to_keep[decode_index(end_b)];

    if (is_start_a && is_end_a && is_start_b && is_end_b) {
      connect(start_a, end_a);
      connect(start_b, end_b);
    }
    else if (is_start_a && is_end_a && is_start_b && !is_end_b) {
      connect(start_a, end_a);
    }
    else if (is_start_a && is_end_a && !is_start_b && is_end_b) {
      connect(start_a, end_a);
    }
    else if (is_start_a && is_end_a && !is_start_b && !is_end_b) {
      connect(start_a, end_a);
    }
    else if (is_start_a && !is_end_a && is_start_b && is_end_b) {
      connect(start_b, end_b);
    }
    else if (is_start_a && !is_end_a && is_start_b && !is_end_b) {
      connect(start_a, start_b);
    }
    else if (is_start_a && !is_end_a && !is_start_b && is_end_b) {
      connect(start_a, end_b);
    }
    else if (is_start_a && !is_end_a && !is_start_b && !is_end_b) {
      /* Pass. */
    }
    else if (!is_start_a && is_end_a && is_start_b && is_end_b) {
      connect(start_b, end_b);
    }
    else if (!is_start_a && is_end_a && is_start_b && !is_end_b) {
      connect(end_a, start_b);
    }
    else if (!is_start_a && is_end_a && !is_start_b && is_end_b) {
      connect(end_a, end_b);
    }
    else if (!is_start_a && is_end_a && !is_start_b && !is_end_b) {
      /* Pass. */
    }
    else if (!is_start_a && !is_end_a && is_start_b && is_end_b) {
      connect(start_b, end_b);
    }
    else if (!is_start_a && !is_end_a && is_start_b && !is_end_b) {
      /* Pass. */
    }
    else if (!is_start_a && !is_end_a && !is_start_b && is_end_b) {
      /* Pass. */
    }
    else if (!is_start_a && !is_end_a && !is_start_b && !is_end_b) {
      /* Pass. */
    }
  }
}

static BooleanResult execute_single_boolean(const CurveBooleanOpParameters op_params,
                                            const int subj_shape_id,
                                            const Span<float2> points,
                                            const GroupedSpan<int> shapes,
                                            const OffsetIndices<int> points_by_curve,
                                            const IndexMask &clipping_shapes,
                                            const VArray<bool> &is_fill,
                                            const VArray<bool> &cyclic)
{
  Vector<IntersectionPoint> intersections;

  Array<Vector<int>> inters_per_curves(points_by_curve.size());

  const IndexMask subj_shapes = IndexRange::from_single(subj_shape_id);

  find_intersections_between_shapes(points,
                                    shapes,
                                    subj_shapes,
                                    clipping_shapes,
                                    points_by_curve,
                                    cyclic,
                                    false,
                                    inters_per_curves,
                                    intersections);

  Vector<Segment> all_segments;
  Array<int> all_segment_offset_data(points_by_curve.size() + 1, 0);

  for (const int curve_i : shapes[subj_shape_id]) {
    all_segment_offset_data[curve_i] = create_segments_from_intersections_single_curve(
        curve_i, inters_per_curves, points_by_curve, intersections, cyclic, all_segments);
  }

  clipping_shapes.foreach_index([&](const int clip_shape_i) {
    const Span<int> curves_j = shapes[clip_shape_i];
    for (const int curve_j : curves_j) {
      all_segment_offset_data[curve_j] = create_segments_from_intersections_single_curve(
          curve_j, inters_per_curves, points_by_curve, intersections, cyclic, all_segments);
    }
  });

  store_segment_map_on_intersections(all_segments, intersections);
  const OffsetIndices<int> segments_by_curve = offset_indices::accumulate_counts_to_offsets(
      all_segment_offset_data);

  Array<bool> all_inside_left(all_segments.size());
  Array<bool> all_inside_right(all_segments.size());

  for (const int curve_i : shapes[subj_shape_id]) {
    get_inside_states_for_segments(op_params,
                                   curve_i,
                                   true,
                                   subj_shape_id,
                                   points,
                                   shapes,
                                   points_by_curve,
                                   clipping_shapes,
                                   all_segments,
                                   segments_by_curve,
                                   is_fill,
                                   all_inside_left,
                                   all_inside_right);
  }
  clipping_shapes.foreach_index([&](const int clip_shape_id) {
    const Span<int> curves_j = shapes[clip_shape_id];
    for (const int curve_j : curves_j) {
      get_inside_states_for_segments(op_params,
                                     curve_j,
                                     false,
                                     subj_shape_id,
                                     points,
                                     shapes,
                                     points_by_curve,
                                     clipping_shapes,
                                     all_segments,
                                     segments_by_curve,
                                     is_fill,
                                     all_inside_left,
                                     all_inside_right);
    }
  });

  Array<bool> segments_to_keep(all_segments.size(), true);
  for (const int segment_i : all_segments.index_range()) {
    const Segment &segment = all_segments[segment_i];

    if (is_fill[segment.curve]) {
      if (!all_inside_left[segment_i] ^ all_inside_right[segment_i]) {
        segments_to_keep[segment_i] = false;
      }
    }
    else {
      BLI_assert(all_inside_left[segment_i] == all_inside_right[segment_i]);
      if (all_inside_left[segment_i]) {
        segments_to_keep[segment_i] = false;
      }
    }
  }

  Array<SegmentConnections> segment_connections(all_segments.size(),
                                                SegmentConnections(SEGMENT_CONNECTION_NULL));
  create_connections_from_intersection_points(
      intersections, segments_to_keep, segment_connections);

  BooleanResult result;
  follow_segment_connections(all_segments,
                             segments_to_keep,
                             segment_connections,
                             result.segments,
                             result.segment_offsets,
                             result.segment_reversed,
                             result.cyclic);

  return result;
}

static Array<bool> calculate_unchanged_curves(const Span<Segment> segments,
                                              const Span<bool> is_segments_clipping,
                                              const OffsetIndices<int> segment_offsets)
{
  Array<bool> unchanged_curves(segment_offsets.size(), false);

  for (const int dst_curve_i : segment_offsets.index_range()) {
    const IndexRange segment_range = segment_offsets[dst_curve_i];
    if (segment_range.size() != 1) {
      continue;
    }

    const int segment_i = segment_range.first();
    const Segment &segment = segments[segment_i];
    if (segment.has_intersection(Side::Start) || segment.has_intersection(Side::End)) {
      continue;
    }

    if (is_segments_clipping[segment_i]) {
      continue;
    }

    unchanged_curves[dst_curve_i] = true;
  }

  return unchanged_curves;
}

static BooleanResult execute_boolean(const CurveBooleanOpParameters op_params,
                                     const Span<float2> positions_2d,
                                     const OffsetIndices<int> points_by_curve,
                                     const IndexMask &clipping_shapes,
                                     const GroupedSpan<int> shapes,
                                     const VArray<int> &fill_ids,
                                     const VArray<bool> &is_cyclic)
{
  BooleanResult results_all;
  results_all.segment_offsets.append(0);

  if (points_by_curve.is_empty()) {
    return results_all;
  }

  IndexMaskMemory memory;
  const IndexMask subject_shapes = clipping_shapes.complement(shapes.index_range(), memory);

  /* Treat filled curves as cyclical. */
  const VArray<bool> cyclic = VArray<bool>::from_func(points_by_curve.size(), [&](int64_t index) {
    return is_cyclic[index] || (fill_ids[index] != 0);
  });

  const VArray<bool> is_fill = VArray<bool>::from_func(points_by_curve.size(), [&](int64_t index) {
    const IndexRange points = points_by_curve[index];

    if (points.size() <= 2) {
      return false;
    }

    return fill_ids[index] != 0;
  });

  Array<Bounds<float2>> screen_space_bbox(shapes.size());
  compute_bounding_boxes(points_by_curve, shapes, positions_2d, screen_space_bbox);

  Array<bool> shapes_bbox_intersecting(shapes.size(), false);
  index_mask::masked_fill(shapes_bbox_intersecting.as_mutable_span(), true, clipping_shapes);

  /* Calculate which bounding boxes intersect the clipping shapes. */
  subject_shapes.foreach_index([&](const int64_t shape_i) {
    const Bounds<float2> bbox_i = screen_space_bbox[shape_i];

    bool box_intersects = false;

    clipping_shapes.foreach_index([&](const int64_t shape_j) {
      const Bounds<float2> bbox_j = screen_space_bbox[shape_j];

      if (bounds::intersect(bbox_i, bbox_j).has_value()) {
        box_intersects = true;
      }
    });

    if (box_intersects) {
      shapes_bbox_intersecting[shape_i] = true;
    }
  });

  subject_shapes.foreach_index([&](const int64_t shape_i) {
    /* Only do full calculation with the bounding boxes are intersecting. */
    if (shapes_bbox_intersecting[shape_i]) {
      const BooleanResult result = execute_single_boolean(op_params,
                                                          shape_i,
                                                          positions_2d,
                                                          shapes,
                                                          points_by_curve,
                                                          clipping_shapes,
                                                          is_fill,
                                                          cyclic);

      const int subj_first_curve = shapes[shape_i].first();
      results_all.append_result(result, subj_first_curve);
    }
    else {
      BooleanResult result;
      result.segment_offsets.append(0);

      const Span<int> shape = shapes[shape_i];
      for (const int pos_j : shape.index_range()) {
        const int curve_j = shape[pos_j];

        result.segments.append(
            Segment::from_curve(curve_j, points_by_curve[curve_j], is_cyclic[curve_j]));
        result.cyclic.append(is_cyclic[curve_j]);
        result.segment_offsets.append(pos_j + 1);
        result.segment_reversed.append(false);
      }

      const int subj_first_curve = shape.first();
      results_all.append_result(result, subj_first_curve);
    }
  });

  if (results_all.segments.is_empty()) {
    return results_all;
  }

  const OffsetIndices<int> dst_segments_by_curve = OffsetIndices<int>(results_all.segment_offsets);

  Array<bool> is_src_curve_clipping(points_by_curve.size(), false);
  for (const int curve_i : points_by_curve.index_range()) {
    if (curve_i == points_by_curve.index_range().last()) {
      is_src_curve_clipping[curve_i] = true;
    }
  }

  results_all.is_segments_clipping = Array<bool>(results_all.segments.size());
  for (const int seg_i : results_all.segments.index_range()) {
    const Segment &segment = results_all.segments[seg_i];
    results_all.is_segments_clipping[seg_i] = is_src_curve_clipping[segment.curve];
  }

  results_all.dst_to_src_curves = Array<int>(dst_segments_by_curve.size());
  for (const int i : dst_segments_by_curve.index_range()) {
    const IndexRange segment_range = dst_segments_by_curve[i];
    const int subj_first_curve = results_all.subj_first_curves[i];
    results_all.dst_to_src_curves[i] = subj_first_curve;

    /* Prioritize non-clipping curves. */
    for (const int seg_i : segment_range) {
      const int curve_i = results_all.segments[seg_i].curve;
      if (!is_src_curve_clipping[curve_i]) {
        results_all.dst_to_src_curves[i] = curve_i;
        break;
      }
    }
  }

  results_all.unchanged_curves = calculate_unchanged_curves(
      results_all.segments, results_all.is_segments_clipping, dst_segments_by_curve);

  return results_all;
}

static bke::CurvesGeometry create_curves_from_segments(const bke::CurvesGeometry &src,
                                                       const Span<Segment> segments,
                                                       const Span<bool> segment_reversed,
                                                       const Span<bool> cyclic,
                                                       const Span<bool> is_segments_clipping,
                                                       const Span<int> dst_to_src_curves,
                                                       const OffsetIndices<int> segment_offsets,
                                                       const Span<bool> unchanged_curves,
                                                       const bool skip_clipping_attributes,
                                                       Vector<bool> &r_is_point_clipping)
{
  IndexMaskMemory memory;
  const IndexMask unchanged_curves_mask = IndexMask::from_bools(unchanged_curves, memory);

  struct InterpolatePoint {
    int dst_point;
    int src_point_1;
    int src_point_2;
    float factor;
  };

  Array<int> point_offsets(segment_offsets.size() + 1);
  Vector<int2> points_to_copy;
  Vector<int2> clipping_points_to_copy;
  Vector<IndexRange> ranges_to_reverse;
  Vector<InterpolatePoint> point_to_interpolate;

  int i = 0;
  for (const int curve_i : segment_offsets.index_range()) {
    point_offsets[curve_i] = i;

    const bool unchanged = unchanged_curves[curve_i];

    const IndexRange segment_range = segment_offsets[curve_i];
    for (const int seg_i : segment_range) {
      const Segment &segment = segments[seg_i];
      const bool reversed = segment_reversed[seg_i];
      const bool is_clipping = is_segments_clipping[seg_i];
      const int point_num = segment.points_num();

      if (segment.has_intersection(reversed ? Side::End : Side::Start) && !segment.is_loop()) {
        const float start_alpha = segment.intersection_factor[reversed ? Side::End : Side::Start];
        const int2 start_edge = segment.edge(reversed ? Side::End : Side::Start);
        point_to_interpolate.append({i, start_edge.x, start_edge.y, start_alpha});
        r_is_point_clipping.append(is_clipping);
        i++;
      }

      r_is_point_clipping.append_n_times(is_clipping, point_num);

      if (!unchanged && !is_clipping) {
        segment.foreach_point(
            [&](const int index, const int pos) { points_to_copy.append(int2(pos + i, index)); });

        if (reversed) {
          ranges_to_reverse.append(IndexRange::from_begin_size(i, point_num));
        }
      }
      if (is_clipping) {
        if (reversed) {
          segment.foreach_point([&](const int index, const int pos) {
            clipping_points_to_copy.append(int2(point_num - 1 - pos + i, index));
          });
        }
        else {
          segment.foreach_point([&](const int index, const int pos) {
            clipping_points_to_copy.append(int2(pos + i, index));
          });
        }
      }

      i += point_num;

      if (seg_i == segment_range.last() &&
          segment.has_intersection(reversed ? Side::Start : Side::End) && !cyclic[curve_i])
      {
        const float end_alpha = segment.intersection_factor[reversed ? Side::Start : Side::End];
        const int2 end_edge = segment.edge(reversed ? Side::Start : Side::End);
        point_to_interpolate.append({i, end_edge.x, end_edge.y, end_alpha});
        r_is_point_clipping.append(is_clipping);
        i++;
      }
    }
  }
  point_offsets.last() = i;

  const OffsetIndices<int> dst_points_by_curve = OffsetIndices<int>(point_offsets);

  const bke::AttributeAccessor src_attributes = src.attributes();
  const VArray<bool> src_cyclic = src.cyclic();
  const OffsetIndices<int> src_points_by_curve = src.points_by_curve();
  bke::CurvesGeometry dst_curves(dst_points_by_curve.total_size(), dst_points_by_curve.size());
  bke::MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();

  dst_curves.offsets_for_write().copy_from(dst_points_by_curve.data());

  Vector<InterpolatePoint> clipping_point_to_interpolate;
  for (const int curve_i : dst_points_by_curve.index_range()) {
    const IndexRange points = dst_points_by_curve[curve_i];
    const Span<bool> range_is_clipping = r_is_point_clipping.as_span().slice(points);

    const IndexMask clipping_ranges = IndexMask::from_bools(range_is_clipping, memory);

    /* If the curve is entirely clipping, then take the point data from some point of the source
     * curve. */
    if (clipping_ranges == points.index_range()) {
      const int src_curve_i = dst_to_src_curves[curve_i];
      const IndexRange src_points = src_points_by_curve[src_curve_i];

      /* Take the attribute data from the middle point of the source curve. */
      const int mid_point_index = int((src_points.first() + src_points.last()) / 2);

      for (const int point_i : points) {
        clipping_point_to_interpolate.append(
            {int(point_i), mid_point_index, mid_point_index, 0.0f});
      }
      continue;
    }

    clipping_ranges.foreach_range([&](const IndexRange &range) {
      const IndexRange full_range = range.shift(points.first());
      int range_first = int(full_range.first() - 1);
      int range_last = int(full_range.last() + 1);
      if (range_first == points.first() - 1) {
        range_first = points.last();
      }
      if (range_last == points.last() + 1) {
        range_last = points.first();
      }
      for (const int i : range.index_range()) {
        const float t = (i + 1.0f) / (range.size() + 1.0f);
        clipping_point_to_interpolate.append({int(full_range[i]), range_first, range_last, t});
      }
    });
  }

  bke::gather_attributes(src_attributes,
                         bke::AttrDomain::Curve,
                         bke::AttrDomain::Curve,
                         bke::attribute_filter_from_skip_ref({"cyclic"}),
                         dst_to_src_curves,
                         dst_attributes);

  MutableSpan<bool> dst_cyclic = dst_curves.cyclic_for_write();
  dst_cyclic.copy_from(cyclic);
  unchanged_curves_mask.foreach_index(
      [&](const int i) { dst_cyclic[i] = src_cyclic[dst_to_src_curves[i]]; });

  /* Copy/Interpolate point attributes. */
  for (auto &attribute : bke::retrieve_attributes_for_transfer(
           src_attributes, dst_attributes, {bke::AttrDomain::Point}, {}))
  {
    bke::attribute_math::to_static_type(attribute.dst.span.type(), [&]<typename T>() {
      auto src_attr = attribute.src.typed<T>();
      auto dst_attr = attribute.dst.span.typed<T>();

      for (const InterpolatePoint &interpolate_point : point_to_interpolate) {
        dst_attr[interpolate_point.dst_point] = bke::attribute_math::mix2<T>(
            interpolate_point.factor,
            src_attr[interpolate_point.src_point_1],
            src_attr[interpolate_point.src_point_2]);
      }
      for (const int2 index : points_to_copy) {
        dst_attr[index.x] = src_attr[index.y];
      }
      for (const int2 index : clipping_points_to_copy) {
        dst_attr[index.x] = src_attr[index.y];
      }
      for (const IndexRange range : ranges_to_reverse) {
        dst_attr.slice(range).reverse();
      }
    });

    attribute.dst.finish();
  }

  if (skip_clipping_attributes) {
    src_attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
      if (iter.domain != bke::AttrDomain::Point) {
        return;
      }
      if (iter.data_type == bke::AttrType::String) {
        return;
      }
      const GVArraySpan src = *iter.get(bke::AttrDomain::Point);
      bke::GSpanAttributeWriter dst = dst_attributes.lookup_or_add_for_write_only_span(
          iter.name, bke::AttrDomain::Point, iter.data_type);
      if (!dst) {
        return;
      }
      unchanged_curves_mask.foreach_index([&](const int i) {
        dst.span.slice(dst_points_by_curve[i])
            .copy_from(src.slice(src_points_by_curve[dst_to_src_curves[i]]));
      });

      if (iter.name != "position") {
        GMutableSpan attribute_data = dst.span;
        bke::attribute_math::to_static_type(attribute_data.type(), [&]<typename T>() {
          MutableSpan<T> span_data = attribute_data.typed<T>();

          for (const InterpolatePoint &interpolate_point : clipping_point_to_interpolate) {
            span_data[interpolate_point.dst_point] = bke::attribute_math::mix2<T>(
                interpolate_point.factor,
                span_data[interpolate_point.src_point_1],
                span_data[interpolate_point.src_point_2]);
          }
        });
      }

      dst.finish();
    });
  }

  return dst_curves;
}

static float4 transform_plane(const float4x4 &mat, const float4 &plane)
{
  float3 normal = float3(plane);
  float3 point = -normal * plane.w;

  normal = math::transform_direction(mat, normal);
  point = math::transform_point(mat, point);

  return float4(normal, -math::dot(normal, point));
}

bke::CurvesGeometry curve_boolean(const CurveBooleanOpParameters op_params,
                                  const bke::CurvesGeometry &curves,
                                  const ProjectionFunc project_fn,
                                  const GroupedSpan<int> shapes,
                                  const IndexMask &clipping_shapes)
{
  Array<float2> src_positions_2d(curves.points_num());
  const Span<float3> positions = curves.positions();
  for (const int i : curves.points_range()) {
    src_positions_2d[i] = project_fn(positions[i]);
  }

  const VArray<int> fill_ids = *curves.attributes().lookup<int>("fill_id", bke::AttrDomain::Curve);

  const BooleanResult result = execute_boolean(op_params,
                                               src_positions_2d,
                                               curves.points_by_curve(),
                                               clipping_shapes,
                                               shapes,
                                               fill_ids,
                                               curves.cyclic());

  if (result.segments.is_empty()) {
    return bke::CurvesGeometry();
  }

  const OffsetIndices<int> dst_segments_by_curve = OffsetIndices<int>(result.segment_offsets);

  Vector<bool> is_point_clipping;
  bke::CurvesGeometry dst_curves = create_curves_from_segments(curves,
                                                               result.segments,
                                                               result.segment_reversed,
                                                               result.cyclic,
                                                               result.is_segments_clipping,
                                                               result.dst_to_src_curves,
                                                               dst_segments_by_curve,
                                                               result.unchanged_curves,
                                                               op_params.skip_clipping_attributes,
                                                               is_point_clipping);

  if (!op_params.keep_caps) {
    cut_caps(dst_curves,
             result.segments,
             result.segment_reversed,
             result.cyclic,
             dst_segments_by_curve);
  }

  if (op_params.separate_islands) {
    separate_fill_islands(dst_curves, result.unchanged_curves, project_fn);
  }

  return dst_curves;
}

bke::CurvesGeometry curve_boolean_with_planes(const CurveBooleanOpParameters op_params,
                                              const bke::CurvesGeometry &curves,
                                              const ProjectionFunc project_fn,
                                              const GroupedSpan<int> shapes,
                                              const Span<float4> curve_planes,
                                              const IndexMask &clipping_shapes,
                                              const float4x4 &layer_to_world,
                                              const ARegion &region)
{
  Array<float2> src_positions_2d(curves.points_num());
  const Span<float3> src_positions = curves.positions();
  for (const int i : curves.points_range()) {
    src_positions_2d[i] = project_fn(src_positions[i]);
  }

  const VArray<int> fill_ids = *curves.attributes().lookup<int>("fill_id", bke::AttrDomain::Curve);

  const BooleanResult result = execute_boolean(op_params,
                                               src_positions_2d,
                                               curves.points_by_curve(),
                                               clipping_shapes,
                                               shapes,
                                               fill_ids,
                                               curves.cyclic());

  if (result.segments.is_empty()) {
    return bke::CurvesGeometry();
  }

  const OffsetIndices<int> dst_segments_by_curve = OffsetIndices<int>(result.segment_offsets);

  Vector<bool> is_point_clipping;
  bke::CurvesGeometry dst_curves = create_curves_from_segments(curves,
                                                               result.segments,
                                                               result.segment_reversed,
                                                               result.cyclic,
                                                               result.is_segments_clipping,
                                                               result.dst_to_src_curves,
                                                               dst_segments_by_curve,
                                                               result.unchanged_curves,
                                                               op_params.skip_clipping_attributes,
                                                               is_point_clipping);

  const OffsetIndices<int> points_by_curve = dst_curves.points_by_curve();
  MutableSpan<float3> positions = dst_curves.positions_for_write();
  const float4x4 world_to_layer = math::invert(layer_to_world);
  for (const int curve_i : dst_curves.curves_range()) {
    const int src_curve = result.dst_to_src_curves[curve_i];
    const float4 &plane = transform_plane(layer_to_world, curve_planes[src_curve]);
    const IndexRange points = points_by_curve[curve_i];

    for (const int point_i : points) {
      if (is_point_clipping[point_i]) {
        const float2 positions_2d = project_fn(positions[point_i]);
        ED_view3d_win_to_3d_on_plane(&region, plane, positions_2d, false, positions[point_i]);
        positions[point_i] = math::transform_point(world_to_layer, positions[point_i]);
      }
    }
  }

  if (!op_params.keep_caps) {
    cut_caps(dst_curves,
             result.segments,
             result.segment_reversed,
             result.cyclic,
             dst_segments_by_curve);
  }

  if (op_params.separate_islands) {
    separate_fill_islands(dst_curves, result.unchanged_curves, project_fn);
  }

  return dst_curves;
}

}  // namespace blender::ed::greasepencil::boolean
