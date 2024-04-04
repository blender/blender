/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edcurves
 */

#include "BLI_array_utils.hh"

#include "BKE_attribute.hh"
#include "BKE_curves.hh"

#include "ED_curves.hh"

namespace blender::ed::curves {

bool remove_selection(bke::CurvesGeometry &curves, const bke::AttrDomain selection_domain)
{
  const bke::AttributeAccessor attributes = curves.attributes();
  const VArray<bool> selection = *attributes.lookup_or_default<bool>(
      ".selection", selection_domain, true);
  const int domain_size_orig = attributes.domain_size(selection_domain);
  IndexMaskMemory memory;
  const IndexMask mask = IndexMask::from_bools(selection, memory);
  switch (selection_domain) {
    case bke::AttrDomain::Point:
      curves.remove_points(mask, {});
      break;
    case bke::AttrDomain::Curve:
      curves.remove_curves(mask, {});
      break;
    default:
      BLI_assert_unreachable();
  }

  return attributes.domain_size(selection_domain) != domain_size_orig;
}

void duplicate_points(bke::CurvesGeometry &curves, const IndexMask &mask)
{
  const OffsetIndices<int> points_by_curve = curves.points_by_curve();
  const VArray<bool> src_cyclic = curves.cyclic();

  Array<bool> points_to_duplicate(curves.points_num());
  mask.to_bools(points_to_duplicate.as_mutable_span());
  const int num_points_to_add = mask.size();

  int curr_dst_point_start = 0;
  Array<int> dst_to_src_point(num_points_to_add);
  Vector<int> dst_curve_counts;
  Vector<int> dst_to_src_curve;
  Vector<bool> dst_cyclic;

  /* Add the duplicated curves and points. */
  for (const int curve_i : curves.curves_range()) {
    const IndexRange points = points_by_curve[curve_i];
    const Span<bool> curve_points_to_duplicate = points_to_duplicate.as_span().slice(points);
    const bool curve_cyclic = src_cyclic[curve_i];

    /* Note, these ranges start at zero and needed to be shifted by `points.first()` */
    const Vector<IndexRange> ranges_to_duplicate = array_utils::find_all_ranges(
        curve_points_to_duplicate, true);

    if (ranges_to_duplicate.is_empty()) {
      continue;
    }

    const bool is_last_segment_selected = curve_cyclic &&
                                          ranges_to_duplicate.first().first() == 0 &&
                                          ranges_to_duplicate.last().last() == points.size() - 1;
    const bool is_curve_self_joined = is_last_segment_selected && ranges_to_duplicate.size() != 1;
    const bool is_cyclic = ranges_to_duplicate.size() == 1 && is_last_segment_selected;

    const IndexRange range_ids = ranges_to_duplicate.index_range();
    /* Skip the first range because it is joined to the end of the last range. */
    for (const int range_i : ranges_to_duplicate.index_range().drop_front(is_curve_self_joined)) {
      const IndexRange range = ranges_to_duplicate[range_i];

      array_utils::fill_index_range<int>(
          dst_to_src_point.as_mutable_span().slice(curr_dst_point_start, range.size()),
          range.start() + points.first());
      curr_dst_point_start += range.size();

      dst_curve_counts.append(range.size());
      dst_to_src_curve.append(curve_i);
      dst_cyclic.append(is_cyclic);
    }

    /* Join the first range to the end of the last range. */
    if (is_curve_self_joined) {
      const IndexRange first_range = ranges_to_duplicate[range_ids.first()];
      array_utils::fill_index_range<int>(
          dst_to_src_point.as_mutable_span().slice(curr_dst_point_start, first_range.size()),
          first_range.start() + points.first());
      curr_dst_point_start += first_range.size();
      dst_curve_counts[dst_curve_counts.size() - 1] += first_range.size();
    }
  }

  const int old_curves_num = curves.curves_num();
  const int old_points_num = curves.points_num();
  const int num_curves_to_add = dst_to_src_curve.size();

  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();

  /* Delete selection attribute so that it will not have to be resized. */
  remove_selection_attributes(attributes);

  curves.resize(old_points_num + num_points_to_add, old_curves_num + num_curves_to_add);

  MutableSpan<int> new_curve_offsets = curves.offsets_for_write();
  array_utils::copy(dst_curve_counts.as_span(),
                    new_curve_offsets.drop_front(old_curves_num).drop_back(1));
  offset_indices::accumulate_counts_to_offsets(new_curve_offsets.drop_front(old_curves_num),
                                               old_points_num);

  /* Transfer curve and point attributes. */
  attributes.for_all([&](const bke::AttributeIDRef &id, const bke::AttributeMetaData meta_data) {
    bke::GSpanAttributeWriter attribute = attributes.lookup_for_write_span(id);
    if (!attribute) {
      return true;
    }

    switch (meta_data.domain) {
      case bke::AttrDomain::Curve: {
        if (id.name() == "cyclic") {
          attribute.finish();
          return true;
        }
        bke::attribute_math::gather(
            attribute.span,
            dst_to_src_curve,
            attribute.span.slice(IndexRange(old_curves_num, num_curves_to_add)));
        break;
      }
      case bke::AttrDomain::Point: {
        bke::attribute_math::gather(
            attribute.span,
            dst_to_src_point,
            attribute.span.slice(IndexRange(old_points_num, num_points_to_add)));
        break;
      }
      default: {
        attribute.finish();
        BLI_assert_unreachable();
        return true;
      }
    }

    attribute.finish();

    return true;
  });

  if (!(src_cyclic.is_single() && !src_cyclic.get_internal_single())) {
    array_utils::copy(dst_cyclic.as_span(), curves.cyclic_for_write().drop_front(old_curves_num));
  }

  curves.update_curve_types();
  curves.tag_topology_changed();

  for (const StringRef selection_name : get_curves_selection_attribute_names(curves)) {
    bke::SpanAttributeWriter<bool> selection = attributes.lookup_or_add_for_write_span<bool>(
        selection_name, bke::AttrDomain::Point);
    selection.span.take_back(num_points_to_add).fill(true);
    selection.finish();
  }
}

void duplicate_curves(bke::CurvesGeometry &curves, const IndexMask &mask)
{
  const int orig_points_num = curves.points_num();
  const int orig_curves_num = curves.curves_num();
  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();

  /* Delete selection attribute so that it will not have to be resized. */
  remove_selection_attributes(attributes);

  /* Resize the curves and copy the offsets of duplicated curves into the new offsets. */
  curves.resize(curves.points_num(), orig_curves_num + mask.size());
  const IndexRange orig_curves_range = curves.curves_range().take_front(orig_curves_num);
  const IndexRange new_curves_range = curves.curves_range().drop_front(orig_curves_num);

  MutableSpan<int> offset_data = curves.offsets_for_write();
  offset_indices::gather_selected_offsets(
      OffsetIndices<int>(offset_data.take_front(orig_curves_num + 1)),
      mask,
      orig_points_num,
      offset_data.drop_front(orig_curves_num));
  const OffsetIndices<int> points_by_curve = curves.points_by_curve();

  /* Resize the points array to match the new total point count. */
  curves.resize(points_by_curve.total_size(), curves.curves_num());

  attributes.for_all([&](const bke::AttributeIDRef &id, const bke::AttributeMetaData meta_data) {
    bke::GSpanAttributeWriter attribute = attributes.lookup_for_write_span(id);
    switch (meta_data.domain) {
      case bke::AttrDomain::Point:
        bke::attribute_math::gather_group_to_group(points_by_curve.slice(orig_curves_range),
                                                   points_by_curve.slice(new_curves_range),
                                                   mask,
                                                   attribute.span,
                                                   attribute.span);
        break;
      case bke::AttrDomain::Curve:
        array_utils::gather(attribute.span, mask, attribute.span.take_back(mask.size()));
        break;
      default:
        BLI_assert_unreachable();
        return true;
    }
    attribute.finish();
    return true;
  });

  curves.update_curve_types();
  curves.tag_topology_changed();

  for (const StringRef selection_name : get_curves_selection_attribute_names(curves)) {
    bke::SpanAttributeWriter<bool> selection = attributes.lookup_or_add_for_write_span<bool>(
        selection_name, bke::AttrDomain::Curve);
    selection.span.take_back(mask.size()).fill(true);
    selection.finish();
  }
}

}  // namespace blender::ed::curves
