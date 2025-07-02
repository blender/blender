/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_curves_utils.hh"
#include "BKE_customdata.hh"

#include "BLI_array_utils.hh"

namespace blender::bke::curves {

IndexMask curve_to_point_selection(OffsetIndices<int> points_by_curve,
                                   const IndexMask &curve_selection,
                                   IndexMaskMemory &memory)
{
  Array<index_mask::IndexMask::Initializer> point_ranges(curve_selection.size());
  curve_selection.foreach_index(GrainSize(2048), [&](const int curve, const int pos) {
    point_ranges[pos] = points_by_curve[curve];
  });
  return IndexMask::from_initializers(point_ranges, memory);
}

void fill_points(const OffsetIndices<int> points_by_curve,
                 const IndexMask &curve_selection,
                 const GPointer value,
                 GMutableSpan dst)
{
  BLI_assert(*value.type() == dst.type());
  const CPPType &type = dst.type();
  curve_selection.foreach_index(GrainSize(512), [&](const int i) {
    const IndexRange points = points_by_curve[i];
    type.fill_assign_n(value.get(), dst.slice(points).data(), points.size());
  });
}

CurvesGeometry copy_only_curve_domain(const CurvesGeometry &src_curves)
{
  CurvesGeometry dst_curves(0, src_curves.curves_num());
  copy_attributes(src_curves.attributes(),
                  AttrDomain::Curve,
                  AttrDomain::Curve,
                  {},
                  dst_curves.attributes_for_write());
  dst_curves.runtime->type_counts = src_curves.runtime->type_counts;
  return dst_curves;
}

IndexMask indices_for_type(const VArray<int8_t> &types,
                           const std::array<int, CURVE_TYPES_NUM> &type_counts,
                           const CurveType type,
                           const IndexMask &selection,
                           IndexMaskMemory &memory)
{
  if (type_counts[type] == types.size()) {
    return selection;
  }
  if (types.is_single()) {
    return types.get_internal_single() == type ? IndexMask(types.size()) : IndexMask(0);
  }
  Span<int8_t> types_span = types.get_internal_span();
  return IndexMask::from_predicate(selection, GrainSize(4096), memory, [&](const int index) {
    return types_span[index] == type;
  });
}

void foreach_curve_by_type(const VArray<int8_t> &types,
                           const std::array<int, CURVE_TYPES_NUM> &counts,
                           const IndexMask &selection,
                           FunctionRef<void(IndexMask)> catmull_rom_fn,
                           FunctionRef<void(IndexMask)> poly_fn,
                           FunctionRef<void(IndexMask)> bezier_fn,
                           FunctionRef<void(IndexMask)> nurbs_fn)
{
  auto call_if_not_empty = [&](const CurveType type, FunctionRef<void(IndexMask)> fn) {
    IndexMaskMemory memory;
    const IndexMask mask = indices_for_type(types, counts, type, selection, memory);
    if (!mask.is_empty()) {
      fn(mask);
    }
  };
  call_if_not_empty(CURVE_TYPE_CATMULL_ROM, catmull_rom_fn);
  call_if_not_empty(CURVE_TYPE_POLY, poly_fn);
  call_if_not_empty(CURVE_TYPE_BEZIER, bezier_fn);
  call_if_not_empty(CURVE_TYPE_NURBS, nurbs_fn);
}

static void if_has_data_call_callback(const Span<int> offset_data,
                                      const int begin,
                                      const int end,
                                      UnselectedCallback callback)
{
  if (begin < end) {
    const IndexRange curves = IndexRange::from_begin_end(begin, end);
    const IndexRange points = IndexRange::from_begin_end(offset_data[begin], offset_data[end]);
    callback(curves, points);
  }
};

template<typename Fn>
static void foreach_selected_point_ranges_per_curve_(const IndexMask &mask,
                                                     const OffsetIndices<int> points_by_curve,
                                                     SelectedCallback selected_fn,
                                                     Fn unselected_fn)
{
  Vector<IndexRange> ranges;
  Span<int> offset_data = points_by_curve.data();

  int curve_i = mask.is_empty() ? -1 : 0;

  int range_first = mask.is_empty() ? 0 : mask.first();
  int range_last = range_first - 1;

  mask.foreach_index([&](const int64_t index) {
    if (offset_data[curve_i + 1] <= index) {
      int first_unselected_curve = curve_i;
      if (range_last >= range_first) {
        ranges.append(IndexRange::from_begin_end_inclusive(range_first, range_last));
        selected_fn(curve_i, points_by_curve[curve_i], ranges);
        ranges.clear();
        first_unselected_curve++;
      }
      do {
        ++curve_i;
      } while (offset_data[curve_i + 1] <= index);
      if constexpr (std::is_invocable_r_v<void, Fn, IndexRange, IndexRange>) {
        if_has_data_call_callback(offset_data, first_unselected_curve, curve_i, unselected_fn);
      }
      range_first = index;
    }
    else if (range_last + 1 != index) {
      ranges.append(IndexRange::from_begin_end_inclusive(range_first, range_last));
      range_first = index;
    }
    range_last = index;
  });

  if (range_last - range_first >= 0) {
    ranges.append(IndexRange::from_begin_end_inclusive(range_first, range_last));
    selected_fn(curve_i, points_by_curve[curve_i], ranges);
  }
  if constexpr (std::is_invocable_r_v<void, Fn, IndexRange, IndexRange>) {
    if_has_data_call_callback(offset_data, curve_i + 1, points_by_curve.size(), unselected_fn);
  }
}

void foreach_selected_point_ranges_per_curve(const IndexMask &mask,
                                             const OffsetIndices<int> offset_indices,
                                             SelectedCallback selected_fn)
{
  foreach_selected_point_ranges_per_curve_<void()>(mask, offset_indices, selected_fn, nullptr);
}

void foreach_selected_point_ranges_per_curve(const IndexMask &mask,
                                             const OffsetIndices<int> offset_indices,
                                             SelectedCallback selected_fn,
                                             UnselectedCallback unselected_fn)
{
  foreach_selected_point_ranges_per_curve_<UnselectedCallback>(
      mask, offset_indices, selected_fn, unselected_fn);
}

namespace bezier {

Array<float3> retrieve_all_positions(const bke::CurvesGeometry &curves,
                                     const IndexMask &curves_selection)
{
  if (curves.is_empty() || !curves.has_curve_with_type(CURVE_TYPE_BEZIER)) {
    return {};
  }
  const OffsetIndices points_by_curve = curves.points_by_curve();
  const Span<float3> positions = curves.positions();
  const Span<float3> handle_positions_left = curves.handle_positions_left();
  const Span<float3> handle_positions_right = curves.handle_positions_right();

  Array<float3> all_positions(positions.size() * 3);
  curves_selection.foreach_index(GrainSize(1024), [&](const int curve) {
    const IndexRange points = points_by_curve[curve];
    for (const int point : points) {
      const int index = point * 3;
      all_positions[index] = handle_positions_left[point];
      all_positions[index + 1] = positions[point];
      all_positions[index + 2] = handle_positions_right[point];
    }
  });

  return all_positions;
}

void write_all_positions(bke::CurvesGeometry &curves,
                         const IndexMask &curves_selection,
                         const Span<float3> all_positions)
{
  if (curves_selection.is_empty() || curves.is_empty() ||
      !curves.has_curve_with_type(CURVE_TYPE_BEZIER))
  {
    return;
  }
  BLI_assert(curves_selection.size() * 3 == all_positions.size());

  const OffsetIndices points_by_curve = curves.points_by_curve();
  MutableSpan<float3> positions = curves.positions_for_write();
  MutableSpan<float3> handle_positions_left = curves.handle_positions_left_for_write();
  MutableSpan<float3> handle_positions_right = curves.handle_positions_right_for_write();

  curves_selection.foreach_index(GrainSize(1024), [&](const int curve) {
    const IndexRange points = points_by_curve[curve];
    for (const int point : points) {
      const int index = point * 3;
      handle_positions_left[point] = all_positions[index];
      positions[point] = all_positions[index + 1];
      handle_positions_right[point] = all_positions[index + 2];
    }
  });
}

}  // namespace bezier

namespace nurbs {

void gather_custom_knots(const bke::CurvesGeometry &src,
                         const IndexMask &src_curves,
                         const int dst_curve_offset,
                         bke::CurvesGeometry &dst)
{
  const OffsetIndices<int> src_knots_by_curve = src.nurbs_custom_knots_by_curve();
  const int start_offset = dst.nurbs_custom_knots_by_curve()[dst_curve_offset].start();
  Array<int> dst_offsets(src_curves.size() + 1);

  offset_indices::gather_selected_offsets(
      src_knots_by_curve, src_curves, start_offset, dst_offsets);

  array_utils::gather_group_to_group(src_knots_by_curve,
                                     dst_offsets.as_span(),
                                     src_curves,
                                     src.nurbs_custom_knots(),
                                     dst.nurbs_custom_knots_for_write());
}

void update_custom_knot_modes(const IndexMask &mask,
                              const KnotsMode mode_for_regular,
                              const KnotsMode mode_for_cyclic,
                              bke::CurvesGeometry &curves)
{
  const VArray<bool> cyclic = curves.cyclic();
  MutableSpan<int8_t> knot_modes = curves.nurbs_knots_modes_for_write();
  mask.foreach_index(GrainSize(512), [&](const int64_t curve) {
    int8_t &knot_mode = knot_modes[curve];
    if (knot_mode == NURBS_KNOT_MODE_CUSTOM) {
      knot_mode = cyclic[curve] ? mode_for_cyclic : mode_for_regular;
    }
  });
  curves.nurbs_custom_knots_update_size();
}

void copy_custom_knots(const bke::CurvesGeometry &src_curves,
                       const IndexMask &exclude_curves,
                       bke::CurvesGeometry &dst_curves)
{
  BLI_assert(src_curves.curves_num() == dst_curves.curves_num());

  if (src_curves.nurbs_has_custom_knots()) {
    /* Ensure excluded curves don't have NURBS_KNOT_MODE_CUSTOM set. */
    bke::curves::nurbs::update_custom_knot_modes(
        exclude_curves, NURBS_KNOT_MODE_NORMAL, NURBS_KNOT_MODE_NORMAL, dst_curves);
    IndexMaskMemory memory;
    bke::curves::nurbs::gather_custom_knots(
        src_curves,
        IndexMask::from_difference(
            src_curves.nurbs_custom_knot_curves(memory), exclude_curves, memory),
        0,
        dst_curves);
  }
}

}  // namespace nurbs

}  // namespace blender::bke::curves
