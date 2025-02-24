/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edcurves
 */

#include "BLI_array_utils.hh"

#include "BKE_anonymous_attribute_id.hh"
#include "BKE_attribute.hh"
#include "BKE_curves.hh"
#include "BKE_curves_utils.hh"

#include "GEO_reorder.hh"

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

static void curve_offsets_from_selection(const Span<IndexRange> selected_points,
                                         const IndexRange points,
                                         const int curve,
                                         const bool cyclic,
                                         Vector<int> &r_new_curve_offsets,
                                         Vector<bool> &r_new_cyclic,
                                         Vector<IndexRange> &r_src_ranges,
                                         Vector<int> &r_dst_offsets,
                                         Vector<int> &r_dst_to_src_curve)
{
  const bool merge_loop = cyclic && selected_points.first().size() < points.size() &&
                          selected_points.first().first() == points.first() &&
                          selected_points.last().last() == points.last();

  int last_dst_offset = r_dst_offsets.last();
  int last_curve_offset = r_new_curve_offsets.last();
  for (const IndexRange range : selected_points.drop_front(merge_loop)) {
    r_src_ranges.append(range);
    last_dst_offset += range.size();
    r_dst_offsets.append(last_dst_offset);
    last_curve_offset += range.size();
    r_new_curve_offsets.append(last_curve_offset);
  };
  if (merge_loop) {
    const IndexRange merge_to_end = selected_points.first();
    r_src_ranges.append(merge_to_end);
    r_dst_offsets.append(last_dst_offset + merge_to_end.size());
    r_new_curve_offsets.last() += merge_to_end.size();
  }
  const int curves_added = selected_points.size() - merge_loop;
  r_dst_to_src_curve.append_n_times(curve, curves_added);
  r_new_cyclic.append_n_times(cyclic && selected_points.first().size() == points.size(),
                              curves_added);
}

void duplicate_points(bke::CurvesGeometry &curves, const IndexMask &mask)
{
  const OffsetIndices<int> points_by_curve = curves.points_by_curve();
  const VArray<bool> src_cyclic = curves.cyclic();

  Vector<int> dst_to_src_curve;
  Vector<int> new_curve_offsets({points_by_curve.data().last()});
  Vector<IndexRange> src_ranges;
  Vector<int> dst_offsets({0});
  Vector<bool> dst_cyclic;
  dst_to_src_curve.reserve(curves.curves_num());
  new_curve_offsets.reserve(curves.curves_num() + 1);
  src_ranges.reserve(curves.curves_num());
  dst_offsets.reserve(curves.curves_num() + 1);
  dst_cyclic.reserve(curves.curves_num());

  /* Add the duplicated curves and points. */
  bke::curves::foreach_selected_point_ranges_per_curve(
      mask,
      points_by_curve,
      [&](const int curve, const IndexRange points, Span<IndexRange> ranges_to_duplicate) {
        curve_offsets_from_selection(ranges_to_duplicate,
                                     points,
                                     curve,
                                     src_cyclic[curve],
                                     new_curve_offsets,
                                     dst_cyclic,
                                     src_ranges,
                                     dst_offsets,
                                     dst_to_src_curve);
      });

  const int old_curves_num = curves.curves_num();
  const int old_points_num = curves.points_num();
  const int num_curves_to_add = dst_to_src_curve.size();
  const int num_points_to_add = mask.size();

  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();

  /* Delete selection attribute so that it will not have to be resized. */
  remove_selection_attributes(attributes);

  curves.resize(old_points_num + num_points_to_add, old_curves_num + num_curves_to_add);

  array_utils::copy(new_curve_offsets.as_span(),
                    curves.offsets_for_write().drop_front(old_curves_num));

  /* Transfer curve and point attributes. */
  attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    bke::GSpanAttributeWriter attribute = attributes.lookup_for_write_span(iter.name);
    if (!attribute) {
      return;
    }

    switch (iter.domain) {
      case bke::AttrDomain::Curve: {
        if (iter.name == "cyclic") {
          attribute.finish();
          return;
        }
        bke::attribute_math::gather(
            attribute.span,
            dst_to_src_curve,
            attribute.span.slice(IndexRange(old_curves_num, num_curves_to_add)));
        break;
      }
      case bke::AttrDomain::Point: {
        bke::attribute_math::gather_ranges_to_groups(
            src_ranges.as_span(),
            dst_offsets.as_span(),
            attribute.span,
            attribute.span.slice(IndexRange(old_points_num, num_points_to_add)));
        break;
      }
      default: {
        attribute.finish();
        BLI_assert_unreachable();
        return;
      }
    }

    attribute.finish();
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

  attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    bke::GSpanAttributeWriter attribute = attributes.lookup_for_write_span(iter.name);
    switch (iter.domain) {
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
        return;
    }
    attribute.finish();
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

static void invert_ranges(const IndexRange universe,
                          const Span<IndexRange> ranges,
                          Array<IndexRange> &inverted)
{
  const bool contains_first = ranges.first().first() == universe.first();
  const bool contains_last = ranges.last().last() == universe.last();
  inverted.reinitialize(ranges.size() - 1 + !contains_first + !contains_last);

  int64_t start = contains_first ? ranges.first().one_after_last() : universe.first();
  int i = 0;
  for (const IndexRange range : ranges.drop_front(contains_first)) {
    inverted[i++] = IndexRange::from_begin_end(start, range.first());
    start = range.one_after_last();
  }
  if (!contains_last) {
    inverted.last() = IndexRange::from_begin_end(start, universe.one_after_last());
  }
}

static IndexRange extend_range(const IndexRange range, const IndexRange universe)
{
  return IndexRange::from_begin_end_inclusive(math::max(range.start() - 1, universe.start()),
                                              math::min(range.one_after_last(), universe.last()));
}

/**
 * Extends each range by one point at both ends of it. Merges adjacent ranges if intersections
 * occur.
 */
static void extend_range_by_1_within_bounds(const IndexRange universe,
                                            const bool cyclic,
                                            const Span<IndexRange> ranges,
                                            Vector<IndexRange> &extended_ranges)
{
  extended_ranges.clear();
  if (ranges.is_empty()) {
    return;
  }

  const bool first_match = ranges.first().first() == universe.first();
  const bool last_match = ranges.last().last() == universe.last();
  const bool add_first = cyclic && last_match && !first_match;
  const bool add_last = cyclic && first_match && !last_match;

  IndexRange current = add_first ? IndexRange::from_single(universe.first()) :
                                   extend_range(ranges.first(), universe);
  for (const IndexRange range : ranges.drop_front(!add_first)) {
    const IndexRange extended = extend_range(range, universe);
    if (extended.first() <= current.last()) {
      current = IndexRange::from_begin_end_inclusive(current.start(), extended.last());
    }
    else {
      extended_ranges.append(current);
      current = extended;
    }
  }
  extended_ranges.append(current);
  if (add_last) {
    extended_ranges.append(IndexRange::from_single(universe.last()));
  }
}

static void copy_data_to_geometry(const bke::CurvesGeometry &src_curves,
                                  const Span<int> dst_to_src_curve,
                                  const Span<int> offsets,
                                  const Span<bool> cyclic,
                                  const Span<IndexRange> src_ranges,
                                  const OffsetIndices<int> dst_offsets,
                                  bke::CurvesGeometry &dst_curves)
{
  dst_curves.resize(offsets.last(), dst_to_src_curve.size());

  array_utils::copy(offsets, dst_curves.offsets_for_write());
  dst_curves.cyclic_for_write().copy_from(cyclic);

  const bke::AttributeAccessor src_attributes = src_curves.attributes();
  bke::MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();

  bke::gather_attributes(src_attributes,
                         bke::AttrDomain::Curve,
                         bke::AttrDomain::Curve,
                         bke::attribute_filter_from_skip_ref({"cyclic"}),
                         dst_to_src_curve,
                         dst_attributes);

  for (auto &attribute : bke::retrieve_attributes_for_transfer(
           src_attributes,
           dst_attributes,
           ATTR_DOMAIN_MASK_POINT,
           bke::attribute_filter_from_skip_ref(
               ed::curves::get_curves_selection_attribute_names(src_curves))))
  {
    bke::attribute_math::gather_ranges_to_groups(
        src_ranges, dst_offsets, attribute.src, attribute.dst.span);
    attribute.dst.finish();
  };

  dst_curves.update_curve_types();
  dst_curves.tag_topology_changed();
}

bke::CurvesGeometry split_points(const bke::CurvesGeometry &curves,
                                 const IndexMask &points_to_split)
{
  const OffsetIndices points_by_curve = curves.points_by_curve();
  const VArray<bool> cyclic = curves.cyclic();

  Vector<int> curve_map;
  Vector<int> new_offsets({0});

  Vector<IndexRange> src_ranges;
  Vector<int> dst_offsets({0});
  Vector<bool> new_cyclic;

  Vector<IndexRange> deselect;

  Array<IndexRange> unselected_curve_points;
  Vector<IndexRange> curve_points_to_preserve;

  bke::curves::foreach_selected_point_ranges_per_curve(
      points_to_split,
      points_by_curve,
      [&](const int curve, const IndexRange points, const Span<IndexRange> selected_curve_points) {
        const int points_start = new_offsets.last();
        curve_offsets_from_selection(selected_curve_points,
                                     points,
                                     curve,
                                     cyclic[curve],
                                     new_offsets,
                                     new_cyclic,
                                     src_ranges,
                                     dst_offsets,
                                     curve_map);
        const int split_points_num = new_offsets.last() - points_start;
        /* Invert ranges to get non selected points. */
        invert_ranges(points, selected_curve_points, unselected_curve_points);
        /* Extended every range to left and right by one point. Any resulting intersection is
         * merged. */
        extend_range_by_1_within_bounds(
            points, cyclic[curve], unselected_curve_points, curve_points_to_preserve);
        const int size_before = curve_map.size();
        curve_offsets_from_selection(curve_points_to_preserve,
                                     points,
                                     curve,
                                     cyclic[curve] &&
                                         (split_points_num <= curve_points_to_preserve.size()),
                                     new_offsets,
                                     new_cyclic,
                                     src_ranges,
                                     dst_offsets,
                                     curve_map);
        deselect.append(IndexRange::from_begin_end(size_before, curve_map.size()));
      },
      [&](const IndexRange curves, const IndexRange points) {
        deselect.append(IndexRange::from_begin_size(curve_map.size(), curves.size()));
        src_ranges.append(points);
        dst_offsets.append(dst_offsets.last() + points.size());
        int last_offset = new_offsets.last();
        for (const int curve : curves) {
          last_offset += points_by_curve[curve].size();
          new_offsets.append(last_offset);
          curve_map.append(curve);
          new_cyclic.append(cyclic[curve]);
        }
      });

  bke::CurvesGeometry new_curves;
  copy_data_to_geometry(
      curves, curve_map, new_offsets, new_cyclic, src_ranges, dst_offsets.as_span(), new_curves);

  OffsetIndices<int> new_points_by_curve = new_curves.points_by_curve();
  foreach_selection_attribute_writer(
      new_curves, bke::AttrDomain::Point, [&](bke::GSpanAttributeWriter &selection) {
        for (const IndexRange curves : deselect) {
          for (const int curve : curves) {
            fill_selection_false(selection.span.slice(new_points_by_curve[curve]));
          }
        }
      });

  return new_curves;
}

void add_curves(bke::CurvesGeometry &curves, const Span<int> new_sizes)
{
  const int orig_points_num = curves.points_num();
  const int orig_curves_num = curves.curves_num();
  curves.resize(orig_points_num, orig_curves_num + new_sizes.size());

  /* Find the final number of points by accumulating the new */
  MutableSpan<int> new_offsets = curves.offsets_for_write().drop_front(orig_curves_num);
  new_offsets.drop_back(1).copy_from(new_sizes);
  offset_indices::accumulate_counts_to_offsets(new_offsets, orig_points_num);
  /* First, resize the curve domain. */
  curves.resize(curves.offsets().last(), curves.curves_num());

  /* Initialize new attribute values, since #CurvesGeometry::resize() doesn't do that. */
  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  bke::fill_attribute_range_default(
      attributes, bke::AttrDomain::Point, {}, curves.points_range().drop_front(orig_points_num));
  bke::fill_attribute_range_default(
      attributes, bke::AttrDomain::Curve, {}, curves.curves_range().drop_front(orig_curves_num));

  curves.update_curve_types();
}

void resize_curves(bke::CurvesGeometry &curves,
                   const IndexMask &curves_to_resize,
                   const Span<int> new_sizes)
{
  if (curves_to_resize.is_empty()) {
    return;
  }
  BLI_assert(curves_to_resize.size() == new_sizes.size());
  bke::CurvesGeometry dst_curves = bke::curves::copy_only_curve_domain(curves);

  IndexMaskMemory memory;
  IndexMask curves_to_copy;
  std::optional<IndexRange> range = curves_to_resize.to_range();
  /* Check if we need to copy some curves over. Write the new sizes into the offsets. */
  if (range && curves.curves_range() == *range) {
    curves_to_copy = {};
    dst_curves.offsets_for_write().drop_back(1).copy_from(new_sizes);
  }
  else {
    curves_to_copy = curves_to_resize.complement(curves.curves_range(), memory);
    offset_indices::copy_group_sizes(
        curves.offsets(), curves_to_copy, dst_curves.offsets_for_write());
    array_utils::scatter(new_sizes, curves_to_resize, dst_curves.offsets_for_write());
  }
  /* Accumulate the sizes written from `new_sizes` into offsets. */
  offset_indices::accumulate_counts_to_offsets(dst_curves.offsets_for_write());

  /* Resize the points domain. */
  dst_curves.resize(dst_curves.offsets().last(), dst_curves.curves_num());

  /* Copy point attributes and default initialize newly added point ranges. */
  const bke::AttrDomain domain(bke::AttrDomain::Point);
  const OffsetIndices<int> src_offsets = curves.points_by_curve();
  const OffsetIndices<int> dst_offsets = dst_curves.points_by_curve();
  const bke::AttributeAccessor src_attributes = curves.attributes();
  bke::MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();
  src_attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    if (iter.domain != domain || bke::attribute_name_is_anonymous(iter.name)) {
      return;
    }
    const GVArraySpan src = *iter.get(domain);
    const CPPType &type = src.type();
    bke::GSpanAttributeWriter dst = dst_attributes.lookup_or_add_for_write_only_span(
        iter.name, domain, iter.data_type);
    if (!dst) {
      return;
    }

    curves_to_resize.foreach_index(GrainSize(512), [&](const int curve_i) {
      const IndexRange src_points = src_offsets[curve_i];
      const IndexRange dst_points = dst_offsets[curve_i];
      if (dst_points.size() < src_points.size()) {
        const int src_excees = src_points.size() - dst_points.size();
        dst.span.slice(dst_points).copy_from(src.slice(src_points.drop_back(src_excees)));
      }
      else {
        const int dst_excees = dst_points.size() - src_points.size();
        dst.span.slice(dst_points.drop_back(dst_excees)).copy_from(src.slice(src_points));
        GMutableSpan dst_end_slice = dst.span.slice(dst_points.take_back(dst_excees));
        type.value_initialize_n(dst_end_slice.data(), dst_end_slice.size());
      }
    });
    array_utils::copy_group_to_group(src_offsets, dst_offsets, curves_to_copy, src, dst.span);
    dst.finish();
  });

  dst_curves.update_curve_types();

  /* Move the result into `curves`. */
  curves = std::move(dst_curves);
  curves.tag_topology_changed();
}

void reorder_curves(bke::CurvesGeometry &curves, const Span<int> old_by_new_indices_map)
{
  curves = geometry::reorder_curves_geometry(curves, old_by_new_indices_map, {});
}

}  // namespace blender::ed::curves
