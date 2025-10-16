/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"
#include "BKE_curves_utils.hh"
#include "BKE_deform.hh"

#include "BLI_array_utils.hh"
#include "BLI_task.hh"

#include "GEO_subdivide_curves.hh"

namespace blender::geometry {

static void calculate_result_offsets(const bke::CurvesGeometry &src_curves,
                                     const IndexMask &selection,
                                     const IndexMask &unselected,
                                     const VArray<int> &cuts,
                                     const Span<bool> cyclic,
                                     MutableSpan<int> dst_curve_offsets,
                                     MutableSpan<int> dst_point_offsets)
{
  /* Fill the array with each curve's point count, then accumulate them to the offsets. */
  const OffsetIndices src_points_by_curve = src_curves.points_by_curve();
  offset_indices::copy_group_sizes(src_points_by_curve, unselected, dst_curve_offsets);
  selection.foreach_index(GrainSize(1024), [&](const int curve_i) {
    const IndexRange src_points = src_points_by_curve[curve_i];
    const IndexRange src_segments = bke::curves::per_curve_point_offsets_range(src_points,
                                                                               curve_i);

    MutableSpan<int> point_offsets = dst_point_offsets.slice(src_segments);
    MutableSpan<int> point_counts = point_offsets.drop_back(1);

    if (src_points.size() == 1) {
      point_counts.first() = 1;
    }
    else {
      cuts.materialize_compressed(src_points, point_counts);
      for (int &count : point_counts) {
        /* Make sure there at least one cut, and add one for the existing point. */
        count = std::max(count, 0) + 1;
      }
      if (!cyclic[curve_i]) {
        /* The last point only has a segment to be subdivided if the curve isn't cyclic. */
        point_counts.last() = 1;
      }
    }

    offset_indices::accumulate_counts_to_offsets(point_offsets);
    dst_curve_offsets[curve_i] = point_offsets.last();
  });
  offset_indices::accumulate_counts_to_offsets(dst_curve_offsets);
}

template<typename T>
static inline void linear_interpolation(const T &a, const T &b, MutableSpan<T> dst)
{
  dst.first() = a;
  const float step = 1.0f / dst.size();
  for (const int i : dst.index_range().drop_front(1)) {
    dst[i] = bke::attribute_math::mix2(i * step, a, b);
  }
}

template<typename T>
static void subdivide_attribute_linear(const OffsetIndices<int> src_points_by_curve,
                                       const OffsetIndices<int> dst_points_by_curve,
                                       const IndexMask &selection,
                                       const Span<int> all_point_offsets,
                                       const Span<T> src,
                                       MutableSpan<T> dst)
{
  selection.foreach_index(GrainSize(512), [&](const int curve_i) {
    const IndexRange src_points = src_points_by_curve[curve_i];
    const IndexRange src_segments = bke::curves::per_curve_point_offsets_range(src_points,
                                                                               curve_i);
    const OffsetIndices<int> curve_offsets = all_point_offsets.slice(src_segments);
    const IndexRange dst_points = dst_points_by_curve[curve_i];
    const Span<T> curve_src = src.slice(src_points);
    MutableSpan<T> curve_dst = dst.slice(dst_points);

    threading::parallel_for(curve_src.index_range().drop_back(1), 1024, [&](IndexRange range) {
      for (const int i : range) {
        const IndexRange segment_points = curve_offsets[i];
        linear_interpolation(curve_src[i], curve_src[i + 1], curve_dst.slice(segment_points));
      }
    });

    const IndexRange dst_last_segment = dst_points.slice(curve_offsets[src_points.size() - 1]);
    linear_interpolation(curve_src.last(), curve_src.first(), dst.slice(dst_last_segment));
  });
}

static void subdivide_attribute_linear(const OffsetIndices<int> src_points_by_curve,
                                       const OffsetIndices<int> dst_points_by_curve,
                                       const IndexMask &selection,
                                       const Span<int> all_point_offsets,
                                       const GSpan src,
                                       GMutableSpan dst)
{
  bke::attribute_math::convert_to_static_type(dst.type(), [&](auto dummy) {
    using T = decltype(dummy);
    subdivide_attribute_linear(src_points_by_curve,
                               dst_points_by_curve,
                               selection,
                               all_point_offsets,
                               src.typed<T>(),
                               dst.typed<T>());
  });
}

static void subdivide_attribute_catmull_rom(const OffsetIndices<int> src_points_by_curve,
                                            const OffsetIndices<int> dst_points_by_curve,
                                            const IndexMask &selection,
                                            const Span<int> all_point_offsets,
                                            const Span<bool> cyclic,
                                            const GSpan src,
                                            GMutableSpan dst)
{
  selection.foreach_index(GrainSize(512), [&](const int curve_i) {
    const IndexRange src_points = src_points_by_curve[curve_i];
    const IndexRange src_segments = bke::curves::per_curve_point_offsets_range(src_points,
                                                                               curve_i);
    const IndexRange dst_points = dst_points_by_curve[curve_i];
    bke::curves::catmull_rom::interpolate_to_evaluated(src.slice(src_points),
                                                       cyclic[curve_i],
                                                       all_point_offsets.slice(src_segments),
                                                       dst.slice(dst_points));
  });
}

static HandleType aligned_or_free_handle_type(const HandleType type)
{
  switch (type) {
    case BEZIER_HANDLE_FREE:
      return BEZIER_HANDLE_FREE;
    case BEZIER_HANDLE_AUTO:
      return BEZIER_HANDLE_ALIGN;
    case BEZIER_HANDLE_VECTOR:
      return BEZIER_HANDLE_FREE;
    case BEZIER_HANDLE_ALIGN:
      return BEZIER_HANDLE_ALIGN;
  }
  BLI_assert_unreachable();
  return BEZIER_HANDLE_FREE;
}

static void subdivide_bezier_segment(const float3 &position_prev,
                                     const float3 &handle_prev,
                                     const float3 &handle_next,
                                     const float3 &position_next,
                                     const HandleType type_prev,
                                     const HandleType type_next,
                                     const IndexRange segment_points,
                                     const int dst_next_segment_start,
                                     MutableSpan<float3> dst_positions,
                                     MutableSpan<float3> dst_handles_l,
                                     MutableSpan<float3> dst_handles_r,
                                     MutableSpan<int8_t> dst_types_l,
                                     MutableSpan<int8_t> dst_types_r)
{
  if (bke::curves::bezier::segment_is_vector(type_prev, type_next)) {
    linear_interpolation(position_prev, position_next, dst_positions.slice(segment_points));
    /* All of the segment handles should be vector handles. */
    dst_types_r[segment_points.first()] = BEZIER_HANDLE_VECTOR;
    dst_types_l[dst_next_segment_start] = BEZIER_HANDLE_VECTOR;
    dst_types_l.slice(segment_points.drop_front(1)).fill(BEZIER_HANDLE_VECTOR);
    dst_types_r.slice(segment_points.drop_front(1)).fill(BEZIER_HANDLE_VECTOR);
  }
  else {
    /* The first point in the segment is always copied. */
    dst_positions[segment_points.first()] = position_prev;

    /* In order to generate a Bezier curve with the same shape as the input curve, apply the
     * De Casteljau algorithm iteratively for the provided number of cuts, constantly updating the
     * previous result point's right handle and the left handle at the end of the segment. */
    float3 segment_start = position_prev;
    float3 segment_handle_prev = handle_prev;
    float3 segment_handle_next = handle_next;
    const float3 segment_end = position_next;

    for (const int i : IndexRange(segment_points.size() - 1)) {
      const float parameter = 1.0f / (segment_points.size() - i);
      const int point_i = segment_points[i];
      bke::curves::bezier::Insertion insert = bke::curves::bezier::insert(
          segment_start, segment_handle_prev, segment_handle_next, segment_end, parameter);

      /* Copy relevant temporary data to the result. */
      dst_handles_r[point_i] = insert.handle_prev;
      dst_handles_l[point_i + 1] = insert.left_handle;
      dst_positions[point_i + 1] = insert.position;

      /* Update the segment to prepare it for the next subdivision. */
      segment_start = insert.position;
      segment_handle_prev = insert.right_handle;
      segment_handle_next = insert.handle_next;
    }

    /* Copy the handles for the last segment from the working variables. */
    dst_handles_r[segment_points.last()] = segment_handle_prev;
    dst_handles_l[dst_next_segment_start] = segment_handle_next;

    /* First and last handles at the ends of the segment are aligned if possible. */
    dst_types_r[segment_points.first()] = aligned_or_free_handle_type(type_prev);
    dst_types_l[dst_next_segment_start] = aligned_or_free_handle_type(type_next);

    /* Handles inside the segment are aligned. */
    dst_types_l.slice(segment_points.drop_front(1)).fill(BEZIER_HANDLE_ALIGN);
    dst_types_r.slice(segment_points.drop_front(1)).fill(BEZIER_HANDLE_ALIGN);
  }
}

static void subdivide_bezier_positions(const Span<float3> src_positions,
                                       const Span<int8_t> src_types_l,
                                       const Span<int8_t> src_types_r,
                                       const Span<float3> src_handles_l,
                                       const Span<float3> src_handles_r,
                                       const OffsetIndices<int> evaluated_offsets,
                                       const bool cyclic,
                                       MutableSpan<float3> dst_positions,
                                       MutableSpan<int8_t> dst_types_l,
                                       MutableSpan<int8_t> dst_types_r,
                                       MutableSpan<float3> dst_handles_l,
                                       MutableSpan<float3> dst_handles_r)
{
  threading::parallel_for(src_positions.index_range().drop_back(1), 512, [&](IndexRange range) {
    for (const int segment_i : range) {
      const IndexRange segment = evaluated_offsets[segment_i];
      subdivide_bezier_segment(src_positions[segment_i],
                               src_handles_r[segment_i],
                               src_handles_l[segment_i + 1],
                               src_positions[segment_i + 1],
                               HandleType(src_types_r[segment_i]),
                               HandleType(src_types_l[segment_i + 1]),
                               segment,
                               segment.one_after_last(),
                               dst_positions,
                               dst_handles_l,
                               dst_handles_r,
                               dst_types_l,
                               dst_types_r);
    }
  });

  if (cyclic) {
    const int last_index = src_positions.index_range().last();
    const IndexRange segment = evaluated_offsets[last_index];
    subdivide_bezier_segment(src_positions.last(),
                             src_handles_r.last(),
                             src_handles_l.first(),
                             src_positions.first(),
                             HandleType(src_types_r.last()),
                             HandleType(src_types_l.first()),
                             segment,
                             0,
                             dst_positions,
                             dst_handles_l,
                             dst_handles_r,
                             dst_types_l,
                             dst_types_r);
  }
  else {
    dst_positions.last() = src_positions.last();
    dst_types_l.first() = src_types_l.first();
    dst_types_r.last() = src_types_r.last();
    dst_handles_l.first() = src_handles_l.first();
    dst_handles_r.last() = src_handles_r.last();
  }

  /* TODO: It would be possible to avoid calling this for all segments besides vector segments. */
  bke::curves::bezier::calculate_auto_handles(
      cyclic, dst_types_l, dst_types_r, dst_positions, dst_handles_l, dst_handles_r);
}

bke::CurvesGeometry subdivide_curves(const bke::CurvesGeometry &src_curves,
                                     const IndexMask &selection,
                                     const VArray<int> &cuts,
                                     const bke::AttributeFilter &attribute_filter)
{
  if (src_curves.is_empty()) {
    return src_curves;
  }

  const OffsetIndices src_points_by_curve = src_curves.points_by_curve();
  /* Cyclic is accessed a lot, it's probably worth it to make sure it's a span. */
  const VArraySpan<bool> cyclic{src_curves.cyclic()};
  IndexMaskMemory memory;
  const IndexMask unselected = selection.complement(src_curves.curves_range(), memory);

  bke::CurvesGeometry dst_curves = bke::curves::copy_only_curve_domain(src_curves);
  /* Copy vertex groups from source curves to allow copying vertex group attributes. */
  BKE_defgroup_copy_list(&dst_curves.vertex_group_names, &src_curves.vertex_group_names);

  /* For each point, this contains the point offset in the corresponding result curve,
   * starting at zero. For example for two curves with four points each, the values might
   * look like this:
   *
   * |                     | Curve 0           | Curve 1            |
   * | ------------------- |---|---|---|---|---|---|---|---|---|----|
   * | Cuts                | 0 | 3 | 0 | 0 | - | 2 | 0 | 0 | 4 | -  |
   * | New Point Count     | 1 | 4 | 1 | 1 | - | 3 | 1 | 1 | 5 | -  |
   * | Accumulated Offsets | 0 | 1 | 5 | 6 | 7 | 0 | 3 | 4 | 5 | 10 |
   *
   * Storing the leading zero is unnecessary but makes the array a bit simpler to use by avoiding
   * a check for the first segment, and because some existing utilities also use leading zeros. */
  Array<int> all_point_offset_data(src_curves.points_num() + src_curves.curves_num());
#ifndef NDEBUG
  all_point_offset_data.fill(-1);
#endif
  calculate_result_offsets(src_curves,
                           selection,
                           unselected,
                           cuts,
                           cyclic,
                           dst_curves.offsets_for_write(),
                           all_point_offset_data);
  const OffsetIndices dst_points_by_curve = dst_curves.points_by_curve();

  const Span<int> all_point_offsets(all_point_offset_data);

  dst_curves.resize(dst_curves.offsets().last(), dst_curves.curves_num());

  const bke::AttributeAccessor src_attributes = src_curves.attributes();
  bke::MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();

  Vector<bke::AttributeTransferData> attributes_to_transfer =
      bke::retrieve_attributes_for_transfer(
          src_attributes, dst_attributes, {bke::AttrDomain::Point}, attribute_filter);

  auto subdivide_catmull_rom = [&](const IndexMask &selection) {
    for (auto &attribute : attributes_to_transfer) {
      subdivide_attribute_catmull_rom(src_points_by_curve,
                                      dst_points_by_curve,
                                      selection,
                                      all_point_offsets,
                                      cyclic,
                                      attribute.src,
                                      attribute.dst.span);
    }
  };

  auto subdivide_poly = [&](const IndexMask &selection) {
    for (auto &attribute : attributes_to_transfer) {
      subdivide_attribute_linear(src_points_by_curve,
                                 dst_points_by_curve,
                                 selection,
                                 all_point_offsets,
                                 attribute.src,
                                 attribute.dst.span);
    }
  };

  auto subdivide_bezier = [&](const IndexMask &selection) {
    const Span<float3> src_positions = src_curves.positions();
    const VArraySpan<int8_t> src_types_l{src_curves.handle_types_left()};
    const VArraySpan<int8_t> src_types_r{src_curves.handle_types_right()};
    const Span<float3> src_handles_l = *src_curves.handle_positions_left();
    const Span<float3> src_handles_r = *src_curves.handle_positions_right();

    MutableSpan<float3> dst_positions = dst_curves.positions_for_write();
    MutableSpan<int8_t> dst_types_l = dst_curves.handle_types_left_for_write();
    MutableSpan<int8_t> dst_types_r = dst_curves.handle_types_right_for_write();
    MutableSpan<float3> dst_handles_l = dst_curves.handle_positions_left_for_write();
    MutableSpan<float3> dst_handles_r = dst_curves.handle_positions_right_for_write();
    const OffsetIndices<int> dst_points_by_curve = dst_curves.points_by_curve();

    selection.foreach_index(GrainSize(512), [&](const int curve_i) {
      const IndexRange src_points = src_points_by_curve[curve_i];
      const IndexRange src_segments = bke::curves::per_curve_point_offsets_range(src_points,
                                                                                 curve_i);
      const IndexRange dst_points = dst_points_by_curve[curve_i];
      subdivide_bezier_positions(src_positions.slice(src_points),
                                 src_types_l.slice(src_points),
                                 src_types_r.slice(src_points),
                                 src_handles_l.slice(src_points),
                                 src_handles_r.slice(src_points),
                                 all_point_offsets.slice(src_segments),
                                 cyclic[curve_i],
                                 dst_positions.slice(dst_points),
                                 dst_types_l.slice(dst_points),
                                 dst_types_r.slice(dst_points),
                                 dst_handles_l.slice(dst_points),
                                 dst_handles_r.slice(dst_points));
    });

    /* Filter out positions and handles that are already interpolated. */
    const Set<StringRef> attributes_to_skip = {
        "position", "handle_type_left", "handle_type_right", "handle_right", "handle_left"};
    for (auto &attribute : attributes_to_transfer) {
      if (attributes_to_skip.contains(attribute.name)) {
        continue;
      }
      subdivide_attribute_linear(src_points_by_curve,
                                 dst_points_by_curve,
                                 selection,
                                 all_point_offsets,
                                 attribute.src,
                                 attribute.dst.span);
    }
  };

  /* NURBS curves are just treated as poly curves. NURBS subdivision that maintains
   * their shape may be possible, but probably wouldn't work with the "cuts" input. */
  auto subdivide_nurbs = subdivide_poly;

  bke::curves::foreach_curve_by_type(src_curves.curve_types(),
                                     src_curves.curve_type_counts(),
                                     selection,
                                     subdivide_catmull_rom,
                                     subdivide_poly,
                                     subdivide_bezier,
                                     subdivide_nurbs);

  for (auto &attribute : attributes_to_transfer) {
    array_utils::copy_group_to_group(
        src_points_by_curve, dst_points_by_curve, unselected, attribute.src, attribute.dst.span);
    attribute.dst.finish();
  }

  bke::curves::nurbs::copy_custom_knots(src_curves, selection, dst_curves);
  return dst_curves;
}

}  // namespace blender::geometry
