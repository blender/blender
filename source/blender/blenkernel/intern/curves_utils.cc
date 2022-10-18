/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BLI_index_mask_ops.hh"

#include "BKE_curves_utils.hh"

namespace blender::bke::curves {

void fill_curve_counts(const bke::CurvesGeometry &curves,
                       const Span<IndexRange> curve_ranges,
                       MutableSpan<int> counts)
{
  threading::parallel_for(curve_ranges.index_range(), 512, [&](IndexRange ranges_range) {
    for (const IndexRange curves_range : curve_ranges.slice(ranges_range)) {
      threading::parallel_for(curves_range, 4096, [&](IndexRange range) {
        for (const int i : range) {
          counts[i] = curves.points_for_curve(i).size();
        }
      });
    }
  });
}

void accumulate_counts_to_offsets(MutableSpan<int> counts_to_offsets, const int start_offset)
{
  int offset = start_offset;
  for (const int i : counts_to_offsets.index_range().drop_back(1)) {
    const int count = counts_to_offsets[i];
    BLI_assert(count > 0);
    counts_to_offsets[i] = offset;
    offset += count;
  }
  counts_to_offsets.last() = offset;
}

void copy_point_data(const CurvesGeometry &src_curves,
                     const CurvesGeometry &dst_curves,
                     const Span<IndexRange> curve_ranges,
                     const GSpan src,
                     GMutableSpan dst)
{
  threading::parallel_for(curve_ranges.index_range(), 512, [&](IndexRange range) {
    for (const IndexRange range : curve_ranges.slice(range)) {
      const IndexRange src_points = src_curves.points_for_curves(range);
      const IndexRange dst_points = dst_curves.points_for_curves(range);
      /* The arrays might be large, so a threaded copy might make sense here too. */
      dst.slice(dst_points).copy_from(src.slice(src_points));
    }
  });
}

void copy_point_data(const CurvesGeometry &src_curves,
                     const CurvesGeometry &dst_curves,
                     const IndexMask src_curve_selection,
                     const GSpan src,
                     GMutableSpan dst)
{
  threading::parallel_for(src_curve_selection.index_range(), 512, [&](IndexRange range) {
    for (const int i : src_curve_selection.slice(range)) {
      const IndexRange src_points = src_curves.points_for_curve(i);
      const IndexRange dst_points = dst_curves.points_for_curve(i);
      /* The arrays might be large, so a threaded copy might make sense here too. */
      dst.slice(dst_points).copy_from(src.slice(src_points));
    }
  });
}

void fill_points(const CurvesGeometry &curves,
                 const IndexMask curve_selection,
                 const GPointer value,
                 GMutableSpan dst)
{
  BLI_assert(*value.type() == dst.type());
  const CPPType &type = dst.type();
  threading::parallel_for(curve_selection.index_range(), 512, [&](IndexRange range) {
    for (const int i : curve_selection.slice(range)) {
      const IndexRange points = curves.points_for_curve(i);
      type.fill_assign_n(value.get(), dst.slice(curves.points_for_curve(i)).data(), points.size());
    }
  });
}

void fill_points(const CurvesGeometry &curves,
                 Span<IndexRange> curve_ranges,
                 GPointer value,
                 GMutableSpan dst)
{
  BLI_assert(*value.type() == dst.type());
  const CPPType &type = dst.type();
  threading::parallel_for(curve_ranges.index_range(), 512, [&](IndexRange range) {
    for (const IndexRange range : curve_ranges.slice(range)) {
      const IndexRange points = curves.points_for_curves(range);
      type.fill_assign_n(value.get(), dst.slice(points).data(), points.size());
    }
  });
}

bke::CurvesGeometry copy_only_curve_domain(const bke::CurvesGeometry &src_curves)
{
  bke::CurvesGeometry dst_curves(0, src_curves.curves_num());
  CustomData_copy(&src_curves.curve_data,
                  &dst_curves.curve_data,
                  CD_MASK_ALL,
                  CD_DUPLICATE,
                  src_curves.curves_num());
  dst_curves.runtime->type_counts = src_curves.runtime->type_counts;
  return dst_curves;
}

IndexMask indices_for_type(const VArray<int8_t> &types,
                           const std::array<int, CURVE_TYPES_NUM> &type_counts,
                           const CurveType type,
                           const IndexMask selection,
                           Vector<int64_t> &r_indices)
{
  if (type_counts[type] == types.size()) {
    return selection;
  }
  if (types.is_single()) {
    return types.get_internal_single() == type ? IndexMask(types.size()) : IndexMask(0);
  }
  Span<int8_t> types_span = types.get_internal_span();
  return index_mask_ops::find_indices_based_on_predicate(
      selection, 4096, r_indices, [&](const int index) { return types_span[index] == type; });
}

void foreach_curve_by_type(const VArray<int8_t> &types,
                           const std::array<int, CURVE_TYPES_NUM> &counts,
                           const IndexMask selection,
                           FunctionRef<void(IndexMask)> catmull_rom_fn,
                           FunctionRef<void(IndexMask)> poly_fn,
                           FunctionRef<void(IndexMask)> bezier_fn,
                           FunctionRef<void(IndexMask)> nurbs_fn)
{
  Vector<int64_t> indices;
  auto call_if_not_empty = [&](const CurveType type, FunctionRef<void(IndexMask)> fn) {
    indices.clear();
    const IndexMask mask = indices_for_type(types, counts, type, selection, indices);
    if (!mask.is_empty()) {
      fn(mask);
    }
  };
  call_if_not_empty(CURVE_TYPE_CATMULL_ROM, catmull_rom_fn);
  call_if_not_empty(CURVE_TYPE_POLY, poly_fn);
  call_if_not_empty(CURVE_TYPE_BEZIER, bezier_fn);
  call_if_not_empty(CURVE_TYPE_NURBS, nurbs_fn);
}

}  // namespace blender::bke::curves
