/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_curves_utils.hh"
#include "BKE_customdata.hh"

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
  CustomData_init_from(
      &src_curves.curve_data, &dst_curves.curve_data, CD_MASK_ALL, src_curves.curves_num());
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

namespace bezier {

Array<float3> retrieve_all_positions(const bke::CurvesGeometry &curves,
                                     const IndexMask &curves_selection)
{
  if (curves.curves_num() == 0 || !curves.has_curve_with_type(CURVE_TYPE_BEZIER)) {
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
  if (curves_selection.is_empty() || curves.curves_num() == 0 ||
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

}  // namespace blender::bke::curves
