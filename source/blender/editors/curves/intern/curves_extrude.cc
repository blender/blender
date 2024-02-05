/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_curves_utils.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_curves.hh"

#include "DEG_depsgraph.hh"

namespace blender::ed::curves {

/**
 * Merges copy intervals at curve endings to minimize number of copy operations.
 * For example given in function 'extrude_curves' intervals [0, 3, 4, 4, 4] became [0, 4, 4].
 * Leading to only two copy operations.
 */
static Span<int> compress_intervals(const Span<IndexRange> curve_interval_ranges,
                                    MutableSpan<int> intervals)
{
  const int *src = intervals.data();
  /* Skip the first curve, as all the data stays in the same place. */
  int *dst = intervals.data() + curve_interval_ranges[0].size();

  for (const int curve : IndexRange(1, curve_interval_ranges.size() - 1)) {
    const IndexRange range = curve_interval_ranges[curve];
    const int width = range.size() - 1;
    std::copy_n(src + range.first() + 1, width, dst);
    dst += width;
  }
  (*dst) = src[curve_interval_ranges[curve_interval_ranges.size() - 1].last() + 1];
  return {intervals.data(), dst - intervals.data() + 1};
}

/**
 * Creates copy intervals for selection #range in the context of #curve_index.
 * If part of the #range is outside given curve, slices it and returns false indicating remaining
 * still needs to be handled. If whole #range was handled returns true.
 */
static bool handle_range(const int curve_index,
                         const int interval_offset,
                         const Span<int> offsets,
                         int &current_interval,
                         IndexRange &range,
                         MutableSpan<int> curve_intervals,
                         MutableSpan<bool> is_first_selected)
{
  const int first_elem = offsets[curve_index];
  const int last_elem = offsets[curve_index + 1] - 1;

  if (current_interval == 0) {
    is_first_selected[curve_index] = range.first() == first_elem && range.size() == 1;
    if (!is_first_selected[curve_index]) {
      current_interval++;
    }
  }
  curve_intervals[interval_offset + current_interval] = range.first();
  current_interval++;

  bool inside_curve = last_elem >= range.last();
  if (inside_curve) {
    curve_intervals[interval_offset + current_interval] = range.last();
  }
  else {
    curve_intervals[interval_offset + current_interval] = last_elem;
    range = IndexRange(last_elem + 1, range.last() - last_elem);
  }
  current_interval++;
  return inside_curve;
}

/**
 * Calculates number of points in resulting curve denoted by #curve_index and sets its
 * #curve_offsets value.
 */
static void calc_curve_offset(const int curve_index,
                              int &interval_offset,
                              const Span<int> offsets,
                              MutableSpan<int> new_offsets,
                              MutableSpan<IndexRange> curve_interval_ranges)
{
  const int points_in_curve = (offsets[curve_index + 1] - offsets[curve_index] +
                               curve_interval_ranges[curve_index].size() - 1);
  new_offsets[curve_index + 1] = new_offsets[curve_index] + points_in_curve;
  interval_offset += curve_interval_ranges[curve_index].size() + 1;
}

static void finish_curve(int &curve_index,
                         int &interval_offset,
                         int last_interval,
                         int last_elem,
                         const Span<int> offsets,
                         MutableSpan<int> new_offsets,
                         MutableSpan<int> curve_intervals,
                         MutableSpan<IndexRange> curve_interval_ranges,
                         MutableSpan<bool> is_first_selected)
{
  if (curve_intervals[interval_offset + last_interval] != last_elem ||
      curve_intervals[interval_offset + last_interval - 1] !=
          curve_intervals[interval_offset + last_interval])
  {
    /* Append last element of the current curve if it is not extruded or extruded together with
     * preceding points. */
    last_interval++;
    curve_intervals[interval_offset + last_interval] = last_elem;
  }
  else if (is_first_selected[curve_index] && last_interval == 1) {
    /* Extrusion from one point. */
    curve_intervals[interval_offset + last_interval + 1] =
        curve_intervals[interval_offset + last_interval];
    is_first_selected[curve_index] = false;
    last_interval++;
  }
  curve_interval_ranges[curve_index] = IndexRange(interval_offset, last_interval);
  calc_curve_offset(curve_index, interval_offset, offsets, new_offsets, curve_interval_ranges);
  curve_index++;
}

static void finish_curve_or_full_copy(int &curve_index,
                                      int &interval_offset,
                                      int current_interval,
                                      const std::optional<IndexRange> prev_range,
                                      const Span<int> offsets,
                                      MutableSpan<int> new_offsets,
                                      MutableSpan<int> curve_intervals,
                                      MutableSpan<IndexRange> curve_interval_ranges,
                                      MutableSpan<bool> is_first_selected)
{
  const int last = offsets[curve_index + 1] - 1;

  if (prev_range.has_value() && prev_range.value().last() >= offsets[curve_index]) {
    finish_curve(curve_index,
                 interval_offset,
                 current_interval - 1,
                 last,
                 offsets,
                 new_offsets,
                 curve_intervals,
                 curve_interval_ranges,
                 is_first_selected);
  }
  else {
    /* Copy full curve if previous selected point was not on this curve. */
    const int first = offsets[curve_index];
    curve_interval_ranges[curve_index] = IndexRange(interval_offset, 1);
    is_first_selected[curve_index] = false;
    curve_intervals[interval_offset] = first;
    curve_intervals[interval_offset + 1] = last;
    calc_curve_offset(curve_index, interval_offset, offsets, new_offsets, curve_interval_ranges);
    curve_index++;
  }
}

static void calc_curves_extrusion(const IndexMask &selection,
                                  const Span<int> offsets,
                                  MutableSpan<int> new_offsets,
                                  MutableSpan<int> curve_intervals,
                                  MutableSpan<IndexRange> curve_interval_ranges,
                                  MutableSpan<bool> is_first_selected)
{
  std::optional<IndexRange> prev_range;
  int current_interval = 0;

  int curve_index = 0;
  int interval_offset = 0;
  curve_intervals[interval_offset] = offsets[0];
  new_offsets[0] = offsets[0];

  selection.foreach_range([&](const IndexRange range) {
    /* Beginning of the range outside current curve. */
    if (range.first() > offsets[curve_index + 1] - 1) {
      do {
        finish_curve_or_full_copy(curve_index,
                                  interval_offset,
                                  current_interval,
                                  prev_range,
                                  offsets,
                                  new_offsets,
                                  curve_intervals,
                                  curve_interval_ranges,
                                  is_first_selected);
      } while (range.first() > offsets[curve_index + 1] - 1);
      current_interval = 0;
      curve_intervals[interval_offset] = offsets[curve_index];
    }

    IndexRange range_to_handle = range;
    while (!handle_range(curve_index,
                         interval_offset,
                         offsets,
                         current_interval,
                         range_to_handle,
                         curve_intervals,
                         is_first_selected))
    {
      finish_curve(curve_index,
                   interval_offset,
                   current_interval - 1,
                   offsets[curve_index + 1] - 1,
                   offsets,
                   new_offsets,
                   curve_intervals,
                   curve_interval_ranges,
                   is_first_selected);
      current_interval = 0;
      curve_intervals[interval_offset] = offsets[curve_index];
    }
    prev_range = range;
  });

  do {
    finish_curve_or_full_copy(curve_index,
                              interval_offset,
                              current_interval,
                              prev_range,
                              offsets,
                              new_offsets,
                              curve_intervals,
                              curve_interval_ranges,
                              is_first_selected);
    prev_range.reset();
  } while (curve_index < offsets.size() - 1);
}

static void extrude_curves(Curves &curves_id)
{
  const bke::AttrDomain selection_domain = bke::AttrDomain(curves_id.selection_domain);
  if (selection_domain != bke::AttrDomain::Point) {
    return;
  }

  IndexMaskMemory memory;
  const IndexMask extruded_points = retrieve_selected_points(curves_id, memory);
  if (extruded_points.is_empty()) {
    return;
  }

  const bke::CurvesGeometry &curves = curves_id.geometry.wrap();
  const Span<int> old_offsets = curves.offsets();

  bke::CurvesGeometry new_curves = bke::curves::copy_only_curve_domain(curves);

  const int curves_num = curves.curves_num();
  const int curve_intervals_size = extruded_points.size() * 2 + curves_num * 2;

  MutableSpan<int> new_offsets = new_curves.offsets_for_write();

  /* Buffer for intervals of all curves. Beginning and end of a curve can be determined only by
   * #curve_interval_ranges. For ex. [0, 3, 4, 4, 4] indicates one copy interval for first curve
   * [0, 3] and two for second [4, 4][4, 4]. The first curve will be copied as is without changes,
   * in the second one (consisting only one point - 4) first point will be duplicated (extruded).
   */
  Array<int> curve_intervals(curve_intervals_size);

  /* Points to intervals for each curve in the curve_intervals array.
   * For example above value would be [{0, 1}, {2, 2}] */
  Array<IndexRange> curve_interval_ranges(curves_num);

  /* Per curve boolean indicating if first interval in a curve is selected.
   * Other can be calculated as in a curve two adjacent intervals can not have same selection
   * state. */
  Array<bool> is_first_selected(curves_num);

  calc_curves_extrusion(extruded_points,
                        old_offsets,
                        new_offsets,
                        curve_intervals,
                        curve_interval_ranges,
                        is_first_selected);

  new_curves.resize(new_offsets.last(), new_curves.curves_num());

  const bke::AttributeAccessor src_attributes = curves.attributes();
  GVArray src_selection_array = *src_attributes.lookup(".selection", bke::AttrDomain::Point);
  if (!src_selection_array) {
    src_selection_array = VArray<bool>::ForSingle(true, curves.points_num());
  }
  const GVArraySpan src_selection = src_selection_array;
  const CPPType &src_selection_type = src_selection.type();
  bke::GSpanAttributeWriter dst_selection = ensure_selection_attribute(
      new_curves,
      bke::AttrDomain::Point,
      src_selection_type.is<bool>() ? CD_PROP_BOOL : CD_PROP_FLOAT);

  threading::parallel_for(curves.curves_range(), 256, [&](IndexRange curves_range) {
    for (const int curve : curves_range) {
      const int first_index = curve_interval_ranges[curve].start();
      const int first_value = curve_intervals[first_index];
      bool is_selected = is_first_selected[curve];

      for (const int i : curve_interval_ranges[curve]) {
        const int dest_index = new_offsets[curve] + curve_intervals[i] - first_value + i -
                               first_index;
        const int size = curve_intervals[i + 1] - curve_intervals[i] + 1;
        GMutableSpan dst_span = dst_selection.span.slice(IndexRange(dest_index, size));
        if (is_selected) {
          src_selection_type.copy_assign_n(
              src_selection.slice(IndexRange(curve_intervals[i], size)).data(),
              dst_span.data(),
              size);
        }
        else {
          fill_selection(dst_span, false);
        }

        is_selected = !is_selected;
      }
    }
  });
  dst_selection.finish();

  const Span<int> intervals = compress_intervals(curve_interval_ranges, curve_intervals);

  bke::MutableAttributeAccessor dst_attributes = new_curves.attributes_for_write();

  for (auto &attribute : bke::retrieve_attributes_for_transfer(
           src_attributes, dst_attributes, ATTR_DOMAIN_MASK_POINT, {}, {".selection"}))
  {
    const CPPType &type = attribute.src.type();
    threading::parallel_for(IndexRange(intervals.size() - 1), 512, [&](IndexRange range) {
      for (const int i : range) {
        const int first = intervals[i];
        const int size = intervals[i + 1] - first + 1;
        const int dest_index = intervals[i] + i;
        type.copy_assign_n(attribute.src.slice(IndexRange(first, size)).data(),
                           attribute.dst.span.slice(IndexRange(dest_index, size)).data(),
                           size);
      }
    });
    attribute.dst.finish();
  }
  curves_id.geometry.wrap() = std::move(new_curves);
  DEG_id_tag_update(&curves_id.id, ID_RECALC_GEOMETRY);
}

static int curves_extrude_exec(bContext *C, wmOperator * /*op*/)
{
  for (Curves *curves_id : get_unique_editable_curves(*C)) {
    extrude_curves(*curves_id);
  }
  return OPERATOR_FINISHED;
}

void CURVES_OT_extrude(wmOperatorType *ot)
{
  ot->name = "Extrude";
  ot->description = "Extrude selected control point(s)";
  ot->idname = "CURVES_OT_extrude";

  ot->exec = curves_extrude_exec;
  ot->poll = editable_curves_in_edit_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

}  // namespace blender::ed::curves
