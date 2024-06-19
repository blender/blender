/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include <limits>

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_kdtree.h"
#include "BLI_math_vector.hh"
#include "BLI_stack.hh"

#include "BKE_curves_utils.hh"
#include "BKE_grease_pencil.hh"

#include "ED_grease_pencil.hh"

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

  float *r_cubic_array;
  uint r_cubic_array_len;
  int error = curve_fit_cubic_to_points_fl(*points.data(),
                                           points.size(),
                                           2,
                                           error_threshold,
                                           CURVE_FIT_CALC_HIGH_QUALIY,
                                           indicies_ptr,
                                           indices.size(),
                                           &r_cubic_array,
                                           &r_cubic_array_len,
                                           nullptr,
                                           nullptr,
                                           nullptr);

  if (error != 0) {
    /* Some error occurred. Return. */
    return {};
  }

  if (r_cubic_array == nullptr) {
    return {};
  }

  Span<float2> r_cubic_array_span(reinterpret_cast<float2 *>(r_cubic_array),
                                  r_cubic_array_len * 3);
  Array<float2> curve_positions(r_cubic_array_span);
  /* Free the c-style array. */
  free(r_cubic_array);
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
  uint *r_corners;
  uint r_corner_len;
  const int error = curve_fit_corners_detect_fl(*points.data(),
                                                points.size(),
                                                float2::type_length,
                                                radius_min,
                                                radius_max,
                                                samples_max,
                                                angle_threshold,
                                                &r_corners,
                                                &r_corner_len);
  if (error != 0) {
    /* Error occurred, return. */
    return IndexMask();
  }

  if (r_corners == nullptr) {
    return IndexMask();
  }

  BLI_assert(samples_max < std::numeric_limits<int>::max());
  Span<int> indices(reinterpret_cast<int *>(r_corners), r_corner_len);
  const IndexMask corner_mask = IndexMask::from_indices<int>(indices, memory);
  /* Free the c-style array. */
  free(r_corners);
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

/* NOTE: The code here is an adapted version of #blender::geometry::point_merge_by_distance. */
blender::bke::CurvesGeometry curves_merge_by_distance(
    const bke::CurvesGeometry &src_curves,
    const float merge_distance,
    const IndexMask &selection,
    const bke::AnonymousAttributePropagationInfo &propagation_info)
{
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

      Array<float> distances_along_curve(points.size());
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
  src_attributes.for_all([&](const bke::AttributeIDRef &id,
                             const bke::AttributeMetaData &meta_data) {
    if (id.is_anonymous() && !propagation_info.propagate(id.anonymous_id())) {
      return true;
    }
    if (meta_data.domain != bke::AttrDomain::Point) {
      return true;
    }

    bke::GAttributeReader src_attribute = src_attributes.lookup(id);
    bke::attribute_math::convert_to_static_type(src_attribute.varray.type(), [&](auto dummy) {
      using T = decltype(dummy);
      if constexpr (!std::is_void_v<bke::attribute_math::DefaultMixer<T>>) {
        bke::SpanAttributeWriter<T> dst_attribute =
            dst_attributes.lookup_or_add_for_write_only_span<T>(id, bke::AttrDomain::Point);
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
    return true;
  });

  return dst_curves;
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
  const float3 normal = {tangent.y, -tangent.x, 0.0f};
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
      r_perimeter.append(point + normal * radius);
      r_src_indices.append(src_point_index);
      break;
    case GP_STROKE_CAP_MAX:
      BLI_assert_unreachable();
      break;
  }
}

/* Generate a corner between two segments, with a rounded outer perimeter.
 * NOTE: The perimeter is considered to be to the right hand side of the stroke. The left side
 * perimeter can be generated by reversing the order of points. */
static void generate_corner(const float3 &pt_a,
                            const float3 &pt_b,
                            const float3 &pt_c,
                            const float radius,
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
  if (is_outside_corner) {
    generate_arc_from_point_to_point(pt_b + normal_prev * radius,
                                     pt_b + normal * radius,
                                     pt_b,
                                     corner_subdivisions,
                                     src_point_index,
                                     r_perimeter,
                                     r_src_indices);
  }
  else {
    const float2 avg_tangent = math::normalize(tangent_prev + tangent);
    const float3 miter = {avg_tangent.y, -avg_tangent.x, 0.0f};
    const float miter_invscale = math::dot(normal, miter);

    /* Avoid division by tiny values for steep angles. */
    const float3 miter_point = (radius < length * miter_invscale &&
                                radius < length_prev * miter_invscale) ?
                                   pt_b + miter * radius / miter_invscale :
                                   pt_b + miter * radius;

    r_perimeter.append(miter_point);
    r_src_indices.append(src_point_index);
  }
}

static void generate_stroke_perimeter(const Span<float3> all_positions,
                                      const VArray<float> all_radii,
                                      const IndexRange points,
                                      const int corner_subdivisions,
                                      const bool is_cyclic,
                                      const bool use_caps,
                                      const eGPDstroke_Caps start_cap_type,
                                      const eGPDstroke_Caps end_cap_type,
                                      const float outline_offset,
                                      Vector<float3> &r_perimeter,
                                      Vector<int> &r_point_counts,
                                      Vector<int> &r_point_indices)
{
  const Span<float3> positions = all_positions.slice(points);
  const int point_num = points.size();
  if (point_num < 2) {
    return;
  }

  auto add_corner = [&](const int a, const int b, const int c) {
    const int point = points[b];
    const float3 pt_a = positions[a];
    const float3 pt_b = positions[b];
    const float3 pt_c = positions[c];
    const float radius = std::max(all_radii[point] + outline_offset, 0.0f);
    generate_corner(
        pt_a, pt_b, pt_c, radius, corner_subdivisions, point, r_perimeter, r_point_indices);
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
      "material_index", bke::AttrDomain::Curve, -1);

  /* Transform positions. */
  Array<float3> transformed_positions(src_positions.size());
  threading::parallel_for(transformed_positions.index_range(), 4096, [&](const IndexRange range) {
    for (const int i : range) {
      transformed_positions[i] = math::transform_point(transform, src_positions[i]);
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
                              src_radii,
                              points,
                              corner_subdivisions,
                              is_cyclic_curve,
                              use_caps,
                              eGPDstroke_Caps(src_start_caps[curve_i]),
                              eGPDstroke_Caps(src_end_caps[curve_i]),
                              outline_offset,
                              data.positions,
                              data.point_counts,
                              data.point_indices);

    /* Transform perimeter positions back into object space. */
    for (float3 &pos : data.positions.as_mutable_span().drop_front(prev_point_num)) {
      pos = math::transform_point(transform_inv, pos);
    }

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
                         {},
                         {"position", "radius"},
                         dst_point_map,
                         dst_attributes);
  bke::gather_attributes(src_attributes,
                         bke::AttrDomain::Curve,
                         {},
                         {"cyclic", "material_index"},
                         dst_curve_map,
                         dst_attributes);

  dst_cyclic.finish();
  dst_material.finish();
  dst_radius.finish();
  dst_curves.update_curve_types();

  return dst_curves;
}

}  // namespace blender::ed::greasepencil
