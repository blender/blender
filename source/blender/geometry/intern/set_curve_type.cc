/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_attribute.hh"
#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"
#include "BKE_curves_utils.hh"
#include "BKE_deform.hh"

#include "BLI_array_utils.hh"
#include "BLI_task.hh"

#include "GEO_set_curve_type.hh"

namespace blender::geometry {

/**
 * This function answers the question about possible conversion method for NURBS-to-Bezier. In
 * general for 3rd degree NURBS curves there is one-to-one relation with 3rd degree Bezier curves
 * that can be exploit for conversion - Bezier handles sit on NURBS hull segments and in the middle
 * between those handles are Bezier anchor points.
 */
static bool is_nurbs_to_bezier_one_to_one(const KnotsMode knots_mode)
{
  if (ELEM(knots_mode, NURBS_KNOT_MODE_NORMAL, NURBS_KNOT_MODE_ENDPOINT)) {
    return true;
  }
  return false;
}

template<typename T>
static void scale_input_assign(const Span<T> src,
                               const int scale,
                               const int offset,
                               MutableSpan<T> dst)
{
  for (const int i : dst.index_range()) {
    dst[i] = src[i * scale + offset];
  }
}

/**
 * The Bezier control point and its handles become three control points on the NURBS curve,
 * so each attribute value is duplicated three times.
 */
template<typename T> static void bezier_generic_to_nurbs(const Span<T> src, MutableSpan<T> dst)
{
  for (const int i : src.index_range()) {
    dst[i * 3] = src[i];
    dst[i * 3 + 1] = src[i];
    dst[i * 3 + 2] = src[i];
  }
}

static void bezier_generic_to_nurbs(const GSpan src, GMutableSpan dst)
{
  bke::attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
    using T = decltype(dummy);
    bezier_generic_to_nurbs(src.typed<T>(), dst.typed<T>());
  });
}

static void bezier_positions_to_nurbs(const Span<float3> src_positions,
                                      const Span<float3> src_handles_l,
                                      const Span<float3> src_handles_r,
                                      MutableSpan<float3> dst_positions)
{
  for (const int i : src_positions.index_range()) {
    dst_positions[i * 3] = src_handles_l[i];
    dst_positions[i * 3 + 1] = src_positions[i];
    dst_positions[i * 3 + 2] = src_handles_r[i];
  }
}

static void catmull_rom_to_bezier_handles(const Span<float3> src_positions,
                                          const bool cyclic,
                                          MutableSpan<float3> dst_handles_l,
                                          MutableSpan<float3> dst_handles_r)
{
  /* Catmull Rom curves are the same as Bezier curves with automatically defined handle positions.
   * This constant defines the portion of the distance between the next/previous points to use for
   * the length of the handles. */
  constexpr float handle_scale = 1.0f / 6.0f;

  if (src_positions.size() == 1) {
    dst_handles_l.first() = src_positions.first();
    dst_handles_r.first() = src_positions.first();
    return;
  }

  const float3 first_offset = cyclic ? src_positions[1] - src_positions.last() :
                                       src_positions[1] - src_positions[0];
  dst_handles_r.first() = src_positions.first() + first_offset * handle_scale;
  dst_handles_l.first() = src_positions.first() - first_offset * handle_scale;

  const float3 last_offset = cyclic ? src_positions.first() - src_positions.last(1) :
                                      src_positions.last() - src_positions.last(1);
  dst_handles_l.last() = src_positions.last() - last_offset * handle_scale;
  dst_handles_r.last() = src_positions.last() + last_offset * handle_scale;

  for (const int i : src_positions.index_range().drop_front(1).drop_back(1)) {
    const float3 left_offset = src_positions[i - 1] - src_positions[i + 1];
    dst_handles_l[i] = src_positions[i] + left_offset * handle_scale;

    const float3 right_offset = src_positions[i + 1] - src_positions[i - 1];
    dst_handles_r[i] = src_positions[i] + right_offset * handle_scale;
  }
}

static void catmull_rom_to_nurbs_positions(const Span<float3> src_positions,
                                           const bool cyclic,
                                           MutableSpan<float3> dst_positions)
{
  /* Convert the Catmull Rom position data to Bezier handles in order to reuse the Bezier to
   * NURBS positions assignment. If this becomes a bottleneck, this step could be avoided. */
  Array<float3, 32> bezier_handles_l(src_positions.size());
  Array<float3, 32> bezier_handles_r(src_positions.size());
  catmull_rom_to_bezier_handles(src_positions, cyclic, bezier_handles_l, bezier_handles_r);
  bezier_positions_to_nurbs(src_positions, bezier_handles_l, bezier_handles_r, dst_positions);
}

template<typename T>
static void nurbs_to_bezier_assign(const Span<T> src,
                                   const MutableSpan<T> dst,
                                   const KnotsMode knots_mode)
{
  switch (knots_mode) {
    case NURBS_KNOT_MODE_NORMAL:
      for (const int i : dst.index_range()) {
        dst[i] = src[(i + 1) % src.size()];
      }
      break;
    case NURBS_KNOT_MODE_ENDPOINT:
      for (const int i : dst.index_range().drop_back(1).drop_front(1)) {
        dst[i] = src[i + 1];
      }
      dst.first() = src.first();
      dst.last() = src.last();
      break;
    default:
      /* Every 3rd NURBS position (starting from index 1) should have its attributes transferred.
       */
      scale_input_assign<T>(src, 3, 1, dst);
  }
}

static void nurbs_to_bezier_assign(const GSpan src, const KnotsMode knots_mode, GMutableSpan dst)
{
  bke::attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
    using T = decltype(dummy);
    nurbs_to_bezier_assign(src.typed<T>(), dst.typed<T>(), knots_mode);
  });
}

static Vector<float3> create_nurbs_to_bezier_handles(const Span<float3> nurbs_positions,
                                                     const KnotsMode knots_mode)
{
  const int nurbs_positions_num = nurbs_positions.size();
  Vector<float3> handle_positions;

  if (is_nurbs_to_bezier_one_to_one(knots_mode)) {
    const bool is_periodic = knots_mode == NURBS_KNOT_MODE_NORMAL;
    if (is_periodic) {
      handle_positions.append(nurbs_positions[1] +
                              ((nurbs_positions[0] - nurbs_positions[1]) / 3));
    }
    else {
      handle_positions.append(2 * nurbs_positions[0] - nurbs_positions[1]);
      handle_positions.append(nurbs_positions[1]);
    }

    /* Place Bezier handles on interior NURBS hull segments. Those handles can be either placed on
     * endpoints, midpoints or 1/3 of the distance of a hull segment. */
    const int segments_num = nurbs_positions_num - 1;
    const bool ignore_interior_segment = segments_num == 3 && is_periodic == false;
    if (ignore_interior_segment == false) {
      const float mid_offset = float(segments_num - 1) / 2.0f;
      for (const int i : IndexRange(1, segments_num - 2)) {
        /* Divisor can have values: 1, 2 or 3. */
        const int divisor = is_periodic ?
                                3 :
                                std::min(3, int(-std::abs(i - mid_offset) + mid_offset + 1.0f));
        const float3 &p1 = nurbs_positions[i];
        const float3 &p2 = nurbs_positions[i + 1];
        const float3 displacement = (p2 - p1) / divisor;
        const int num_handles_on_segment = divisor < 3 ? 1 : 2;
        for (int j : IndexRange(1, num_handles_on_segment)) {
          handle_positions.append(p1 + (displacement * j));
        }
      }
    }

    const int last_index = nurbs_positions_num - 1;
    if (is_periodic) {
      handle_positions.append(
          nurbs_positions[last_index - 1] +
          ((nurbs_positions[last_index] - nurbs_positions[last_index - 1]) / 3));
    }
    else {
      handle_positions.append(nurbs_positions[last_index - 1]);
      handle_positions.append(2 * nurbs_positions[last_index] - nurbs_positions[last_index - 1]);
    }
  }
  else {
    for (const int i : IndexRange(nurbs_positions_num)) {
      if (i % 3 == 1) {
        continue;
      }
      handle_positions.append(nurbs_positions[i]);
    }
    if (nurbs_positions_num % 3 == 1) {
      handle_positions.pop_last();
    }
    else if (nurbs_positions_num % 3 == 2) {
      const int last_index = nurbs_positions_num - 1;
      handle_positions.append(2 * nurbs_positions[last_index] - nurbs_positions[last_index - 1]);
    }
  }

  return handle_positions;
}

static void create_nurbs_to_bezier_positions(const Span<float3> nurbs_positions,
                                             const Span<float3> handle_positions,
                                             const KnotsMode knots_mode,
                                             MutableSpan<float3> bezier_positions)
{
  if (is_nurbs_to_bezier_one_to_one(knots_mode)) {
    for (const int i : bezier_positions.index_range()) {
      bezier_positions[i] = math::interpolate(
          handle_positions[i * 2], handle_positions[i * 2 + 1], 0.5f);
    }
  }
  else {
    /* Every 3rd NURBS position (starting from index 1) should be converted to Bezier position. */
    scale_input_assign(nurbs_positions, 3, 1, bezier_positions);
  }
}

static int to_bezier_size(const CurveType src_type,
                          const bool cyclic,
                          const KnotsMode knots_mode,
                          const int src_size)
{
  switch (src_type) {
    case CURVE_TYPE_NURBS: {
      if (is_nurbs_to_bezier_one_to_one(knots_mode)) {
        return cyclic ? src_size : std::max(1, src_size - 2);
      }
      return (src_size + 1) / 3;
    }
    default:
      return src_size;
  }
}

static int to_nurbs_size(const CurveType src_type, const int src_size)
{
  switch (src_type) {
    case CURVE_TYPE_BEZIER:
    case CURVE_TYPE_CATMULL_ROM:
      return src_size * 3;
    default:
      return src_size;
  }
}

static bke::CurvesGeometry convert_curves_to_bezier(const bke::CurvesGeometry &src_curves,
                                                    const IndexMask &selection,
                                                    const bke::AttributeFilter &attribute_filter)
{
  const OffsetIndices src_points_by_curve = src_curves.points_by_curve();
  const VArray<int8_t> src_knot_modes = src_curves.nurbs_knots_modes();
  const VArray<int8_t> src_types = src_curves.curve_types();
  const VArray<bool> src_cyclic = src_curves.cyclic();
  const Span<float3> src_positions = src_curves.positions();
  const bke::AttributeAccessor src_attributes = src_curves.attributes();
  IndexMaskMemory memory;
  const IndexMask unselected = selection.complement(src_curves.curves_range(), memory);

  bke::CurvesGeometry dst_curves = bke::curves::copy_only_curve_domain(src_curves);
  /* Copy vertex groups from source curves to allow copying vertex group attributes. */
  BKE_defgroup_copy_list(&dst_curves.vertex_group_names, &src_curves.vertex_group_names);
  dst_curves.fill_curve_types(selection, CURVE_TYPE_BEZIER);

  MutableSpan<int> dst_offsets = dst_curves.offsets_for_write();
  offset_indices::copy_group_sizes(src_points_by_curve, unselected, dst_offsets);
  selection.foreach_index(GrainSize(1024), [&](const int i) {
    dst_offsets[i] = to_bezier_size(CurveType(src_types[i]),
                                    src_cyclic[i],
                                    KnotsMode(src_knot_modes[i]),
                                    src_points_by_curve[i].size());
  });
  offset_indices::accumulate_counts_to_offsets(dst_offsets);
  dst_curves.resize(dst_offsets.last(), dst_curves.curves_num());
  const OffsetIndices dst_points_by_curve = dst_curves.points_by_curve();

  bke::MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();
  MutableSpan<float3> dst_positions = dst_curves.positions_for_write();
  MutableSpan<float3> dst_handles_l = dst_curves.handle_positions_left_for_write();
  MutableSpan<float3> dst_handles_r = dst_curves.handle_positions_right_for_write();
  MutableSpan<int8_t> dst_types_l = dst_curves.handle_types_left_for_write();
  MutableSpan<int8_t> dst_types_r = dst_curves.handle_types_right_for_write();
  Vector<bke::AttributeTransferData> generic_attributes = bke::retrieve_attributes_for_transfer(
      src_attributes, dst_attributes, {bke::AttrDomain::Point}, attribute_filter);
  Set<StringRef> attributes_to_skip = {
      "position", "handle_type_left", "handle_type_right", "handle_right", "handle_left"};
  if (!dst_curves.has_curve_with_type(CURVE_TYPE_NURBS)) {
    attributes_to_skip.add_new("nurbs_weight");
  }

  auto catmull_rom_to_bezier = [&](const IndexMask &selection) {
    bke::curves::fill_points<int8_t>(
        dst_points_by_curve, selection, BEZIER_HANDLE_ALIGN, dst_types_l);
    bke::curves::fill_points<int8_t>(
        dst_points_by_curve, selection, BEZIER_HANDLE_ALIGN, dst_types_r);
    array_utils::copy_group_to_group(
        src_points_by_curve, dst_points_by_curve, selection, src_positions, dst_positions);

    selection.foreach_index(GrainSize(512), [&](const int i) {
      const IndexRange src_points = src_points_by_curve[i];
      const IndexRange dst_points = dst_points_by_curve[i];
      catmull_rom_to_bezier_handles(src_positions.slice(src_points),
                                    src_cyclic[i],
                                    dst_handles_l.slice(dst_points),
                                    dst_handles_r.slice(dst_points));
    });

    for (bke::AttributeTransferData &attribute : generic_attributes) {
      if (attributes_to_skip.contains(attribute.name)) {
        continue;
      }
      array_utils::copy_group_to_group(
          src_points_by_curve, dst_points_by_curve, selection, attribute.src, attribute.dst.span);
    }
  };

  auto poly_to_bezier = [&](const IndexMask &selection) {
    array_utils::copy_group_to_group(
        src_points_by_curve, dst_points_by_curve, selection, src_positions, dst_positions);
    bke::curves::fill_points<int8_t>(
        dst_points_by_curve, selection, BEZIER_HANDLE_VECTOR, dst_types_l);
    bke::curves::fill_points<int8_t>(
        dst_points_by_curve, selection, BEZIER_HANDLE_VECTOR, dst_types_r);
    dst_curves.calculate_bezier_auto_handles();
    for (bke::AttributeTransferData &attribute : generic_attributes) {
      if (attributes_to_skip.contains(attribute.name)) {
        continue;
      }
      array_utils::copy_group_to_group(
          src_points_by_curve, dst_points_by_curve, selection, attribute.src, attribute.dst.span);
    }
  };

  auto bezier_to_bezier = [&](const IndexMask &selection) {
    const VArraySpan<int8_t> src_types_l = src_curves.handle_types_left();
    const VArraySpan<int8_t> src_types_r = src_curves.handle_types_right();
    const Span<float3> src_handles_l = *src_curves.handle_positions_left();
    const Span<float3> src_handles_r = *src_curves.handle_positions_right();

    array_utils::copy_group_to_group(
        src_points_by_curve, dst_points_by_curve, selection, src_positions, dst_positions);
    array_utils::copy_group_to_group(
        src_points_by_curve, dst_points_by_curve, selection, src_handles_l, dst_handles_l);
    array_utils::copy_group_to_group(
        src_points_by_curve, dst_points_by_curve, selection, src_handles_r, dst_handles_r);
    array_utils::copy_group_to_group(
        src_points_by_curve, dst_points_by_curve, selection, src_types_l, dst_types_l);
    array_utils::copy_group_to_group(
        src_points_by_curve, dst_points_by_curve, selection, src_types_r, dst_types_r);

    dst_curves.calculate_bezier_auto_handles();

    for (bke::AttributeTransferData &attribute : generic_attributes) {
      if (attributes_to_skip.contains(attribute.name)) {
        continue;
      }
      array_utils::copy_group_to_group(
          src_points_by_curve, dst_points_by_curve, selection, attribute.src, attribute.dst.span);
    }
  };

  auto nurbs_to_bezier = [&](const IndexMask &selection) {
    bke::curves::fill_points<int8_t>(
        dst_points_by_curve, selection, BEZIER_HANDLE_ALIGN, dst_types_l);
    bke::curves::fill_points<int8_t>(
        dst_points_by_curve, selection, BEZIER_HANDLE_ALIGN, dst_types_r);

    selection.foreach_index(GrainSize(64), [&](const int i) {
      const IndexRange src_points = src_points_by_curve[i];
      const IndexRange dst_points = dst_points_by_curve[i];
      const Span<float3> src_curve_positions = src_positions.slice(src_points);
      if (dst_points.size() == 1) {
        const float3 &position = src_positions[src_points.first()];
        dst_positions[dst_points.first()] = position;
        dst_handles_l[dst_points.first()] = position;
        dst_handles_r[dst_points.first()] = position;
        return;
      }

      KnotsMode knots_mode = KnotsMode(src_knot_modes[i]);
      Span<float3> nurbs_positions = src_curve_positions;
      Vector<float3> nurbs_positions_vector;
      if (src_cyclic[i] && is_nurbs_to_bezier_one_to_one(knots_mode)) {
        /* For conversion treat this as periodic closed curve. Extend NURBS hull to first and
         * second point which will act as a skeleton for placing Bezier handles. */
        nurbs_positions_vector.extend(src_curve_positions);
        nurbs_positions_vector.append(src_curve_positions[0]);
        nurbs_positions_vector.append(src_curve_positions[1]);
        nurbs_positions = nurbs_positions_vector;
        knots_mode = NURBS_KNOT_MODE_NORMAL;
      }

      const Vector<float3> handle_positions = create_nurbs_to_bezier_handles(nurbs_positions,
                                                                             knots_mode);

      scale_input_assign(handle_positions.as_span(), 2, 0, dst_handles_l.slice(dst_points));
      scale_input_assign(handle_positions.as_span(), 2, 1, dst_handles_r.slice(dst_points));

      create_nurbs_to_bezier_positions(
          nurbs_positions, handle_positions, knots_mode, dst_positions.slice(dst_points));
    });

    for (bke::AttributeTransferData &attribute : generic_attributes) {
      if (attributes_to_skip.contains(attribute.name)) {
        continue;
      }
      selection.foreach_index(GrainSize(512), [&](const int i) {
        const IndexRange src_points = src_points_by_curve[i];
        const IndexRange dst_points = dst_points_by_curve[i];
        nurbs_to_bezier_assign(attribute.src.slice(src_points),
                               KnotsMode(src_knot_modes[i]),
                               attribute.dst.span.slice(dst_points));
      });
    }
  };

  bke::curves::foreach_curve_by_type(src_curves.curve_types(),
                                     src_curves.curve_type_counts(),
                                     selection,
                                     catmull_rom_to_bezier,
                                     poly_to_bezier,
                                     bezier_to_bezier,
                                     nurbs_to_bezier);

  for (bke::AttributeTransferData &attribute : generic_attributes) {
    array_utils::copy_group_to_group(
        src_points_by_curve, dst_points_by_curve, unselected, attribute.src, attribute.dst.span);

    attribute.dst.finish();
  }
  bke::curves::nurbs::copy_custom_knots(src_curves, selection, dst_curves);
  return dst_curves;
}

static bke::CurvesGeometry convert_curves_to_nurbs(const bke::CurvesGeometry &src_curves,
                                                   const IndexMask &selection,
                                                   const bke::AttributeFilter &attribute_filter)
{
  const OffsetIndices src_points_by_curve = src_curves.points_by_curve();
  const VArray<int8_t> src_types = src_curves.curve_types();
  const VArray<bool> src_cyclic = src_curves.cyclic();
  const Span<float3> src_positions = src_curves.positions();
  const bke::AttributeAccessor src_attributes = src_curves.attributes();
  IndexMaskMemory memory;
  const IndexMask unselected = selection.complement(src_curves.curves_range(), memory);

  bke::CurvesGeometry dst_curves = bke::curves::copy_only_curve_domain(src_curves);
  /* Copy vertex groups from source curves to allow copying vertex group attributes. */
  BKE_defgroup_copy_list(&dst_curves.vertex_group_names, &src_curves.vertex_group_names);
  dst_curves.fill_curve_types(selection, CURVE_TYPE_NURBS);

  MutableSpan<int> dst_offsets = dst_curves.offsets_for_write();
  offset_indices::copy_group_sizes(src_points_by_curve, unselected, dst_offsets);
  selection.foreach_index(GrainSize(1024), [&](const int i) {
    dst_offsets[i] = to_nurbs_size(CurveType(src_types[i]), src_points_by_curve[i].size());
  });
  offset_indices::accumulate_counts_to_offsets(dst_offsets);
  dst_curves.resize(dst_offsets.last(), dst_curves.curves_num());
  const OffsetIndices dst_points_by_curve = dst_curves.points_by_curve();

  MutableSpan<float3> dst_positions = dst_curves.positions_for_write();
  bke::MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();
  Vector<bke::AttributeTransferData> generic_attributes = bke::retrieve_attributes_for_transfer(
      src_attributes, dst_attributes, {bke::AttrDomain::Point}, attribute_filter);
  const Set<StringRef> attributes_to_skip = {"position",
                                             "handle_type_left",
                                             "handle_type_right",
                                             "handle_right",
                                             "handle_left",
                                             "nurbs_weight"};

  auto fill_weights_if_necessary = [&](const IndexMask &selection) {
    if (src_attributes.contains("nurbs_weight")) {
      bke::curves::fill_points(
          dst_points_by_curve, selection, 1.0f, dst_curves.nurbs_weights_for_write());
    }
  };

  auto catmull_rom_to_nurbs = [&](const IndexMask &selection) {
    index_mask::masked_fill<int8_t>(dst_curves.nurbs_orders_for_write(), 4, selection);
    index_mask::masked_fill<int8_t>(
        dst_curves.nurbs_knots_modes_for_write(), NURBS_KNOT_MODE_BEZIER, selection);
    fill_weights_if_necessary(selection);

    selection.foreach_segment(GrainSize(512), [&](const IndexMaskSegment segment) {
      for (const int i : segment) {
        const IndexRange src_points = src_points_by_curve[i];
        const IndexRange dst_points = dst_points_by_curve[i];
        catmull_rom_to_nurbs_positions(
            src_positions.slice(src_points), src_cyclic[i], dst_positions.slice(dst_points));
      }
    });

    for (bke::AttributeTransferData &attribute : generic_attributes) {
      if (attributes_to_skip.contains(attribute.name)) {
        continue;
      }
      selection.foreach_index(GrainSize(512), [&](const int i) {
        const IndexRange src_points = src_points_by_curve[i];
        const IndexRange dst_points = dst_points_by_curve[i];
        bezier_generic_to_nurbs(attribute.src.slice(src_points),
                                attribute.dst.span.slice(dst_points));
      });
    }
  };

  auto poly_to_nurbs = [&](const IndexMask &selection) {
    index_mask::masked_fill<int8_t>(dst_curves.nurbs_orders_for_write(), 4, selection);
    array_utils::copy_group_to_group(
        src_points_by_curve, dst_points_by_curve, selection, src_positions, dst_positions);
    fill_weights_if_necessary(selection);

    /* Avoid using "Endpoint" knots modes for cyclic curves, since it adds a sharp point at the
     * start/end. */
    if (src_cyclic.is_single()) {
      index_mask::masked_fill<int8_t>(dst_curves.nurbs_knots_modes_for_write(),
                                      src_cyclic.get_internal_single() ? NURBS_KNOT_MODE_NORMAL :
                                                                         NURBS_KNOT_MODE_ENDPOINT,
                                      selection);
    }
    else {
      VArraySpan<bool> cyclic{src_cyclic};
      MutableSpan<int8_t> knots_modes = dst_curves.nurbs_knots_modes_for_write();
      selection.foreach_index(GrainSize(1024), [&](const int i) {
        knots_modes[i] = cyclic[i] ? NURBS_KNOT_MODE_NORMAL : NURBS_KNOT_MODE_ENDPOINT;
      });
    }

    for (bke::AttributeTransferData &attribute : generic_attributes) {
      if (attributes_to_skip.contains(attribute.name)) {
        continue;
      }
      array_utils::copy_group_to_group(
          src_points_by_curve, dst_points_by_curve, selection, attribute.src, attribute.dst.span);
    }
  };

  auto bezier_to_nurbs = [&](const IndexMask &selection) {
    const Span<float3> src_handles_l = *src_curves.handle_positions_left();
    const Span<float3> src_handles_r = *src_curves.handle_positions_right();

    index_mask::masked_fill<int8_t>(dst_curves.nurbs_orders_for_write(), 4, selection);
    index_mask::masked_fill<int8_t>(
        dst_curves.nurbs_knots_modes_for_write(), NURBS_KNOT_MODE_BEZIER, selection);
    fill_weights_if_necessary(selection);

    selection.foreach_index(GrainSize(512), [&](const int i) {
      const IndexRange src_points = src_points_by_curve[i];
      const IndexRange dst_points = dst_points_by_curve[i];
      bezier_positions_to_nurbs(src_positions.slice(src_points),
                                src_handles_l.slice(src_points),
                                src_handles_r.slice(src_points),
                                dst_positions.slice(dst_points));
    });

    for (bke::AttributeTransferData &attribute : generic_attributes) {
      if (attributes_to_skip.contains(attribute.name)) {
        continue;
      }
      selection.foreach_index(GrainSize(512), [&](const int i) {
        const IndexRange src_points = src_points_by_curve[i];
        const IndexRange dst_points = dst_points_by_curve[i];
        bezier_generic_to_nurbs(attribute.src.slice(src_points),
                                attribute.dst.span.slice(dst_points));
      });
    }
  };

  auto nurbs_to_nurbs = [&](const IndexMask &selection) {
    array_utils::copy_group_to_group(
        src_points_by_curve, dst_points_by_curve, selection, src_positions, dst_positions);

    if (const std::optional<Span<float>> nurbs_weights = src_curves.nurbs_weights()) {
      array_utils::copy_group_to_group(src_points_by_curve,
                                       dst_points_by_curve,
                                       selection,
                                       *nurbs_weights,
                                       dst_curves.nurbs_weights_for_write());
    }

    for (bke::AttributeTransferData &attribute : generic_attributes) {
      if (attributes_to_skip.contains(attribute.name)) {
        continue;
      }
      array_utils::copy_group_to_group(
          src_points_by_curve, dst_points_by_curve, selection, attribute.src, attribute.dst.span);
    }
  };

  bke::curves::foreach_curve_by_type(src_curves.curve_types(),
                                     src_curves.curve_type_counts(),
                                     selection,
                                     catmull_rom_to_nurbs,
                                     poly_to_nurbs,
                                     bezier_to_nurbs,
                                     nurbs_to_nurbs);

  for (bke::AttributeTransferData &attribute : generic_attributes) {
    array_utils::copy_group_to_group(
        src_points_by_curve, dst_points_by_curve, unselected, attribute.src, attribute.dst.span);

    attribute.dst.finish();
  }
  bke::curves::nurbs::copy_custom_knots(src_curves, IndexMask(), dst_curves);
  return dst_curves;
}

static bke::CurvesGeometry convert_curves_trivial(const bke::CurvesGeometry &src_curves,
                                                  const IndexMask &selection,
                                                  const CurveType dst_type)
{
  bke::CurvesGeometry dst_curves(src_curves);
  dst_curves.fill_curve_types(selection, dst_type);
  dst_curves.remove_attributes_based_on_types();
  return dst_curves;
}

static bke::CurvesGeometry convert_curves_to_catmull_rom_or_poly(
    const bke::CurvesGeometry &src_curves,
    const IndexMask &selection,
    const CurveType dst_type,
    const bke::AttributeFilter &attribute_filter,
    const ConvertCurvesOptions &options)
{
  const bool use_bezier_handles = (dst_type == CURVE_TYPE_CATMULL_ROM) ?
                                      options.convert_bezier_handles_to_catmull_rom_points :
                                      options.convert_bezier_handles_to_poly_points;
  if (!use_bezier_handles || !src_curves.has_curve_with_type(CURVE_TYPE_BEZIER)) {
    return convert_curves_trivial(src_curves, selection, dst_type);
  }

  const OffsetIndices src_points_by_curve = src_curves.points_by_curve();
  const VArray<int8_t> src_types = src_curves.curve_types();
  const VArray<bool> src_cyclic = src_curves.cyclic();
  const Span<float3> src_positions = src_curves.positions();
  const bke::AttributeAccessor src_attributes = src_curves.attributes();
  IndexMaskMemory memory;
  const IndexMask unselected = selection.complement(src_curves.curves_range(), memory);

  bke::CurvesGeometry dst_curves = bke::curves::copy_only_curve_domain(src_curves);
  /* Copy vertex groups from source curves to allow copying vertex group attributes. */
  BKE_defgroup_copy_list(&dst_curves.vertex_group_names, &src_curves.vertex_group_names);
  dst_curves.fill_curve_types(selection, dst_type);

  MutableSpan<int> dst_offsets = dst_curves.offsets_for_write();
  offset_indices::copy_group_sizes(src_points_by_curve, unselected, dst_offsets);
  selection.foreach_index(GrainSize(1024), [&](const int i) {
    const IndexRange src_points = src_points_by_curve[i];
    const CurveType src_curve_type = CurveType(src_types[i]);
    int &size = dst_offsets[i];
    if (src_curve_type == CURVE_TYPE_BEZIER) {
      size = src_points.size() * 3;
    }
    else {
      size = src_points.size();
    }
  });
  offset_indices::accumulate_counts_to_offsets(dst_offsets);
  dst_curves.resize(dst_offsets.last(), dst_curves.curves_num());
  const OffsetIndices dst_points_by_curve = dst_curves.points_by_curve();

  MutableSpan<float3> dst_positions = dst_curves.positions_for_write();
  bke::MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();
  Vector<bke::AttributeTransferData> generic_attributes = bke::retrieve_attributes_for_transfer(
      src_attributes, dst_attributes, {bke::AttrDomain::Point}, attribute_filter);
  const Set<StringRef> attributes_to_skip = {"position",
                                             "handle_type_left",
                                             "handle_type_right",
                                             "handle_right",
                                             "handle_left",
                                             "nurbs_weight"};

  auto convert_from_catmull_rom_or_poly_or_nurbs = [&](const IndexMask &selection) {
    array_utils::copy_group_to_group(
        src_points_by_curve, dst_points_by_curve, selection, src_positions, dst_positions);
    for (bke::AttributeTransferData &attribute : generic_attributes) {
      if (attributes_to_skip.contains(attribute.name)) {
        continue;
      }
      array_utils::copy_group_to_group(
          src_points_by_curve, dst_points_by_curve, selection, attribute.src, attribute.dst.span);
    }
  };

  auto convert_from_bezier = [&](const IndexMask &selection) {
    const Span<float3> src_left_handles = *src_curves.handle_positions_left();
    const Span<float3> src_right_handles = *src_curves.handle_positions_right();

    /* Transfer positions. */
    selection.foreach_index([&](const int curve_i) {
      const IndexRange src_points = src_points_by_curve[curve_i];
      const IndexRange dst_points = dst_points_by_curve[curve_i];
      for (const int i : src_points.index_range()) {
        const int src_point_i = src_points[i];
        const int dst_points_start = dst_points.start() + 3 * i;
        dst_positions[dst_points_start + 0] = src_left_handles[src_point_i];
        dst_positions[dst_points_start + 1] = src_positions[src_point_i];
        dst_positions[dst_points_start + 2] = src_right_handles[src_point_i];
      }
    });
    /* Transfer attributes. The handles the same attribute values as their corresponding control
     * point. */
    for (bke::AttributeTransferData &attribute : generic_attributes) {
      if (attributes_to_skip.contains(attribute.name)) {
        continue;
      }
      const CPPType &cpp_type = attribute.src.type();
      selection.foreach_index([&](const int curve_i) {
        const IndexRange src_points = src_points_by_curve[curve_i];
        const IndexRange dst_points = dst_points_by_curve[curve_i];
        for (const int i : src_points.index_range()) {
          const int src_point_i = src_points[i];
          const int dst_points_start = dst_points.start() + 3 * i;
          const void *src_value = attribute.src[src_point_i];
          cpp_type.fill_assign_n(src_value, attribute.dst.span[dst_points_start], 3);
        }
      });
    }
  };

  bke::curves::foreach_curve_by_type(src_curves.curve_types(),
                                     src_curves.curve_type_counts(),
                                     selection,
                                     convert_from_catmull_rom_or_poly_or_nurbs,
                                     convert_from_catmull_rom_or_poly_or_nurbs,
                                     convert_from_bezier,
                                     convert_from_catmull_rom_or_poly_or_nurbs);

  for (bke::AttributeTransferData &attribute : generic_attributes) {
    array_utils::copy_group_to_group(
        src_points_by_curve, dst_points_by_curve, unselected, attribute.src, attribute.dst.span);

    attribute.dst.finish();
  }
  bke::curves::nurbs::copy_custom_knots(src_curves, selection, dst_curves);
  return dst_curves;
}

/**
 * Converts some curves to poly curves before they are converted to nurbs. This is useful because
 * it discards the bezier/catmull-rom shape which is sometimes the desired behavior.
 */
static bke::CurvesGeometry convert_bezier_or_catmull_rom_to_poly_before_conversion_to_nurbs(
    const bke::CurvesGeometry &src_curves,
    const IndexMask &selection,
    const ConvertCurvesOptions &options)
{
  const VArray<int8_t> src_curve_types = src_curves.curve_types();
  IndexMaskMemory memory;
  const IndexMask mask = IndexMask::from_predicate(
      selection, GrainSize(4096), memory, [&](const int curve_i) {
        const CurveType type = CurveType(src_curve_types[curve_i]);
        if (!options.keep_bezier_shape_as_nurbs && type == CURVE_TYPE_BEZIER) {
          return true;
        }
        if (!options.keep_catmull_rom_shape_as_nurbs && type == CURVE_TYPE_CATMULL_ROM) {
          return true;
        }
        return false;
      });
  return convert_curves_trivial(src_curves, mask, CURVE_TYPE_POLY);
}

bke::CurvesGeometry convert_curves(const bke::CurvesGeometry &src_curves,
                                   const IndexMask &selection,
                                   const CurveType dst_type,
                                   const bke::AttributeFilter &attribute_filter,
                                   const ConvertCurvesOptions &options)
{
  switch (dst_type) {
    case CURVE_TYPE_CATMULL_ROM:
    case CURVE_TYPE_POLY:
      return convert_curves_to_catmull_rom_or_poly(
          src_curves, selection, dst_type, attribute_filter, options);
    case CURVE_TYPE_BEZIER:
      return convert_curves_to_bezier(src_curves, selection, attribute_filter);
    case CURVE_TYPE_NURBS: {
      if (!options.keep_bezier_shape_as_nurbs || !options.keep_catmull_rom_shape_as_nurbs) {
        const bke::CurvesGeometry tmp_src_curves =
            convert_bezier_or_catmull_rom_to_poly_before_conversion_to_nurbs(
                src_curves, selection, options);
        return convert_curves_to_nurbs(tmp_src_curves, selection, attribute_filter);
      }
      return convert_curves_to_nurbs(src_curves, selection, attribute_filter);
    }
  }
  BLI_assert_unreachable();
  return {};
}

}  // namespace blender::geometry
