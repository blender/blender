/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include <algorithm>

#include "BLI_array_utils.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_kdopbvh.hh"
#include "BLI_kdtree.h"
#include "BLI_math_vector.hh"
#include "BLI_offset_indices.hh"
#include "BLI_rect.h"
#include "BLI_stack.hh"
#include "BLI_task.hh"

#include "BKE_attribute.hh"
#include "BKE_curves.hh"
#include "BKE_curves_utils.hh"
#include "BKE_grease_pencil.hh"

#include "DNA_curves_types.h"
#include "DNA_gpencil_legacy_types.h"

#include "ED_curves.hh"
#include "ED_grease_pencil.hh"
#include "ED_view3d.hh"

#include "GEO_merge_curves.hh"

extern "C" {
#include "curve_fit_nd.h"
}

namespace blender::ed::greasepencil {

int64_t ramer_douglas_peucker_simplify(
    const IndexRange range,
    const float epsilon,
    const FunctionRef<float(int64_t, int64_t, int64_t)> dist_function,
    MutableSpan<bool> points_to_delete)
{
  /* Mark all points to not be removed. */
  points_to_delete.slice(range).fill(false);
  int64_t total_points_to_remove = 0;

  Stack<IndexRange> stack;
  stack.push(range);
  while (!stack.is_empty()) {
    const IndexRange sub_range = stack.pop();
    /* Skip ranges with less than 3 points. All points are kept. */
    if (sub_range.size() < 3) {
      continue;
    }
    const IndexRange inside_range = sub_range.drop_front(1).drop_back(1);
    /* Compute the maximum distance and the corresponding index. */
    float max_dist = -1.0f;
    int max_index = -1;
    for (const int64_t index : inside_range) {
      const float dist = dist_function(sub_range.first(), sub_range.last(), index);
      if (dist > max_dist) {
        max_dist = dist;
        max_index = index - sub_range.first();
      }
    }

    if (max_dist > epsilon) {
      /* Found point outside the epsilon-sized strip. The point at `max_index` will be kept, repeat
       * the search on the left & right side. */
      stack.push(sub_range.slice(0, max_index + 1));
      stack.push(sub_range.slice(max_index, sub_range.size() - max_index));
    }
    else {
      /* Points in `sub_range` are inside the epsilon-sized strip. Mark them to be deleted. */
      total_points_to_remove += inside_range.size();
      points_to_delete.slice(inside_range).fill(true);
    }
  }
  return total_points_to_remove;
}

Array<float2> polyline_fit_curve(Span<float2> points,
                                 const float error_threshold,
                                 const IndexMask &corner_mask)
{
  if (points.is_empty()) {
    return {};
  }
  double total_length = 0.0;
  for (const int point_i : points.index_range().drop_front(1)) {
    total_length += math::distance(points[point_i - 1], points[point_i]);
  }
  /* Just return a dot. */
  if (total_length < 1e-8) {
    return Array<float2>({points[0], points[0], points[0]});
  }

  Array<int32_t> indices(corner_mask.size());
  corner_mask.to_indices(indices.as_mutable_span());
  uint *indicies_ptr = corner_mask.is_empty() ? nullptr : reinterpret_cast<uint *>(indices.data());

  float *cubic_array;
  uint cubic_array_len;
  int error = curve_fit_cubic_to_points_fl(*points.data(),
                                           points.size(),
                                           2,
                                           error_threshold,
                                           CURVE_FIT_CALC_HIGH_QUALIY,
                                           indicies_ptr,
                                           indices.size(),
                                           &cubic_array,
                                           &cubic_array_len,
                                           nullptr,
                                           nullptr,
                                           nullptr);

  if (error != 0) {
    /* Some error occurred. Return. */
    return {};
  }

  if (cubic_array == nullptr) {
    return {};
  }

  Span<float2> cubic_array_span(reinterpret_cast<float2 *>(cubic_array), cubic_array_len * 3);
  Array<float2> curve_positions(cubic_array_span);
  /* Free the c-style array. */
  free(cubic_array);
  return curve_positions;
}

IndexMask polyline_detect_corners(Span<float2> points,
                                  const float radius_min,
                                  const float radius_max,
                                  const int samples_max,
                                  const float angle_threshold,
                                  IndexMaskMemory &memory)
{
  if (points.is_empty()) {
    return {};
  }
  if (points.size() == 1) {
    return IndexMask::from_indices<int>({0}, memory);
  }
  uint *corners;
  uint corners_len;
  const int error = curve_fit_corners_detect_fl(*points.data(),
                                                points.size(),
                                                float2::type_length,
                                                radius_min,
                                                radius_max,
                                                samples_max,
                                                angle_threshold,
                                                &corners,
                                                &corners_len);
  if (error != 0) {
    /* Error occurred, return. */
    return IndexMask();
  }

  if (corners == nullptr) {
    return IndexMask();
  }

  BLI_assert(samples_max < std::numeric_limits<int>::max());
  Span<int> indices(reinterpret_cast<int *>(corners), corners_len);
  const IndexMask corner_mask = IndexMask::from_indices<int>(indices, memory);
  /* Free the c-style array. */
  free(corners);
  return corner_mask;
}

int curve_merge_by_distance(const IndexRange points,
                            const Span<float> distances,
                            const IndexMask &selection,
                            const float merge_distance,
                            MutableSpan<int> r_merge_indices)
{
  /* We use a KDTree_1d here, because we can only merge neighboring points in the curves. */
  KDTree_1d *tree = BLI_kdtree_1d_new(selection.size());
  /* The selection is an IndexMask of the points just in this curve. */
  selection.foreach_index_optimized<int64_t>([&](const int64_t i, const int64_t pos) {
    BLI_kdtree_1d_insert(tree, pos, &distances[i - points.first()]);
  });
  BLI_kdtree_1d_balance(tree);

  Array<int> selection_merge_indices(selection.size(), -1);
  const int duplicate_count = BLI_kdtree_1d_calc_duplicates_fast(
      tree, merge_distance, false, selection_merge_indices.data());
  BLI_kdtree_1d_free(tree);

  array_utils::fill_index_range<int>(r_merge_indices);

  selection.foreach_index([&](const int src_index, const int pos) {
    const int merge_index = selection_merge_indices[pos];
    if (merge_index != -1) {
      const int src_merge_index = selection[merge_index] - points.first();
      r_merge_indices[src_index - points.first()] = src_merge_index;
    }
  });

  return duplicate_count;
}

blender::bke::CurvesGeometry curves_merge_by_distance(const bke::CurvesGeometry &src_curves,
                                                      const float merge_distance,
                                                      const IndexMask &selection,
                                                      const bke::AttributeFilter &attribute_filter)
{
  /* NOTE: The code here is an adapted version of #blender::geometry::point_merge_by_distance. */

  const int src_point_size = src_curves.points_num();
  if (src_point_size == 0) {
    return {};
  }
  const OffsetIndices<int> points_by_curve = src_curves.points_by_curve();
  const VArray<bool> cyclic = src_curves.cyclic();
  src_curves.ensure_evaluated_lengths();

  bke::CurvesGeometry dst_curves = bke::curves::copy_only_curve_domain(src_curves);
  MutableSpan<int> dst_offsets = dst_curves.offsets_for_write();

  std::atomic<int> total_duplicate_count = 0;
  Array<Array<int>> merge_indices_per_curve(src_curves.curves_num());
  threading::parallel_for(src_curves.curves_range(), 512, [&](const IndexRange range) {
    for (const int curve_i : range) {
      const IndexRange points = points_by_curve[curve_i];
      merge_indices_per_curve[curve_i].reinitialize(points.size());

      Array<float> distances_along_curve(points.size() + int(cyclic[curve_i]));
      distances_along_curve.first() = 0.0f;
      const Span<float> lengths = src_curves.evaluated_lengths_for_curve(curve_i, cyclic[curve_i]);
      distances_along_curve.as_mutable_span().drop_front(1).copy_from(lengths);

      MutableSpan<int> merge_indices = merge_indices_per_curve[curve_i].as_mutable_span();
      array_utils::fill_index_range<int>(merge_indices);

      const int duplicate_count = curve_merge_by_distance(points,
                                                          distances_along_curve,
                                                          selection.slice_content(points),
                                                          merge_distance,
                                                          merge_indices);
      /* Write the curve size. The counts will be accumulated to offsets below. */
      dst_offsets[curve_i] = points.size() - duplicate_count;
      total_duplicate_count += duplicate_count;
    }
  });

  const int dst_point_size = src_point_size - total_duplicate_count;
  dst_curves.resize(dst_point_size, src_curves.curves_num());
  offset_indices::accumulate_counts_to_offsets(dst_offsets);

  int merged_points = 0;
  Array<int> src_to_dst_indices(src_point_size);
  for (const int curve_i : src_curves.curves_range()) {
    const IndexRange points = points_by_curve[curve_i];
    const Span<int> merge_indices = merge_indices_per_curve[curve_i].as_span();
    for (const int i : points.index_range()) {
      const int point_i = points.start() + i;
      src_to_dst_indices[point_i] = point_i - merged_points;
      if (merge_indices[i] != i) {
        merged_points++;
      }
    }
  }

  Array<int> point_merge_counts(dst_point_size, 0);
  for (const int curve_i : src_curves.curves_range()) {
    const IndexRange points = points_by_curve[curve_i];
    const Span<int> merge_indices = merge_indices_per_curve[curve_i].as_span();
    for (const int i : points.index_range()) {
      const int merge_index = merge_indices[i];
      const int point_src = points.start() + merge_index;
      const int dst_index = src_to_dst_indices[point_src];
      point_merge_counts[dst_index]++;
    }
  }

  Array<int> map_offsets_data(dst_point_size + 1);
  map_offsets_data.as_mutable_span().drop_back(1).copy_from(point_merge_counts);
  OffsetIndices<int> map_offsets = offset_indices::accumulate_counts_to_offsets(map_offsets_data);

  point_merge_counts.fill(0);

  Array<int> merge_map_indices(src_point_size);
  for (const int curve_i : src_curves.curves_range()) {
    const IndexRange points = points_by_curve[curve_i];
    const Span<int> merge_indices = merge_indices_per_curve[curve_i].as_span();
    for (const int i : points.index_range()) {
      const int point_i = points.start() + i;
      const int merge_index = merge_indices[i];
      const int dst_index = src_to_dst_indices[points.start() + merge_index];
      merge_map_indices[map_offsets[dst_index].first() + point_merge_counts[dst_index]] = point_i;
      point_merge_counts[dst_index]++;
    }
  }

  bke::AttributeAccessor src_attributes = src_curves.attributes();
  bke::MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();
  src_attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    if (attribute_filter.allow_skip(iter.name)) {
      return;
    }
    if (iter.domain != bke::AttrDomain::Point) {
      return;
    }

    bke::GAttributeReader src_attribute = iter.get();
    bke::attribute_math::convert_to_static_type(src_attribute.varray.type(), [&](auto dummy) {
      using T = decltype(dummy);
      if constexpr (!std::is_void_v<bke::attribute_math::DefaultMixer<T>>) {
        bke::SpanAttributeWriter<T> dst_attribute =
            dst_attributes.lookup_or_add_for_write_only_span<T>(iter.name, bke::AttrDomain::Point);
        BLI_assert(dst_attribute);
        VArraySpan<T> src = src_attribute.varray.typed<T>();

        threading::parallel_for(dst_curves.points_range(), 1024, [&](IndexRange range) {
          for (const int dst_point_i : range) {
            /* Create a separate mixer for every point to avoid allocating temporary buffers
             * in the mixer the size of the result curves and to improve memory locality. */
            bke::attribute_math::DefaultMixer<T> mixer{dst_attribute.span.slice(dst_point_i, 1)};

            Span<int> src_merge_indices = merge_map_indices.as_span().slice(
                map_offsets[dst_point_i]);
            for (const int src_point_i : src_merge_indices) {
              mixer.mix_in(0, src[src_point_i]);
            }

            mixer.finalize();
          }
        });

        dst_attribute.finish();
      }
    });
  });

  if (dst_curves.nurbs_has_custom_knots()) {
    bke::curves::nurbs::update_custom_knot_modes(
        dst_curves.curves_range(), NURBS_KNOT_MODE_NORMAL, NURBS_KNOT_MODE_NORMAL, dst_curves);
  }
  return dst_curves;
}

bke::CurvesGeometry curves_merge_endpoints_by_distance(
    const ARegion &region,
    const bke::CurvesGeometry &src_curves,
    const float4x4 &layer_to_world,
    const float merge_distance,
    const IndexMask &selection,
    const bke::AttributeFilter &attribute_filter)
{
  const OffsetIndices src_points_by_curve = src_curves.points_by_curve();
  const Span<float3> src_positions = src_curves.positions();
  const float merge_distance_squared = merge_distance * merge_distance;

  Array<float2> screen_start_points(src_curves.curves_num());
  Array<float2> screen_end_points(src_curves.curves_num());
  const VArray<bool> cyclic = *src_curves.attributes().lookup_or_default<bool>(
      "cyclic", bke::AttrDomain::Curve, false);
  /* For comparing screen space positions use a 2D KDTree. Each curve adds 2 points. */
  KDTree_2d *tree = BLI_kdtree_2d_new(2 * src_curves.curves_num());

  threading::parallel_for(src_curves.curves_range(), 1024, [&](const IndexRange range) {
    for (const int src_i : range) {
      const IndexRange points = src_points_by_curve[src_i];
      const float3 start_pos = src_positions[points.first()];
      const float3 end_pos = src_positions[points.last()];
      const float3 start_world = math::transform_point(layer_to_world, start_pos);
      const float3 end_world = math::transform_point(layer_to_world, end_pos);

      ED_view3d_project_float_global(
          &region, start_world, screen_start_points[src_i], V3D_PROJ_TEST_NOP);
      ED_view3d_project_float_global(
          &region, end_world, screen_end_points[src_i], V3D_PROJ_TEST_NOP);
    }
  });
  /* Note: KDTree insertion is not thread-safe, don't parallelize this. */
  for (const int src_i : src_curves.curves_range()) {
    if (cyclic[src_i] == true) {
      continue;
    }
    BLI_kdtree_2d_insert(tree, src_i * 2, screen_start_points[src_i]);
    BLI_kdtree_2d_insert(tree, src_i * 2 + 1, screen_end_points[src_i]);
  }
  BLI_kdtree_2d_balance(tree);

  Array<int> connect_to_curve(src_curves.curves_num(), -1);
  Array<bool> flip_direction(src_curves.curves_num(), false);
  selection.foreach_index(GrainSize(512), [&](const int src_i) {
    const float2 &start_co = screen_start_points[src_i];
    const float2 &end_co = screen_end_points[src_i];
    /* Index of KDTree points so they can be ignored. */
    const int start_index = src_i * 2;
    const int end_index = src_i * 2 + 1;

    KDTreeNearest_2d nearest_start, nearest_end;
    const bool is_start_ok = (BLI_kdtree_2d_find_nearest_cb_cpp(
                                  tree,
                                  start_co,
                                  &nearest_start,
                                  [&](const int other, const float * /*co*/, const float dist_sq) {
                                    if (start_index == other || dist_sq > merge_distance_squared) {
                                      return 0;
                                    }
                                    return 1;
                                  }) != -1);
    const bool is_end_ok = (BLI_kdtree_2d_find_nearest_cb_cpp(
                                tree,
                                end_co,
                                &nearest_end,
                                [&](const int other, const float * /*co*/, const float dist_sq) {
                                  if (end_index == other || dist_sq > merge_distance_squared) {
                                    return 0;
                                  }
                                  return 1;
                                }) != -1);

    if (is_start_ok) {
      const int curve_index = nearest_start.index / 2;
      const bool is_end_point = bool(nearest_start.index % 2);
      if (connect_to_curve[curve_index] < 0) {
        connect_to_curve[curve_index] = src_i;
        flip_direction[curve_index] = !is_end_point;
      }
    }
    if (is_end_ok) {
      const int curve_index = nearest_end.index / 2;
      const bool is_end_point = bool(nearest_end.index % 2);
      if (connect_to_curve[src_i] < 0) {
        connect_to_curve[src_i] = curve_index;
        flip_direction[curve_index] = is_end_point;
      }
    }
  });
  BLI_kdtree_2d_free(tree);

  return geometry::curves_merge_endpoints(
      src_curves, connect_to_curve, flip_direction, attribute_filter);
}

/* Generate a full circle around a point. */
static void generate_circle_from_point(const float3 &pt,
                                       const float radius,
                                       const int corner_subdivisions,
                                       const int src_point_index,
                                       Vector<float3> &r_perimeter,
                                       Vector<int> &r_src_indices)
{
  /* Number of points is 2^(n+2) on a full circle (n=corner_subdivisions). */
  BLI_assert(corner_subdivisions >= 0);
  const int num_points = 1 << (corner_subdivisions + 2);
  const float delta_angle = 2 * M_PI / float(num_points);
  const float delta_cos = math::cos(delta_angle);
  const float delta_sin = math::sin(delta_angle);

  float3 vec = float3(radius, 0, 0);
  for ([[maybe_unused]] const int i : IndexRange(num_points)) {
    r_perimeter.append(pt + vec);
    r_src_indices.append(src_point_index);

    const float x = delta_cos * vec.x - delta_sin * vec.y;
    const float y = delta_sin * vec.x + delta_cos * vec.y;
    vec = float3(x, y, 0.0f);
  }
}

/* Generate points in an counter-clockwise arc between two directions. */
static void generate_arc_from_point_to_point(const float3 &from,
                                             const float3 &to,
                                             const float3 &center_pt,
                                             const int corner_subdivisions,
                                             const int src_point_index,
                                             Vector<float3> &r_perimeter,
                                             Vector<int> &r_src_indices)
{
  const float3 vec_from = from - center_pt;
  const float3 vec_to = to - center_pt;
  if (math::is_zero(vec_from) || math::is_zero(vec_to)) {
    r_perimeter.append(center_pt);
    r_src_indices.append(src_point_index);
    return;
  }

  const float cos_angle = math::dot(vec_from.xy(), vec_to.xy());
  const float sin_angle = vec_from.x * vec_to.y - vec_from.y * vec_to.x;
  /* Compute angle in range [0, 2pi) so that the rotation is always counter-clockwise. */
  const float angle = math::atan2(-sin_angle, -cos_angle) + M_PI;

  /* Number of points is 2^(n+1) + 1 on half a circle (n=corner_subdivisions)
   * so we multiply by (angle / pi) to get the right amount of
   * points to insert. */
  const int num_full = (1 << (corner_subdivisions + 1)) + 1;
  const int num_points = num_full * math::abs(angle) / M_PI;
  if (num_points < 2) {
    r_perimeter.append(center_pt + vec_from);
    r_src_indices.append(src_point_index);
    return;
  }
  const float delta_angle = angle / float(num_points - 1);
  const float delta_cos = math::cos(delta_angle);
  const float delta_sin = math::sin(delta_angle);

  float3 vec = vec_from;
  for ([[maybe_unused]] const int i : IndexRange(num_points)) {
    r_perimeter.append(center_pt + vec);
    r_src_indices.append(src_point_index);

    const float x = delta_cos * vec.x - delta_sin * vec.y;
    const float y = delta_sin * vec.x + delta_cos * vec.y;
    vec = float3(x, y, 0.0f);
  }
}

/* Generate a semi-circle around a point, opposite the direction. */
static void generate_cap(const float3 &point,
                         const float3 &tangent,
                         const float radius,
                         const int corner_subdivisions,
                         const eGPDstroke_Caps cap_type,
                         const int src_point_index,
                         Vector<float3> &r_perimeter,
                         Vector<int> &r_src_indices)
{
  const float3 normal = math::normalize(float3{tangent.y, -tangent.x, 0.0f});
  switch (cap_type) {
    case GP_STROKE_CAP_ROUND:
      generate_arc_from_point_to_point(point - normal * radius,
                                       point + normal * radius,
                                       point,
                                       corner_subdivisions,
                                       src_point_index,
                                       r_perimeter,
                                       r_src_indices);
      break;
    case GP_STROKE_CAP_FLAT:
      r_perimeter.append(point - normal * radius);
      r_src_indices.append(src_point_index);
      r_perimeter.append(point + normal * radius);
      r_src_indices.append(src_point_index);
      break;
    case GP_STROKE_CAP_MAX:
      BLI_assert_unreachable();
      break;
  }
}

/* Generate a corner between two segments, using `miter_limit_angle` as the corner type.
 * NOTE: The perimeter is considered to be to the right hand side of the stroke. The left side
 * perimeter can be generated by reversing the order of points. */
static void generate_corner(const float3 &pt_a,
                            const float3 &pt_b,
                            const float3 &pt_c,
                            const float radius,
                            const float miter_limit_angle,
                            const int corner_subdivisions,
                            const int src_point_index,
                            Vector<float3> &r_perimeter,
                            Vector<int> &r_src_indices)
{
  const float length = math::length(pt_c - pt_b);
  const float length_prev = math::length(pt_b - pt_a);
  const float2 tangent = math::normalize((pt_c - pt_b).xy());
  const float2 tangent_prev = math::normalize((pt_b - pt_a).xy());
  const float3 normal = {tangent.y, -tangent.x, 0.0f};
  const float3 normal_prev = {tangent_prev.y, -tangent_prev.x, 0.0f};

  const float sin_angle = tangent_prev.x * tangent.y - tangent_prev.y * tangent.x;
  /* Whether the corner is an inside or outside corner.
   * This determines whether an arc is added or a single miter point. */
  const bool is_outside_corner = (sin_angle >= 0.0f);
  if (is_outside_corner && miter_limit_angle <= GP_STROKE_MITER_ANGLE_ROUND) {
    generate_arc_from_point_to_point(pt_b + normal_prev * radius,
                                     pt_b + normal * radius,
                                     pt_b,
                                     corner_subdivisions,
                                     src_point_index,
                                     r_perimeter,
                                     r_src_indices);
    return;
  }

  const float2 avg_tangent = math::normalize(tangent_prev + tangent);
  const float3 miter = {avg_tangent.y, -avg_tangent.x, 0.0f};
  const float miter_invscale = math::dot(normal, miter);

  if (is_outside_corner) {
    const bool is_bevel = -math::dot(tangent, tangent_prev) > math::cos(miter_limit_angle);
    if (is_bevel) {
      r_perimeter.append(pt_b + normal_prev * radius);
      r_perimeter.append(pt_b + normal * radius);
      r_src_indices.append_n_times(src_point_index, 2);
      return;
    }
    else {
      const float3 miter_point = pt_b + miter * radius / miter_invscale;

      r_perimeter.append(miter_point);
      r_src_indices.append(src_point_index);
      return;
    }
  }

  /* Avoid division by tiny values for steep angles. */
  const float3 miter_point = (radius < length * miter_invscale &&
                              radius < length_prev * miter_invscale) ?
                                 pt_b + miter * radius / miter_invscale :
                                 pt_b + miter * radius;

  r_perimeter.append(miter_point);
  r_src_indices.append(src_point_index);
}

static void generate_stroke_perimeter(const Span<float3> all_positions,
                                      const Span<float> all_radii,
                                      const IndexRange points,
                                      const int corner_subdivisions,
                                      const bool is_cyclic,
                                      const bool use_caps,
                                      const eGPDstroke_Caps start_cap_type,
                                      const eGPDstroke_Caps end_cap_type,
                                      const VArray<float> miter_angles,
                                      const float outline_offset,
                                      Vector<float3> &r_perimeter,
                                      Vector<int> &r_point_counts,
                                      Vector<int> &r_point_indices)
{
  const Span<float3> positions = all_positions.slice(points);
  const int point_num = points.size();
  if (point_num == 0) {
    return;
  }
  if (point_num == 1) {
    /* Generate a circle for a single point. */
    const int perimeter_start = r_perimeter.size();
    const int point = points.first();
    const float radius = std::max(all_radii[point] + outline_offset, 0.0f);
    generate_circle_from_point(
        positions.first(), radius, corner_subdivisions, point, r_perimeter, r_point_indices);
    const int perimeter_count = r_perimeter.size() - perimeter_start;
    if (perimeter_count > 0) {
      r_point_counts.append(perimeter_count);
    }
    return;
  }

  auto add_corner = [&](const int a, const int b, const int c) {
    const int point = points[b];
    const float3 pt_a = positions[a];
    const float3 pt_b = positions[b];
    const float3 pt_c = positions[c];
    const float radius = std::max(all_radii[point] + outline_offset, 0.0f);
    const float miter_angle = miter_angles[point];
    generate_corner(pt_a,
                    pt_b,
                    pt_c,
                    radius,
                    miter_angle,
                    corner_subdivisions,
                    point,
                    r_perimeter,
                    r_point_indices);
  };
  auto add_cap = [&](const int center_i, const int next_i, const eGPDstroke_Caps cap_type) {
    const int point = points[center_i];
    const float3 &center = positions[center_i];
    const float3 dir = math::normalize(positions[next_i] - center);
    const float radius = std::max(all_radii[point] + outline_offset, 0.0f);
    generate_cap(
        center, dir, radius, corner_subdivisions, cap_type, point, r_perimeter, r_point_indices);
  };

  /* Creates a single cyclic curve with end caps. */
  if (use_caps) {
    /* Open curves generate a start and end cap and a connecting stroke on either side. */
    const int perimeter_start = r_perimeter.size();

    /* Start cap. */
    add_cap(0, 1, start_cap_type);

    /* Right perimeter half. */
    for (const int i : points.index_range().drop_front(1).drop_back(1)) {
      add_corner(i - 1, i, i + 1);
    }
    if (is_cyclic) {
      add_corner(point_num - 2, point_num - 1, 0);
    }

    /* End cap. */
    if (is_cyclic) {
      /* End point is same as start point. */
      add_cap(0, point_num - 1, end_cap_type);
    }
    else {
      /* End point is last point of the curve. */
      add_cap(point_num - 1, point_num - 2, end_cap_type);
    }

    /* Left perimeter half. */
    if (is_cyclic) {
      add_corner(0, point_num - 1, point_num - 2);
    }
    for (const int i : points.index_range().drop_front(1).drop_back(1)) {
      add_corner(point_num - i, point_num - i - 1, point_num - i - 2);
    }

    const int perimeter_count = r_perimeter.size() - perimeter_start;
    if (perimeter_count > 0) {
      r_point_counts.append(perimeter_count);
    }
  }
  else {
    /* Generate separate "inside" and an "outside" perimeter curves.
     * The distinction is arbitrary, called left/right here. */

    /* Right side perimeter. */
    const int left_perimeter_start = r_perimeter.size();
    add_corner(point_num - 1, 0, 1);
    for (const int i : points.index_range().drop_front(1).drop_back(1)) {
      add_corner(i - 1, i, i + 1);
    }
    add_corner(point_num - 2, point_num - 1, 0);
    const int left_perimeter_count = r_perimeter.size() - left_perimeter_start;
    if (left_perimeter_count > 0) {
      r_point_counts.append(left_perimeter_count);
    }

    /* Left side perimeter. */
    const int right_perimeter_start = r_perimeter.size();
    add_corner(0, point_num - 1, point_num - 2);
    for (const int i : points.index_range().drop_front(1).drop_back(1)) {
      add_corner(point_num - i, point_num - i - 1, point_num - i - 2);
    }
    add_corner(1, 0, point_num - 1);
    const int right_perimeter_count = r_perimeter.size() - right_perimeter_start;
    if (right_perimeter_count > 0) {
      r_point_counts.append(right_perimeter_count);
    }
  }
}

struct PerimeterData {
  /* New points per curve count. */
  Vector<int> point_counts;
  /* New point coordinates. */
  Vector<float3> positions;
  /* Source curve index. */
  Vector<int> curve_indices;
  /* Source point index. */
  Vector<int> point_indices;
};

bke::CurvesGeometry create_curves_outline(const bke::greasepencil::Drawing &drawing,
                                          const IndexMask &strokes,
                                          const float4x4 &transform,
                                          const int corner_subdivisions,
                                          const float outline_radius,
                                          const float outline_offset,
                                          const int material_index)
{
  const bke::CurvesGeometry &src_curves = drawing.strokes();
  Span<float3> src_positions = src_curves.positions();
  bke::AttributeAccessor src_attributes = src_curves.attributes();
  const VArray<float> src_radii = drawing.radii();
  const VArray<bool> src_cyclic = *src_attributes.lookup_or_default(
      "cyclic", bke::AttrDomain::Curve, false);
  const VArray<int8_t> src_start_caps = *src_attributes.lookup_or_default<int8_t>(
      "start_cap", bke::AttrDomain::Curve, GP_STROKE_CAP_ROUND);
  const VArray<int8_t> src_end_caps = *src_attributes.lookup_or_default<int8_t>(
      "end_cap", bke::AttrDomain::Curve, GP_STROKE_CAP_ROUND);
  const VArray<int> src_material_index = *src_attributes.lookup_or_default(
      "material_index", bke::AttrDomain::Curve, 0);
  const VArray<float> miter_angles = *src_attributes.lookup_or_default<float>(
      "miter_angle", bke::AttrDomain::Point, GP_STROKE_MITER_ANGLE_ROUND);

  /* Transform positions and radii. */
  Array<float3> transformed_positions(src_positions.size());
  math::transform_points(src_positions, transform, transformed_positions);

  Array<float> transformed_radii(src_radii.size());
  const float scale = math::average(math::to_scale(transform));
  threading::parallel_for(transformed_radii.index_range(), 4096, [&](const IndexRange range) {
    for (const int i : range) {
      transformed_radii[i] = src_radii[i] * scale;
    }
  });

  const float4x4 transform_inv = math::invert(transform);
  threading::EnumerableThreadSpecific<PerimeterData> thread_data;
  strokes.foreach_index(GrainSize(256), [&](const int64_t curve_i) {
    PerimeterData &data = thread_data.local();

    const bool is_cyclic_curve = src_cyclic[curve_i];
    /* NOTE: Cyclic curves would better be represented by a cyclic perimeter without end caps, but
     * we always generate caps for compatibility with GPv2. Fill materials cannot create holes, so
     * a cyclic outline does not work well. */
    const bool use_caps = true /*!is_cyclic_curve*/;

    const int prev_point_num = data.positions.size();
    const int prev_curve_num = data.point_counts.size();
    const IndexRange points = src_curves.points_by_curve()[curve_i];

    generate_stroke_perimeter(transformed_positions,
                              transformed_radii,
                              points,
                              corner_subdivisions,
                              is_cyclic_curve,
                              use_caps,
                              eGPDstroke_Caps(src_start_caps[curve_i]),
                              eGPDstroke_Caps(src_end_caps[curve_i]),
                              miter_angles,
                              outline_offset,
                              data.positions,
                              data.point_counts,
                              data.point_indices);

    /* Transform perimeter positions back into object space. */
    math::transform_points(transform_inv,
                           data.positions.as_mutable_span().drop_front(prev_point_num));

    data.curve_indices.append_n_times(curve_i, data.point_counts.size() - prev_curve_num);
  });

  int dst_curve_num = 0;
  int dst_point_num = 0;
  for (const PerimeterData &data : thread_data) {
    BLI_assert(data.point_counts.size() == data.curve_indices.size());
    BLI_assert(data.positions.size() == data.point_indices.size());
    dst_curve_num += data.point_counts.size();
    dst_point_num += data.positions.size();
  }

  bke::CurvesGeometry dst_curves(dst_point_num, dst_curve_num);
  if (dst_point_num == 0 || dst_curve_num == 0) {
    return dst_curves;
  }

  bke::MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();
  bke::SpanAttributeWriter<bool> dst_cyclic = dst_attributes.lookup_or_add_for_write_span<bool>(
      "cyclic", bke::AttrDomain::Curve);
  bke::SpanAttributeWriter<int> dst_material = dst_attributes.lookup_or_add_for_write_span<int>(
      "material_index", bke::AttrDomain::Curve);
  bke::SpanAttributeWriter<float> dst_radius = dst_attributes.lookup_or_add_for_write_span<float>(
      "radius", bke::AttrDomain::Point);
  const MutableSpan<int> dst_offsets = dst_curves.offsets_for_write();
  const MutableSpan<float3> dst_positions = dst_curves.positions_for_write();
  /* Source indices for attribute mapping. */
  Array<int> dst_curve_map(dst_curve_num);
  Array<int> dst_point_map(dst_point_num);

  IndexRange curves;
  IndexRange points;
  for (const PerimeterData &data : thread_data) {
    curves = curves.after(data.point_counts.size());
    points = points.after(data.positions.size());

    /* Append curve data. */
    dst_curve_map.as_mutable_span().slice(curves).copy_from(data.curve_indices);
    /* Curve offsets are accumulated below. */
    dst_offsets.slice(curves).copy_from(data.point_counts);
    dst_cyclic.span.slice(curves).fill(true);
    if (material_index >= 0) {
      dst_material.span.slice(curves).fill(material_index);
    }
    else {
      for (const int i : curves.index_range()) {
        dst_material.span[curves[i]] = src_material_index[data.curve_indices[i]];
      }
    }

    /* Append point data. */
    dst_positions.slice(points).copy_from(data.positions);
    dst_point_map.as_mutable_span().slice(points).copy_from(data.point_indices);
    dst_radius.span.slice(points).fill(outline_radius);
  }
  offset_indices::accumulate_counts_to_offsets(dst_curves.offsets_for_write());

  bke::gather_attributes(src_attributes,
                         bke::AttrDomain::Point,
                         bke::AttrDomain::Point,
                         bke::attribute_filter_from_skip_ref({"position", "radius"}),
                         dst_point_map,
                         dst_attributes);
  bke::gather_attributes(src_attributes,
                         bke::AttrDomain::Curve,
                         bke::AttrDomain::Curve,
                         bke::attribute_filter_from_skip_ref({"cyclic", "material_index"}),
                         dst_curve_map,
                         dst_attributes);

  dst_cyclic.finish();
  dst_material.finish();
  dst_radius.finish();
  dst_curves.update_curve_types();

  return dst_curves;
}

namespace trim {

enum Side : uint8_t { Start = 0, End = 1 };
enum Distance : uint8_t { Min = 0, Max = 1 };

/* When looking for intersections, we need a little padding, otherwise we could miss curves
 * that intersect for the eye, but not in hard numbers. */
static constexpr int BBOX_PADDING = 2;

/* When creating new intersection points, we don't want them too close to their neighbor,
 * because that clutters the geometry. This threshold defines what 'too close' is. */
static constexpr float DISTANCE_FACTOR_THRESHOLD = 0.01f;

/**
 * Structure describing a curve segment (a point range in a curve) that needs to be removed from
 * the curve.
 */
struct Segment {
  /* Curve index. */
  int curve;

  /* Point range of the segment: starting point and end point. Matches the point offsets
   * in a CurvesGeometry. */
  int point_range[2];

  /* The normalized distance where the trim segment is intersected by another curve.
   * For the outer ends of the trim segment the intersection distance is given between:
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
 * - A collection of trim segments.
 */
struct Segments {
  /* Collection of trim segments: parts of curves between other curves, to be removed from the
   * geometry. */
  Vector<Segment> segments;

  /* Create an initial trim segment with a point range of one point. */
  Segment *create_segment(const int curve, const int point)
  {
    Segment segment{};
    segment.curve = curve;
    segment.point_range[Side::Start] = point;
    segment.point_range[Side::End] = point;

    this->segments.append(std::move(segment));

    return &this->segments.last();
  }

  /* Merge trim segments that are next to each other. */
  void merge_adjacent_segments()
  {
    Vector<Segment> merged_segments;

    /* Note on performance: we deal with small numbers here, so we can afford the double loop. */
    while (!this->segments.is_empty()) {
      Segment a = this->segments.pop_last();

      bool merged = false;
      for (Segment &b : merged_segments) {
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
                                                   const Span<rcti> screen_space_curve_bounds,
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
      if (!BLI_rcti_isect(&bbox_ab, &screen_space_curve_bounds[curve], nullptr)) {
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
 * Expand a trim segment by walking along the curve in forward or backward direction.
 * A trim segments ends at an intersection with another curve, or at the outer end of the curve.
 */
static void expand_trim_segment_direction(Segment &segment,
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
 * Expand a trim segment of one point by walking along the curve in both directions.
 */
static void expand_trim_segment(Segment &segment,
                                const bke::CurvesGeometry &src,
                                const Span<bool> is_intersected_after_point,
                                const Span<float2> intersection_distance,
                                MutableSpan<bool> point_is_in_segment)
{
  const int8_t directions[2] = {-1, 1};
  for (const int8_t direction : directions) {
    expand_trim_segment_direction(segment,
                                  direction,
                                  src,
                                  is_intersected_after_point,
                                  intersection_distance,
                                  point_is_in_segment);
  }
}

bke::CurvesGeometry trim_curve_segments(const bke::CurvesGeometry &src,
                                        const Span<float2> screen_space_positions,
                                        const Span<rcti> screen_space_curve_bounds,
                                        const IndexMask &curve_selection,
                                        const Vector<Vector<int>> &selected_points_in_curves,
                                        const bool keep_caps)
{
  const OffsetIndices<int> src_points_by_curve = src.points_by_curve();

  /* For the selected curves, find all the intersections with other curves. */
  const int src_points_num = src.points_num();
  Array<bool> is_intersected_after_point(src_points_num, false);
  Array<float2> intersection_distance(src_points_num);
  curve_selection.foreach_index(GrainSize(32), [&](const int curve_i) {
    get_intersections_of_curve_with_curves(curve_i,
                                           src,
                                           screen_space_positions,
                                           screen_space_curve_bounds,
                                           is_intersected_after_point,
                                           intersection_distance);
  });

  /* Expand the selected curve points to trim segments (the part of the curve between two
   * intersections). */
  const VArray<bool> is_cyclic = src.cyclic();
  Array<bool> point_is_in_segment(src_points_num, false);
  threading::EnumerableThreadSpecific<Segments> trim_segments_by_thread;
  curve_selection.foreach_index(GrainSize(32), [&](const int curve_i, const int pos) {
    Segments &thread_segments = trim_segments_by_thread.local();
    for (const int selected_point : selected_points_in_curves[pos]) {
      /* Skip point when it is already part of a trim segment. */
      if (point_is_in_segment[selected_point]) {
        continue;
      }

      /* Create new trim segment. */
      Segment *segment = thread_segments.create_segment(curve_i, selected_point);

      /* Expand the trim segment in both directions until an intersection is found or the
       * end of the curve is reached. */
      expand_trim_segment(
          *segment, src, is_intersected_after_point, intersection_distance, point_is_in_segment);

      /* When the end of a curve is reached and the curve is cyclic, we add an extra trim
       * segment for the cyclic second part. */
      if (is_cyclic[curve_i] &&
          (!segment->is_intersected[Side::Start] || !segment->is_intersected[Side::End]) &&
          !(!segment->is_intersected[Side::Start] && !segment->is_intersected[Side::End]))
      {
        const int cyclic_outer_point = !segment->is_intersected[Side::Start] ?
                                           src_points_by_curve[curve_i].last() :
                                           src_points_by_curve[curve_i].first();
        segment = thread_segments.create_segment(curve_i, cyclic_outer_point);

        /* Expand this second segment. */
        expand_trim_segment(
            *segment, src, is_intersected_after_point, intersection_distance, point_is_in_segment);
      }
    }
  });
  Segments trim_segments;
  for (Segments &thread_segments : trim_segments_by_thread) {
    trim_segments.segments.extend(thread_segments.segments);
  }

  /* Abort when no trim segments are found in the lasso area. */
  bke::CurvesGeometry dst;
  if (trim_segments.segments.is_empty()) {
    return dst;
  }

  /* Merge adjacent trim segments. E.g. two point ranges of 0-10 and 11-20 will be merged
   * to one range of 0-20. */
  trim_segments.merge_adjacent_segments();

  /* Create the point transfer data, for converting the source geometry into the new geometry.
   * First, add all curve points not affected by the trim tool. */
  Array<Vector<PointTransferData>> src_to_dst_points(src_points_num);
  for (const int src_curve : src.curves_range()) {
    const IndexRange src_points = src_points_by_curve[src_curve];
    for (const int src_point : src_points) {
      Vector<PointTransferData> &dst_points = src_to_dst_points[src_point];
      const int src_next_point = (src_point == src_points.last()) ? src_points.first() :
                                                                    (src_point + 1);

      /* Add the source point only if it does not lie inside a trim segment. */
      if (!point_is_in_segment[src_point]) {
        dst_points.append({src_point, src_next_point, 0.0f, true, false});
      }
    }
  }

  /* Add new curve points at the intersection points of the trim segments.
   *
   *                               a                 b
   *  source curve    o--------o---*---o--------o----*---o--------o
   *                               ^                 ^
   *  trim segment                 |-----------------|
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
  for (const Segment &trim_segment : trim_segments.segments) {
    /* Intersection at trim segment start. */
    if (trim_segment.is_intersected[Side::Start] &&
        trim_segment.intersection_distance[Side::Start] > DISTANCE_FACTOR_THRESHOLD)
    {
      const int src_point = trim_segment.point_range[Side::Start] - 1;
      Vector<PointTransferData> &dst_points = src_to_dst_points[src_point];
      dst_points.append({src_point,
                         src_point + 1,
                         trim_segment.intersection_distance[Side::Start],
                         false,
                         false});
    }
    /* Intersection at trim segment end. */
    if (trim_segment.is_intersected[Side::End]) {
      const int src_point = trim_segment.point_range[Side::End];
      if (trim_segment.intersection_distance[Side::End] < (1.0f - DISTANCE_FACTOR_THRESHOLD)) {
        Vector<PointTransferData> &dst_points = src_to_dst_points[src_point];
        dst_points.append({src_point,
                           src_point + 1,
                           trim_segment.intersection_distance[Side::End],
                           false,
                           true});
      }
      else {
        /* Mark the 'is_cut' flag on the next point, because a new curve is starting here after
         * the removed trim segment. */
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
  compute_topology_change(src, dst, src_to_dst_points, keep_caps);

  return dst;
}

}  // namespace trim

Curves2DBVHTree build_curves_2d_bvh_from_visible(const ViewContext &vc,
                                                 const Object &object,
                                                 const GreasePencil &grease_pencil,
                                                 Span<MutableDrawingInfo> drawings,
                                                 const int frame_number)
{
  Curves2DBVHTree data;

  /* Upper bound for line count. Arrays are sized for easy index mapping, exact count isn't
   * necessary. Not all points are added to the BVH tree. */
  int max_bvh_lines = 0;
  for (const int i_drawing : drawings.index_range()) {
    if (drawings[i_drawing].frame_number == frame_number) {
      max_bvh_lines += drawings[i_drawing].drawing.strokes().evaluated_points_num();
    }
  }

  data.tree = BLI_bvhtree_new(max_bvh_lines, 0.0f, 4, 6);
  data.start_positions.reinitialize(max_bvh_lines);
  data.end_positions.reinitialize(max_bvh_lines);
  /* Compute offsets array in advance. */
  data.drawing_offsets.reinitialize(drawings.size() + 1);
  for (const int i_drawing : drawings.index_range()) {
    const MutableDrawingInfo &info = drawings[i_drawing];
    data.drawing_offsets[i_drawing] = (drawings[i_drawing].frame_number == frame_number ?
                                           info.drawing.strokes().evaluated_points_num() :
                                           0);
  }
  OffsetIndices bvh_elements_by_drawing = offset_indices::accumulate_counts_to_offsets(
      data.drawing_offsets);

  /* Insert a line for each point except end points. */
  for (const int i_drawing : drawings.index_range()) {
    const MutableDrawingInfo &info = drawings[i_drawing];
    if (drawings[i_drawing].frame_number != frame_number) {
      continue;
    }

    const bke::greasepencil::Layer &layer = grease_pencil.layer(info.layer_index);
    const float4x4 layer_to_world = layer.to_world_space(object);
    const float4x4 projection = ED_view3d_ob_project_mat_get_from_obmat(vc.rv3d, layer_to_world);
    const bke::CurvesGeometry &curves = info.drawing.strokes();
    const OffsetIndices evaluated_points_by_curve = curves.evaluated_points_by_curve();
    const VArray<bool> cyclic = curves.cyclic();
    const Span<float3> evaluated_positions = curves.evaluated_positions();
    const IndexMask curves_mask = curves.curves_range();

    /* Range of indices in the BVH tree for this drawing. */
    const IndexRange bvh_index_range = bvh_elements_by_drawing[i_drawing];
    const MutableSpan<float2> start_positions = data.start_positions.as_mutable_span().slice(
        bvh_index_range);
    const MutableSpan<float2> end_positions = data.end_positions.as_mutable_span().slice(
        bvh_index_range);

    curves_mask.foreach_index([&](const int i_curve) {
      const bool is_cyclic = cyclic[i_curve];
      const IndexRange evaluated_points = evaluated_points_by_curve[i_curve];

      /* Compute screen space positions. */
      for (const int i_point : evaluated_points) {
        const float2 co = ED_view3d_project_float_v2_m4(
            vc.region, evaluated_positions[i_point], projection);
        start_positions[i_point] = co;

        /* Last point is only valid for cyclic curves, gets ignored for non-cyclic curves. */
        const int i_prev_point = (i_point > 0 ? i_point - 1 : evaluated_points.last());
        end_positions[i_prev_point] = co;
      }

      for (const int i_point : evaluated_points.drop_back(1)) {
        const float2 &start = start_positions[i_point];
        const float2 &end = end_positions[i_point];

        const float bb[6] = {start.x, start.y, 0.0f, end.x, end.y, 0.0f};
        BLI_bvhtree_insert(data.tree, bvh_index_range[i_point], bb, 2);
      }
      /* Last->first point segment only used for cyclic curves. */
      if (is_cyclic) {
        const float2 &start = start_positions.last();
        const float2 &end = end_positions.first();

        const float bb[6] = {start.x, start.y, 0.0f, end.x, end.y, 0.0f};
        BLI_bvhtree_insert(data.tree, bvh_index_range[evaluated_points.last()], bb, 2);
      }
    });
  }

  BLI_bvhtree_balance(data.tree);

  return data;
}

void free_curves_2d_bvh_data(Curves2DBVHTree &data)
{
  if (data.tree) {
    BLI_bvhtree_free(data.tree);
    data.tree = nullptr;
  }
  data.drawing_offsets.reinitialize(0);
  data.start_positions.reinitialize(0);
  data.end_positions.reinitialize(0);
}

void find_curve_intersections(const bke::CurvesGeometry &curves,
                              const IndexMask &curve_mask,
                              const Span<float2> screen_space_positions,
                              const Curves2DBVHTree &tree_data,
                              const IndexRange tree_data_range,
                              MutableSpan<bool> r_hits,
                              std::optional<MutableSpan<float>> r_first_intersect_factors,
                              std::optional<MutableSpan<float>> r_last_intersect_factors)
{
  /* Insert segments for cutting extensions on stroke intersection. */
  const OffsetIndices points_by_curve = curves.points_by_curve();
  const VArray<bool> cyclic = curves.cyclic();

  struct RaycastArgs {
    const Curves2DBVHTree &tree_data;
    /* Indices that need to be ignored to avoid intersecting a line with itself or its immediate
     * neighbors. */
    int ignore_index1;
    int ignore_index2;
    int ignore_index3;
  };
  BVHTree_RayCastCallback callback =
      [](void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit) {
        using Result = math::isect_result<float2>;

        const RaycastArgs &args = *static_cast<const RaycastArgs *>(userdata);
        if (ELEM(index, args.ignore_index1, args.ignore_index2, args.ignore_index3)) {
          return;
        }

        const float2 ray_start = float2(ray->origin);
        const float2 ray_end = ray_start + float2(ray->direction) * ray->radius;
        const float2 &line_start = args.tree_data.start_positions[index];
        const float2 &line_end = args.tree_data.end_positions[index];
        Result result = math::isect_seg_seg(ray_start, ray_end, line_start, line_end);
        if (result.kind <= 0) {
          return;
        }
        const float dist = result.lambda * math::distance(ray_start, ray_end);
        if (dist >= hit->dist) {
          return;
        }
        /* These always need to be calculated for the BVH traversal function. */
        hit->index = index;
        hit->dist = result.lambda * math::distance(ray_start, ray_end);
        /* Don't need the hit point, only the lambda. */
        hit->no[0] = result.lambda;
      };

  /* Ray-cast in the forward direction. Ignores intersections with neighboring lines. */
  auto do_raycast = [&](const int index_back,
                        const int index,
                        const int index_forward,
                        float &r_lambda) -> bool {
    if (index_forward < 0) {
      return false;
    }

    const float2 start = screen_space_positions[index];
    const float2 end = screen_space_positions[index_forward];
    float length;
    const float2 dir = math::normalize_and_get_length(end - start, length);

    RaycastArgs args = {tree_data,
                        index_back >= 0 ? int(tree_data_range[index_back]) : -1,
                        int(tree_data_range[index]),
                        index_forward >= 0 ? int(tree_data_range[index_forward]) : -1};
    BVHTreeRayHit hit;
    hit.index = -1;
    hit.dist = FLT_MAX;
    BLI_bvhtree_ray_cast(
        tree_data.tree, float3(start, 0.0f), float3(dir, 0.0f), length, &hit, callback, &args);

    if (hit.index >= 0) {
      r_lambda = hit.no[0];
      return true;
    }
    return false;
  };

  r_hits.fill(false);
  if (r_first_intersect_factors) {
    r_first_intersect_factors->fill(-1.0f);
  }
  if (r_last_intersect_factors) {
    r_last_intersect_factors->fill(-1.0f);
  }

  curve_mask.foreach_index(GrainSize(1024), [&](const int i_curve) {
    const bool is_cyclic = cyclic[i_curve];
    const IndexRange points = points_by_curve[i_curve];

    for (const int i_point : points) {
      const int i_prev_point = (i_point == points.first() ? (is_cyclic ? points.last() : -1) :
                                                            i_point - 1);
      const int i_next_point = (i_point == points.last() ? (is_cyclic ? points.first() : -1) :
                                                           i_point + 1);
      float lambda;
      /* Find first intersections by raycast from each point to the next. */
      if (do_raycast(i_prev_point, i_point, i_next_point, lambda)) {
        r_hits[i_point] = true;
        if (r_first_intersect_factors) {
          (*r_first_intersect_factors)[i_point] = lambda;
        }
      }
      /* Find last intersections by raycast from each point to the previous. */
      if (do_raycast(i_next_point, i_point, i_prev_point, lambda)) {
        /* Note: factor = (1 - lambda) because of reverse raycast. */
        if (r_last_intersect_factors) {
          (*r_last_intersect_factors)[i_point] = 1.0f - lambda;
        }
      }
    }
  });
}

CurveSegmentsData find_curve_segments(const bke::CurvesGeometry &curves,
                                      const IndexMask &curve_mask,
                                      const Span<float2> screen_space_positions,
                                      const Curves2DBVHTree &tree_data,
                                      const IndexRange tree_data_range)
{
  const OffsetIndices points_by_curve = curves.points_by_curve();
  const VArray<bool> cyclic = curves.cyclic();

  Array<bool> hits(curves.points_num());
  Array<float> first_hit_factors(curves.points_num());
  Array<float> last_hit_factors(curves.points_num());
  find_curve_intersections(curves,
                           curve_mask,
                           screen_space_positions,
                           tree_data,
                           tree_data_range,
                           hits,
                           first_hit_factors,
                           last_hit_factors);

  IndexMaskMemory memory;
  const IndexMask hit_mask = IndexMask::from_bools(hits, memory);

  /* Count number of segments in each curve.
   * This is needed to write to the correct segments range for each curve. */
  CurveSegmentsData result;
  result.segment_offsets.reinitialize(curves.curves_num() + 1);
  /* Only segments with hits are written to, initialize all to zero. */
  result.segment_offsets.fill(0);
  curve_mask.foreach_index(GrainSize(512), [&](const int curve_i) {
    const IndexRange points = points_by_curve[curve_i];
    const IndexMask curve_hit_mask = hit_mask.slice_content(points);
    const bool is_cyclic = cyclic[curve_i];

    /* Each hit splits a segment in two. Non-cyclic curves add the curve start point as a segment
     * start point. */
    result.segment_offsets[curve_i] = (is_cyclic ? 0 : 1) + curve_hit_mask.size();
  });
  const OffsetIndices segments_by_curve = offset_indices::accumulate_counts_to_offsets(
      result.segment_offsets);

  const int num_segments = segments_by_curve.total_size();
  result.segment_start_points.reinitialize(num_segments);
  result.segment_start_fractions.reinitialize(num_segments);

  curve_mask.foreach_index(GrainSize(512), [&](const int curve_i) {
    const IndexRange points = points_by_curve[curve_i];
    const IndexMask curve_hit_mask = hit_mask.slice_content(points);
    const bool is_cyclic = cyclic[curve_i];
    const IndexRange segments = segments_by_curve[curve_i];
    const int hit_segments_start = (is_cyclic ? 0 : 1);

    if (segments.is_empty()) {
      return;
    }

    /* Add curve start a segment. */
    if (!is_cyclic) {
      result.segment_start_points[segments[0]] = points.first();
      result.segment_start_fractions[segments[0]] = 0.0f;
    }

    curve_hit_mask.foreach_index([&](const int point_i, const int hit_i) {
      result.segment_start_points[segments[hit_segments_start + hit_i]] = point_i;
      result.segment_start_fractions[segments[hit_segments_start + hit_i]] =
          first_hit_factors[point_i];
    });
  });

  return result;
}

}  // namespace blender::ed::greasepencil
