/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"

#include "BKE_attribute.hh"
#include "BKE_curves.hh"
#include "BKE_curves_utils.hh"
#include "BKE_deform.hh"

#include "GEO_curves_remove_and_split.hh"

namespace blender::geometry {

bke::CurvesGeometry remove_points_and_split(const bke::CurvesGeometry &curves,
                                            const IndexMask &mask)
{
  const OffsetIndices<int> points_by_curve = curves.points_by_curve();
  const VArray<bool> src_cyclic = curves.cyclic();

  Array<bool> points_to_delete(curves.points_num());
  mask.to_bools(points_to_delete.as_mutable_span());
  const int total_points = points_to_delete.as_span().count(false);

  /* Return if deleting everything. */
  if (total_points == 0) {
    return {};
  }

  int curr_dst_point_id = 0;
  Array<int> dst_to_src_point(total_points);
  Vector<int> dst_curve_counts;
  Vector<int> dst_to_src_curve;
  Vector<bool> dst_cyclic;

  for (const int curve_i : curves.curves_range()) {
    const IndexRange points = points_by_curve[curve_i];
    const Span<bool> curve_points_to_delete = points_to_delete.as_span().slice(points);
    const bool curve_cyclic = src_cyclic[curve_i];

    /* Note, these ranges start at zero and needed to be shifted by `points.first()` */
    const Vector<IndexRange> ranges_to_keep = array_utils::find_all_ranges(curve_points_to_delete,
                                                                           false);

    if (ranges_to_keep.is_empty()) {
      continue;
    }

    const bool is_last_segment_selected = curve_cyclic && ranges_to_keep.first().first() == 0 &&
                                          ranges_to_keep.last().last() == points.size() - 1;
    const bool is_curve_self_joined = is_last_segment_selected && ranges_to_keep.size() != 1;
    const bool is_cyclic = ranges_to_keep.size() == 1 && is_last_segment_selected;

    IndexRange range_ids = ranges_to_keep.index_range();
    /* Skip the first range because it is joined to the end of the last range. */
    for (const int range_i : ranges_to_keep.index_range().drop_front(is_curve_self_joined)) {
      const IndexRange range = ranges_to_keep[range_i];

      int count = range.size();
      for (const int src_point : range.shift(points.first())) {
        dst_to_src_point[curr_dst_point_id++] = src_point;
      }

      /* Join the first range to the end of the last range. */
      if (is_curve_self_joined && range_i == range_ids.last()) {
        const IndexRange first_range = ranges_to_keep[range_ids.first()];
        for (const int src_point : first_range.shift(points.first())) {
          dst_to_src_point[curr_dst_point_id++] = src_point;
        }
        count += first_range.size();
      }

      dst_curve_counts.append(count);
      dst_to_src_curve.append(curve_i);
      dst_cyclic.append(is_cyclic);
    }
  }

  const int total_curves = dst_to_src_curve.size();

  bke::CurvesGeometry dst_curves(total_points, total_curves);

  BKE_defgroup_copy_list(&dst_curves.vertex_group_names, &curves.vertex_group_names);

  MutableSpan<int> new_curve_offsets = dst_curves.offsets_for_write();
  array_utils::copy(dst_curve_counts.as_span(), new_curve_offsets.drop_back(1));
  offset_indices::accumulate_counts_to_offsets(new_curve_offsets);

  bke::MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();
  const bke::AttributeAccessor src_attributes = curves.attributes();

  /* Transfer curve attributes. */
  gather_attributes(src_attributes,
                    bke::AttrDomain::Curve,
                    bke::AttrDomain::Curve,
                    bke::attribute_filter_from_skip_ref({"cyclic"}),
                    dst_to_src_curve,
                    dst_attributes);
  array_utils::copy(dst_cyclic.as_span(), dst_curves.cyclic_for_write());

  /* Transfer point attributes. */
  gather_attributes(src_attributes,
                    bke::AttrDomain::Point,
                    bke::AttrDomain::Point,
                    {},
                    dst_to_src_point,
                    dst_attributes);

  dst_curves.update_curve_types();
  dst_curves.remove_attributes_based_on_types();

  if (curves.nurbs_has_custom_knots()) {
    bke::curves::nurbs::update_custom_knot_modes(
        dst_curves.curves_range(), NURBS_KNOT_MODE_NORMAL, NURBS_KNOT_MODE_NORMAL, dst_curves);
  }
  return dst_curves;
}

}  // namespace blender::geometry
