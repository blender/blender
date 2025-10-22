/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_report.hh"

#include "DNA_scene_types.h"

#include "DEG_depsgraph.hh"

#include "ED_curves.hh"
#include "ED_grease_pencil.hh"
#include "RNA_access.hh"

#include "WM_api.hh"

#include "RNA_define.hh"

#include <algorithm>

namespace blender::ed::greasepencil {

namespace {

/**
 * Struct holding information regarding the set of points identified by \a range.
 * All the points belonging to a `PointsRange` are contiguous
 */
struct PointsRange {
  bke::greasepencil::Drawing *from_drawing;
  IndexRange range;
};

enum class ActionOnNextRange { Nothing, ReverseExisting, ReverseAddition, ReverseBoth };

enum class ActiveLayerBehavior { JoinStrokes, SplitAndCopy, SplitPoints };

/**
 * Iterates over \a drawings and returns a vector with all the selected ranges of points.
 * In the parameter \a r_total_points_selected will be written the number of points selected
 *
 *  A range of points is defined as a group contiguous and visually connected points
 */
Vector<PointsRange> retrieve_selection_ranges(Object &object,
                                              const Span<MutableDrawingInfo> drawings,
                                              const ActiveLayerBehavior active_layer_behavior,
                                              int64_t &r_total_points_selected,
                                              IndexMaskMemory &memory)
{
  Vector<PointsRange> selected_ranges{};
  r_total_points_selected = 0;

  for (const MutableDrawingInfo &info : drawings) {
    if (active_layer_behavior == ActiveLayerBehavior::JoinStrokes) {
      IndexMask curves_selection = retrieve_editable_and_selected_strokes(
          object, info.drawing, info.layer_index, memory);
      if (curves_selection.is_empty()) {
        continue;
      }

      const OffsetIndices<int> points_by_curve = info.drawing.strokes().points_by_curve();
      curves_selection.foreach_index([&](const int curve_i) {
        const IndexRange points = points_by_curve[curve_i];
        selected_ranges.append({&info.drawing, points});
        r_total_points_selected += points.size();
      });

      continue;
    }

    IndexMask points_selection = retrieve_editable_and_selected_points(
        object, info.drawing, info.layer_index, memory);
    if (points_selection.is_empty()) {
      continue;
    }
    r_total_points_selected += points_selection.size();

    const Vector<IndexRange> initial_ranges = points_selection.to_ranges();

    /**
     * Splitting the source selection by ranges doesn't take into account the strokes,
     * i.e, if both the end of an stroke and the beginning of the next are selected, all the
     * indices end up in the same range. Let's refine the splitting
     */
    const Array<int> points_map = info.drawing.strokes().point_to_curve_map();
    for (const IndexRange initial_range : initial_ranges) {
      if (points_map[initial_range.first()] == points_map[initial_range.last()]) {
        selected_ranges.append({&info.drawing, initial_range});
        continue;
      }

      IndexRange range = {initial_range.start(), 1};
      int previous_curve = points_map[range.start()];
      for (const int64_t index : initial_range.drop_front(1)) {
        const int current_curve = points_map[index];
        if (previous_curve != current_curve) {
          selected_ranges.append({&info.drawing, range});
          range = {index, 1};
          previous_curve = current_curve;
        }
        else {
          range = {range.start(), range.size() + 1};
        }
      }

      selected_ranges.append({&info.drawing, range});
    }
  }

  return selected_ranges;
}

template<typename T> void reverse_point_data(const IndexRange point_range, MutableSpan<T> data)
{
  data.slice(point_range.first(), point_range.size()).reverse();
}

template<typename T>
void swap_handle_attributes(MutableSpan<T> handles_left, MutableSpan<T> handles_right)
{
  BLI_assert(handles_left.size() == handles_right.size());
  threading::parallel_for(handles_left.index_range(), 8192, [&](const IndexRange range) {
    for (const int point : range) {
      std::swap(handles_left[point], handles_right[point]);
    }
  });
};

/**
 * Change on \dst_curves the direction of \a points_to_reverse (switch the start and end) without
 * changing their shape.
 */
void reverse_points_of(bke::CurvesGeometry &dst_curves, const IndexRange points_to_reverse)
{
  bke::MutableAttributeAccessor attributes = dst_curves.attributes_for_write();

  attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    if (iter.domain != bke::AttrDomain::Point) {
      return;
    }
    if (iter.data_type == bke::AttrType::String) {
      return;
    }

    bke::GSpanAttributeWriter attribute = attributes.lookup_for_write_span(iter.name);
    bke::attribute_math::convert_to_static_type(attribute.span.type(), [&](auto dummy) {
      using T = decltype(dummy);
      reverse_point_data<T>(points_to_reverse, attribute.span.typed<T>());
    });
    attribute.finish();
  });

  /* Also needs to swap left/right bezier handles if handle attributes exist. */
  if (attributes.contains("handle_left") && attributes.contains("handle_right")) {
    MutableSpan<float3> handles_left = dst_curves.handle_positions_left_for_write().slice(
        points_to_reverse);
    MutableSpan<float3> handles_right = dst_curves.handle_positions_right_for_write().slice(
        points_to_reverse);
    swap_handle_attributes<float3>(handles_left, handles_right);
  }
  if (attributes.contains(".selection_handle_left") &&
      attributes.contains(".selection_handle_right"))
  {
    bke::SpanAttributeWriter<bool> writer_left = attributes.lookup_for_write_span<bool>(
        ".selection_handle_left");
    bke::SpanAttributeWriter<bool> writer_right = attributes.lookup_for_write_span<bool>(
        ".selection_handle_right");
    const MutableSpan<bool> selection_left = writer_left.span.slice(points_to_reverse);
    const MutableSpan<bool> selection_right = writer_right.span.slice(points_to_reverse);
    swap_handle_attributes<bool>(selection_left, selection_right);
    writer_left.finish();
    writer_right.finish();
  }
  if (attributes.contains("handle_type_left") && attributes.contains("handle_type_right")) {
    MutableSpan<int8_t> types_left = dst_curves.handle_types_left_for_write().slice(
        points_to_reverse);
    MutableSpan<int8_t> types_right = dst_curves.handle_types_right_for_write().slice(
        points_to_reverse);
    swap_handle_attributes<int8_t>(types_left, types_right);
  }
}

void apply_action(ActionOnNextRange action,
                  const IndexRange working_range,
                  const IndexRange adding_range,
                  bke::CurvesGeometry &dst_curves)
{
  /** In order to connect the selections through the closest distance between them, the points have
   * to be arranged properly
   * For instance, if the working range is [0, 3) and the adding range is [13, 15),
   * and assuming this is the spatial positioning of these points:
   * 0 - 1 - 2      14 - 13
   * The computed closest distance is between points 2 and 14. But then the range [13, 15) has to
   * be reversed in dst_curves in order to create the desired stroke:
   * 0 - 1 - 2 ---- 13 - 14
   * The actual range to revert is determined by the action parameter, computed alongside with
   * adding_range
   */

  switch (action) {
    case ActionOnNextRange::Nothing:
      return;
    case ActionOnNextRange::ReverseExisting: {
      reverse_points_of(dst_curves, working_range);
      break;
    }
    case ActionOnNextRange::ReverseAddition: {
      const IndexRange src_range_on_dst = {working_range.last() + 1, adding_range.size()};
      reverse_points_of(dst_curves, src_range_on_dst);
      break;
    }
    case ActionOnNextRange::ReverseBoth: {
      apply_action(ActionOnNextRange::ReverseExisting, working_range, adding_range, dst_curves);
      apply_action(ActionOnNextRange::ReverseAddition, working_range, adding_range, dst_curves);
      break;
    }
  }
}

/**
 * Given \a range, computes which one of \a ranges is closer to it.
 * Does not evaluate the whole span, but just from the position \a starting_from onward.
 *
 * Returns the index of the closest range. The parameter \a r_action will hold the action to take
 * on this range
 */
int64_t compute_closest_range_to(PointsRange &range,
                                 const Span<PointsRange> &ranges,
                                 int64_t starting_from,
                                 ActionOnNextRange &r_action)
{
  auto get_range_begin_end = [](const PointsRange &points_range) -> std::pair<float3, float3> {
    const Span<float3> current_range_positions = points_range.from_drawing->strokes().positions();
    const float3 range_begin = current_range_positions[points_range.range.first()];
    const float3 range_end = current_range_positions[points_range.range.last()];

    return {range_begin, range_end};
  };

  const auto [cur_range_begin, cur_range_end] = get_range_begin_end(range);
  float min_dist = FLT_MAX;

  int64_t ret_value = starting_from;
  ActionOnNextRange action = ActionOnNextRange::Nothing;

  const int64_t iterations = ranges.size() - starting_from;
  for (const int64_t i : IndexRange(starting_from, iterations)) {
    const auto [range_begin, range_end] = get_range_begin_end(ranges[i]);

    float dist = math::distance_squared(cur_range_end, range_begin);
    if (dist < min_dist) {
      action = ActionOnNextRange::Nothing;
      ret_value = i;
      min_dist = dist;
    }

    dist = math::distance_squared(cur_range_begin, range_begin);
    if (dist < min_dist) {
      action = ActionOnNextRange::ReverseExisting;
      ret_value = i;
      min_dist = dist;
    }

    dist = math::distance_squared(cur_range_end, range_end);
    if (dist < min_dist) {
      action = ActionOnNextRange::ReverseAddition;
      ret_value = i;
      min_dist = dist;
    }

    dist = math::distance_squared(cur_range_begin, range_end);
    if (dist < min_dist) {
      action = ActionOnNextRange::ReverseBoth;
      ret_value = i;
      min_dist = dist;
    }
  }

  r_action = action;
  return ret_value;
}

void copy_range_to_dst(const PointsRange &points_range,
                       int &dst_starting_point,
                       bke::CurvesGeometry &dst_curves)
{
  Array<int> src_raw_offsets(2);
  Array<int> dst_raw_offsets(2);

  const int64_t selection_size = points_range.range.size();
  src_raw_offsets[0] = points_range.range.first();
  src_raw_offsets[1] = points_range.range.last() + 1;

  dst_raw_offsets[0] = dst_starting_point;
  dst_starting_point += selection_size;
  dst_raw_offsets[1] = dst_starting_point;

  OffsetIndices<int> src_offsets{src_raw_offsets};
  OffsetIndices<int> dst_offsets{dst_raw_offsets};

  copy_attributes_group_to_group(points_range.from_drawing->strokes().attributes(),
                                 bke::AttrDomain::Point,
                                 {},
                                 {},
                                 src_offsets,
                                 dst_offsets,
                                 IndexMask{1},
                                 dst_curves.attributes_for_write());
}

PointsRange copy_point_attributes(MutableSpan<PointsRange> selected_ranges,
                                  bke::CurvesGeometry &dst_curves,
                                  bke::greasepencil::Drawing &dst_drawing)
{
  /* The algorithm for joining the points goes as follows:
   * 1. Pick the first range of the selected ranges of points, which will be the working range
   * 2. Copy the attributes of this range to dst_curves
   * 3. Lookup in the remaining ranges for the one closer to the working range
   * 4. Copy its attributes
   * 5. In order to minimize the length of the stroke connecting them, reverse their points as
   * needed
   * 6. Extend the working range with the new range
   * 7. Remove the new range from the list of remaining ranges. Lookup for the next one and
   * continue
   */

  const PointsRange &first_range = selected_ranges.first();
  PointsRange working_range = {&dst_drawing, {0, first_range.range.size()}};

  int next_point_index = 0;
  copy_range_to_dst(first_range, next_point_index, dst_curves);

  const int64_t ranges = selected_ranges.size() - 1;
  for (const int64_t i : IndexRange(1, ranges)) {
    ActionOnNextRange action;
    const int64_t closest_range = compute_closest_range_to(
        working_range, selected_ranges, i, action);
    std::swap(selected_ranges[i], selected_ranges[closest_range]);
    PointsRange &next_range = selected_ranges[i];
    copy_range_to_dst(next_range, next_point_index, dst_curves);
    apply_action(action, working_range.range, next_range.range, dst_curves);
    working_range.range = {0, next_point_index};
  }

  return working_range;
}

void copy_curve_attributes(Span<PointsRange> ranges_selected,
                           bke::CurvesGeometry &dst_curves,
                           bke::greasepencil::Drawing &dst_drawing)
{
  /* The decision of which stroke use to copy the curve attributes is a bit arbitrary, since the
   * original selection may embrace several strokes. The criteria is as follows:
   *  - If the selection contained points from the active layer, the first selected stroke from it
   * is used.
   *  - Otherwise, the first selected stroke is used.
   * Reasoning behind is that the user will probably want to keep similar curve parameters for
   * all the strokes in a layer.
   * Also, the "cyclic" attribute is deliberately set to false, since user
   * probably wants to set it manually
   */

  auto src_range = [&]() -> const PointsRange & {
    const auto *it = std::find_if(
        ranges_selected.begin(), ranges_selected.end(), [dst_drawing](const PointsRange &range) {
          return range.from_drawing == &dst_drawing;
        });

    return it != ranges_selected.end() ? *it : ranges_selected.first();
  }();

  const bke::CurvesGeometry &src_curves = src_range.from_drawing->strokes();
  const Array<int> points_map = src_curves.point_to_curve_map();
  const int first_selected_curve = points_map[src_range.range.first()];

  const int final_curve_index = dst_curves.curves_num() - 1;
  const Array<int> dst_curves_raw_offsets = {final_curve_index, dst_curves.curves_num()};
  const OffsetIndices<int> dst_curve_offsets{dst_curves_raw_offsets};

  gather_attributes_to_groups(src_curves.attributes(),
                              bke::AttrDomain::Curve,
                              bke::AttrDomain::Curve,
                              bke::attribute_filter_from_skip_ref({"cyclic"}),
                              dst_curve_offsets,
                              IndexMask({first_selected_curve, 1}),
                              dst_curves.attributes_for_write());
  dst_curves.cyclic_for_write().first() = false;
}

/**
 * Removes the selection state of all the affected CurvesGeometry, except the one
 * of the active layer. Points in the active layer do not get unselected
 */
void clear_selection_attribute(Span<PointsRange> ranges_selected,
                               const bke::AttrDomain selection_domain)
{
  for (const PointsRange &range : ranges_selected) {
    bke::CurvesGeometry &curves = range.from_drawing->strokes_for_write();
    bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
    if (bke::SpanAttributeWriter<bool> selection = attributes.lookup_or_add_for_write_span<bool>(
            ".selection", selection_domain))
    {
      selection.span.fill(false);
      selection.finish();
    }
    if (bke::GSpanAttributeWriter selection = attributes.lookup_for_write_span(".selection_left"))
    {
      ed::curves::fill_selection_false(selection.span);
      selection.finish();
    }
    if (bke::GSpanAttributeWriter selection = attributes.lookup_for_write_span(".selection_right"))
    {
      ed::curves::fill_selection_false(selection.span);
      selection.finish();
    }
  }
}

void remove_selected_points(Span<PointsRange> ranges_selected)
{
  /* Removing points from a drawing invalidates subsequent ranges for the same drawing.
   * Combine all ranges for the same drawings first to prevent removing the wrong points. */
  using RangesMap = Map<bke::greasepencil::Drawing *, Vector<IndexMask>>;
  RangesMap ranges_by_drawing;
  for (const PointsRange &points_range : ranges_selected) {
    BLI_assert(points_range.from_drawing != nullptr);
    Vector<IndexMask> &ranges = ranges_by_drawing.lookup_or_add(points_range.from_drawing, {});
    ranges.append(points_range.range);
  }

  for (const RangesMap::Item &item : ranges_by_drawing.items()) {
    bke::CurvesGeometry &dst_curves = item.key->strokes_for_write();
    IndexMaskMemory memory;
    const IndexMask combined_mask = IndexMask::from_union(item.value, memory);
    dst_curves.remove_points(combined_mask, {});
    item.key->tag_topology_changed();
  }
}

void append_strokes_from(bke::CurvesGeometry &&other, bke::CurvesGeometry &dst)
{
  const int initial_points_num = dst.points_num();
  const int initial_curves_num = dst.curves_num();
  const int other_points_num = other.points_num();
  const int other_curves_num = other.curves_num();

  dst.resize(initial_points_num + other_points_num, initial_curves_num + other_curves_num);

  Array<int> other_raw_offsets{0, other_points_num};
  Array<int> dst_raw_offsets{initial_points_num, initial_points_num + other_points_num};

  OffsetIndices<int> other_point_offsets{other_raw_offsets};
  OffsetIndices<int> dst_point_offsets{dst_raw_offsets};

  copy_attributes_group_to_group(other.attributes(),
                                 bke::AttrDomain::Point,
                                 bke::AttrDomain::Point,
                                 {},
                                 other_point_offsets,
                                 dst_point_offsets,
                                 IndexMask{1},
                                 dst.attributes_for_write());

  other_raw_offsets = {0, other_curves_num};
  dst_raw_offsets = {initial_curves_num, initial_curves_num + other_curves_num};

  OffsetIndices<int> other_curve_offsets{other_raw_offsets};
  OffsetIndices<int> dst_curve_offsets{dst_raw_offsets};

  copy_attributes_group_to_group(other.attributes(),
                                 bke::AttrDomain::Curve,
                                 bke::AttrDomain::Curve,
                                 {},
                                 other_curve_offsets,
                                 dst_curve_offsets,
                                 IndexMask{1},
                                 dst.attributes_for_write());
}

/* -------------------------------------------------------------------- */
/** \name Join Selection Operator
 * \{ */

/**
 * This operator builds a new stroke from the points/curves selected. It makes a copy of all the
 * selected points and joins them in a single stroke, which is added to the active layer.
 */
wmOperatorStatus grease_pencil_join_selection_exec(bContext *C, wmOperator *op)
{
  using namespace bke::greasepencil;

  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  const bke::AttrDomain selection_domain = ED_grease_pencil_selection_domain_get(
      scene->toolsettings, object);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  if (!grease_pencil.has_active_layer()) {
    BKE_report(op->reports, RPT_ERROR, "No active layer");
    return OPERATOR_CANCELLED;
  }

  const ActiveLayerBehavior active_layer_behavior = static_cast<ActiveLayerBehavior>(
      RNA_enum_get(op->ptr, "type"));
  const Layer &active_layer = *grease_pencil.get_active_layer();

  Drawing *dst_drawing = grease_pencil.get_editable_drawing_at(active_layer, scene->r.cfra);
  if (dst_drawing == nullptr) {
    return OPERATOR_CANCELLED;
  }

  IndexMaskMemory memory;
  int64_t selected_points_count;
  const Vector<MutableDrawingInfo> editable_drawings = retrieve_editable_drawings(*scene,
                                                                                  grease_pencil);
  Vector<PointsRange> ranges_selected = retrieve_selection_ranges(
      *object, editable_drawings, active_layer_behavior, selected_points_count, memory);
  if (ranges_selected.size() <= 1) {
    /* Nothing to join */
    return OPERATOR_FINISHED;
  }

  /* Temporary geometry where to perform the logic
   * Once it gets stable, it is appended all at once to the destination curves */
  Drawing tmp_drawing;
  tmp_drawing.strokes_for_write() = bke::CurvesGeometry(selected_points_count, 1);
  bke::CurvesGeometry &tmp_curves = tmp_drawing.strokes_for_write();

  const PointsRange working_range = copy_point_attributes(
      ranges_selected, tmp_curves, tmp_drawing);
  copy_curve_attributes(ranges_selected, tmp_curves, *dst_drawing);

  clear_selection_attribute(ranges_selected, selection_domain);

  Array<PointsRange> working_range_collection = {working_range};
  clear_selection_attribute(working_range_collection, selection_domain);

  bke::CurvesGeometry &dst_curves = dst_drawing->strokes_for_write();
  if (ELEM(active_layer_behavior,
           ActiveLayerBehavior::SplitPoints,
           ActiveLayerBehavior::JoinStrokes))
  {
    remove_selected_points(ranges_selected);
  }

  append_strokes_from(std::move(tmp_curves), dst_curves);

  if (active_layer_behavior != ActiveLayerBehavior::JoinStrokes) {
    bke::GSpanAttributeWriter selection = ed::curves::ensure_selection_attribute(
        dst_curves, selection_domain, bke::AttrType::Bool);

    if (selection_domain == bke::AttrDomain::Curve) {
      ed::curves::fill_selection_true(selection.span.take_back(tmp_curves.curves_num()));
    }
    else {
      ed::curves::fill_selection_true(selection.span.take_back(tmp_curves.points_num()));
    }
    selection.finish();
  }

  dst_curves.update_curve_types();
  dst_curves.tag_topology_changed();
  dst_drawing->tag_topology_changed();

  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);

  return OPERATOR_FINISHED;
}

void GREASE_PENCIL_OT_join_selection(wmOperatorType *ot)
{
  static const EnumPropertyItem active_layer_behavior[] = {
      {int(ActiveLayerBehavior::JoinStrokes),
       "JOINSTROKES",
       0,
       "Join Strokes",
       "Join the selected strokes into one stroke"},
      {int(ActiveLayerBehavior::SplitAndCopy),
       "SPLITCOPY",
       0,
       "Split and Copy",
       "Copy the selected points to a new stroke"},
      {int(ActiveLayerBehavior::SplitPoints),
       "SPLIT",
       0,
       "Split",
       "Split the selected point to a new stroke"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers. */
  ot->name = "Join Selection";
  ot->idname = "GREASE_PENCIL_OT_join_selection";
  ot->description = "New stroke from selected points/strokes";

  /* callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = grease_pencil_join_selection_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(
      ot->srna,
      "type",
      active_layer_behavior,
      int(ActiveLayerBehavior::JoinStrokes),
      "Type",
      "Defines how the operator will behave on the selection in the active layer");
}

}  // namespace

/** \} */

}  // namespace blender::ed::greasepencil

void ED_operatortypes_grease_pencil_join()
{
  using namespace blender::ed::greasepencil;

  WM_operatortype_append(GREASE_PENCIL_OT_join_selection);
}
