/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"

#include "BLI_array_utils.hh"
#include "BLI_index_mask.hh"
#include "BLI_math_angle_types.hh"
#include "BLI_math_geom.h"
#include "BLI_math_rotation.h"
#include "BLI_math_rotation.hh"
#include "BLI_math_vector.hh"
#include "BLI_offset_indices.hh"
#include "BLI_task.hh"
#include "BLT_translation.hh"

#include "DEG_depsgraph.hh"

#include "DNA_grease_pencil_types.h"

#include "ED_grease_pencil.hh"
#include "ED_numinput.hh"
#include "ED_screen.hh"

#include "GEO_interpolate_curves.hh"
#include "GEO_smooth_curves.hh"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include <climits>

namespace blender::ed::sculpt_paint::greasepencil {

using ed::greasepencil::InterpolateFlipMode;
using ed::greasepencil::InterpolateLayerMode;

/* -------------------------------------------------------------------- */
/** \name Interpolate Operator
 * \{ */

constexpr float interpolate_factor_min = -1.0f;
constexpr float interpolate_factor_max = 2.0f;

/* Pair of curves in a layer that get interpolated. */
struct InterpolationPairs {
  Vector<int> from_frames;
  Vector<int> to_frames;
  Vector<int> from_curves;
  Vector<int> to_curves;
};

struct InterpolateOpData {
  struct LayerData {
    /* Curve pairs to interpolate from this layer. */
    InterpolationPairs curve_pairs;

    /* Geometry of the target frame before interpolation for restoring on cancel. */
    std::optional<bke::CurvesGeometry> orig_curves;
  };

  /* Layers to include. */
  IndexMaskMemory layer_mask_memory;
  IndexMask layer_mask;
  /* Exclude breakdown keyframes when finding intervals. */
  bool exclude_breakdowns;

  /* Interpolation factor bias controlled by the user. */
  float shift;
  /* Interpolation base factor for the active layer. */
  float init_factor;
  InterpolateFlipMode flipmode;
  float smooth_factor;
  int smooth_steps;

  NumInput numeric_input;
  Array<LayerData> layer_data;
  int active_layer_index;
};

using FramesMapKeyIntervalT = std::pair<int, int>;

static std::optional<FramesMapKeyIntervalT> find_frames_interval(
    const bke::greasepencil::Layer &layer, const int frame_number, const bool exclude_breakdowns)
{
  using Layer = bke::greasepencil::Layer;
  using bke::greasepencil::FramesMapKeyT;
  using SortedKeysIterator = Layer::SortedKeysIterator;

  const Span<FramesMapKeyT> sorted_keys = layer.sorted_keys();
  SortedKeysIterator prev_key_it = layer.sorted_keys_iterator_at(frame_number);
  if (!prev_key_it) {
    return std::nullopt;
  }
  SortedKeysIterator next_key_it = std::next(prev_key_it);

  /* Skip over invalid keyframes on either side. */
  auto is_valid_keyframe = [&](const FramesMapKeyT key) {
    const GreasePencilFrame *frame = layer.frame_at(key);
    if (!frame || frame->is_end()) {
      return false;
    }
    if (exclude_breakdowns && frame->type == BEZT_KEYTYPE_BREAKDOWN) {
      return false;
    }
    return true;
  };

  for (; next_key_it != sorted_keys.end(); ++next_key_it) {
    if (is_valid_keyframe(*next_key_it)) {
      break;
    }
  }
  for (; prev_key_it != sorted_keys.begin(); --prev_key_it) {
    if (is_valid_keyframe(*prev_key_it)) {
      break;
    }
  }
  if (next_key_it == sorted_keys.end() || !is_valid_keyframe(*prev_key_it)) {
    return std::nullopt;
  }

  return std::make_pair(*prev_key_it, *next_key_it);
}

/* Build index lists for curve interpolation using index. */
static void find_curve_mapping_from_index(const GreasePencil &grease_pencil,
                                          const bke::greasepencil::Layer &layer,
                                          const int current_frame,
                                          const bool exclude_breakdowns,
                                          InterpolationPairs &pairs)
{
  using bke::greasepencil::Drawing;

  const std::optional<FramesMapKeyIntervalT> interval = find_frames_interval(
      layer, current_frame, exclude_breakdowns);
  if (!interval) {
    return;
  }

  BLI_assert(layer.has_drawing_at(interval->first));
  BLI_assert(layer.has_drawing_at(interval->second));
  const Drawing &from_drawing = *grease_pencil.get_drawing_at(layer, interval->first);
  const Drawing &to_drawing = *grease_pencil.get_drawing_at(layer, interval->second);

  const int pairs_num = std::min(from_drawing.strokes().curves_num(),
                                 to_drawing.strokes().curves_num());

  const int old_pairs_num = pairs.from_frames.size();
  pairs.from_frames.append_n_times(interval->first, pairs_num);
  pairs.to_frames.append_n_times(interval->second, pairs_num);
  pairs.from_curves.resize(old_pairs_num + pairs_num);
  pairs.to_curves.resize(old_pairs_num + pairs_num);
  array_utils::fill_index_range(
      pairs.from_curves.as_mutable_span().slice(old_pairs_num, pairs_num));
  array_utils::fill_index_range(pairs.to_curves.as_mutable_span().slice(old_pairs_num, pairs_num));
}

static bool compute_auto_flip(const Span<float3> from_positions, const Span<float3> to_positions)
{
  if (from_positions.size() < 2 || to_positions.size() < 2) {
    return false;
  }

  constexpr float min_angle = DEG2RADF(15);

  const float3 &from_first = from_positions.first();
  const float3 &from_last = from_positions.last();
  const float3 &to_first = to_positions.first();
  const float3 &to_last = to_positions.last();

  /* If lines intersect at a sharp angle check distances. */
  if (isect_seg_seg_v2(from_first, to_first, from_last, to_last) == ISECT_LINE_LINE_CROSS) {
    if (math::angle_between(math::normalize(to_first - from_first),
                            math::normalize(to_last - from_last))
            .radian() < min_angle)
    {
      if (math::distance_squared(from_first, to_first) >=
          math::distance_squared(from_last, to_first))
      {
        return math::distance_squared(from_last, to_first) >=
               math::distance_squared(from_last, to_last);
      }

      return math::distance_squared(from_first, to_first) <
             math::distance_squared(from_first, to_last);
    }

    return true;
  }

  return math::dot(from_last - from_first, to_last - to_first) < 0.0f;
}

static bke::CurvesGeometry interpolate_between_curves(const GreasePencil &grease_pencil,
                                                      const bke::greasepencil::Layer &layer,
                                                      const InterpolationPairs &curve_pairs,
                                                      const float mix_factor,
                                                      const InterpolateFlipMode flip_mode)
{
  using bke::greasepencil::Drawing;

  const int dst_curve_num = curve_pairs.from_curves.size();
  BLI_assert(curve_pairs.to_curves.size() == dst_curve_num);
  BLI_assert(curve_pairs.from_frames.size() == dst_curve_num);
  BLI_assert(curve_pairs.to_frames.size() == dst_curve_num);

  /* Sort pairs by unique to/from frame combinations.
   * Curves for each frame pair are then interpolated together.
   * Map entries are indices into the original curve_pairs array,
   * so the order of strokes can be maintained. */
  Array<int> sorted_pairs(dst_curve_num);
  array_utils::fill_index_range(sorted_pairs.as_mutable_span());
  std::sort(sorted_pairs.begin(), sorted_pairs.end(), [&](const int a, const int b) {
    const int from_frame_a = curve_pairs.from_frames[a];
    const int to_frame_a = curve_pairs.to_frames[a];
    const int from_frame_b = curve_pairs.from_frames[b];
    const int to_frame_b = curve_pairs.to_frames[b];
    return from_frame_a < from_frame_b ||
           (from_frame_a == from_frame_b && to_frame_a < to_frame_b);
  });

  /* Find ranges of sorted pairs with the same from/to frame intervals. */
  Vector<int> pair_offsets;
  const OffsetIndices curves_by_pair = [&]() {
    int prev_from_frame = INT_MIN;
    int prev_to_frame = INT_MIN;
    int current_count = 0;
    for (const int sorted_index : IndexRange(dst_curve_num)) {
      const int pair_index = sorted_pairs[sorted_index];
      const int from_frame = curve_pairs.from_frames[pair_index];
      const int to_frame = curve_pairs.to_frames[pair_index];
      if (from_frame != prev_from_frame || to_frame != prev_to_frame) {
        /* New pair. */
        if (current_count > 0) {
          pair_offsets.append(current_count);
        }
        current_count = 0;
      }
      ++current_count;
    }
    if (current_count > 0) {
      pair_offsets.append(current_count);
    }

    /* Last entry for overall size. */
    if (pair_offsets.is_empty()) {
      return OffsetIndices<int>{};
    }

    pair_offsets.append(0);
    return offset_indices::accumulate_counts_to_offsets(pair_offsets);
  }();

  /* Compute curve length and flip mode for each pair. */
  Vector<int> dst_curve_offsets;
  Vector<bool> dst_curve_flip;
  const OffsetIndices dst_points_by_curve = [&]() {
    for (const int pair_range_i : curves_by_pair.index_range()) {
      const IndexRange pair_range = curves_by_pair[pair_range_i];
      BLI_assert(!pair_range.is_empty());

      const int first_pair_index = sorted_pairs[pair_range.first()];
      const int from_frame = curve_pairs.from_frames[first_pair_index];
      const int to_frame = curve_pairs.to_frames[first_pair_index];
      const Drawing *from_drawing = grease_pencil.get_drawing_at(layer, from_frame);
      const Drawing *to_drawing = grease_pencil.get_drawing_at(layer, to_frame);
      if (!from_drawing || !to_drawing) {
        continue;
      }
      const OffsetIndices from_points_by_curve = from_drawing->strokes().points_by_curve();
      const OffsetIndices to_points_by_curve = to_drawing->strokes().points_by_curve();
      const Span<float3> from_positions = from_drawing->strokes().positions();
      const Span<float3> to_positions = to_drawing->strokes().positions();

      for (const int sorted_index : pair_range) {
        const int pair_index = sorted_pairs[sorted_index];
        const int from_curve = curve_pairs.from_curves[pair_index];
        const int to_curve = curve_pairs.to_curves[pair_index];
        const IndexRange from_points = from_points_by_curve[from_curve];
        const IndexRange to_points = to_points_by_curve[to_curve];

        dst_curve_offsets.append(std::max(from_points.size(), to_points.size()));
        switch (flip_mode) {
          case InterpolateFlipMode::None:
            dst_curve_flip.append(false);
            break;
          case InterpolateFlipMode::Flip:
            dst_curve_flip.append(true);
            break;
          case InterpolateFlipMode::FlipAuto: {
            dst_curve_flip.append(compute_auto_flip(from_positions.slice(from_points),
                                                    to_positions.slice(to_points)));
            break;
          }
        }
      }
    }
    /* Last entry for overall size. */
    if (dst_curve_offsets.is_empty()) {
      return OffsetIndices<int>{};
    }

    dst_curve_offsets.append(0);
    return offset_indices::accumulate_counts_to_offsets(dst_curve_offsets);
  }();
  const int dst_point_num = dst_points_by_curve.total_size();

  bke::CurvesGeometry dst_curves(dst_point_num, dst_curve_num);
  /* Offsets are empty when there are no curves. */
  if (dst_curve_num > 0) {
    dst_curves.offsets_for_write().copy_from(dst_curve_offsets);
  }

  /* Sorted map arrays that can be passed to the interpolation function directly.
   * These index maps have the same order as the sorted indices, so slices of indices can be used
   * for interpolating all curves of a frame pair at once. */
  Array<int> sorted_from_curve_indices(dst_curve_num);
  Array<int> sorted_to_curve_indices(dst_curve_num);

  for (const int pair_range_i : curves_by_pair.index_range()) {
    const IndexRange pair_range = curves_by_pair[pair_range_i];
    const int first_pair_index = sorted_pairs[pair_range.first()];
    const int from_frame = curve_pairs.from_frames[first_pair_index];
    const int to_frame = curve_pairs.to_frames[first_pair_index];
    const Drawing *from_drawing = grease_pencil.get_drawing_at(layer, from_frame);
    const Drawing *to_drawing = grease_pencil.get_drawing_at(layer, to_frame);
    if (!from_drawing || !to_drawing) {
      continue;
    }
    const IndexRange from_curves = from_drawing->strokes().curves_range();
    const IndexRange to_curves = to_drawing->strokes().curves_range();

    /* Subset of target curves that are filled by this frame pair. */
    IndexMaskMemory selection_memory;
    const IndexMask selection = IndexMask::from_indices(sorted_pairs.as_span().slice(pair_range),
                                                        selection_memory);
    MutableSpan<int> pair_from_indices = sorted_from_curve_indices.as_mutable_span().slice(
        pair_range);
    MutableSpan<int> pair_to_indices = sorted_to_curve_indices.as_mutable_span().slice(pair_range);
    for (const int i : pair_range) {
      const int pair_index = sorted_pairs[i];
      sorted_from_curve_indices[i] = std::clamp(
          curve_pairs.from_curves[pair_index], 0, int(from_curves.last()));
      sorted_to_curve_indices[i] = std::clamp(
          curve_pairs.to_curves[pair_index], 0, int(to_curves.last()));
    }
    geometry::interpolate_curves(from_drawing->strokes(),
                                 to_drawing->strokes(),
                                 pair_from_indices,
                                 pair_to_indices,
                                 selection,
                                 dst_curve_flip,
                                 mix_factor,
                                 dst_curves);
  }

  return dst_curves;
}

static void grease_pencil_interpolate_status_indicators(bContext &C,
                                                        const InterpolateOpData &opdata)
{
  Scene &scene = *CTX_data_scene(&C);
  ScrArea &area = *CTX_wm_area(&C);

  const StringRef msg = IFACE_("GPencil Interpolation: ");

  std::string status;
  if (hasNumInput(&opdata.numeric_input)) {
    char str_ofs[NUM_STR_REP_LEN];
    outputNumInput(&const_cast<NumInput &>(opdata.numeric_input), str_ofs, &scene.unit);
    status = msg + std::string(str_ofs);
  }
  else {
    status = msg + std::to_string(int((opdata.init_factor + opdata.shift) * 100.0f)) + " %";
  }

  ED_area_status_text(&area, status.c_str());
  ED_workspace_status_text(
      &C, IFACE_("ESC/RMB to cancel, Enter/LMB to confirm, WHEEL/MOVE to adjust factor"));
}

/* Utility function to get a drawing at the exact frame number. */
static bke::greasepencil::Drawing *get_drawing_at_exact_frame(GreasePencil &grease_pencil,
                                                              bke::greasepencil::Layer &layer,
                                                              const int frame_number)
{
  using bke::greasepencil::Drawing;

  const std::optional<int> start_frame = layer.start_frame_at(frame_number);
  if (start_frame && *start_frame == frame_number) {
    return grease_pencil.get_editable_drawing_at(layer, frame_number);
  }
  return nullptr;
}

static bke::greasepencil::Drawing *ensure_drawing_at_exact_frame(
    GreasePencil &grease_pencil,
    bke::greasepencil::Layer &layer,
    InterpolateOpData::LayerData &layer_data,
    const int frame_number)
{
  using bke::greasepencil::Drawing;

  if (Drawing *drawing = get_drawing_at_exact_frame(grease_pencil, layer, frame_number)) {
    layer_data.orig_curves = drawing->strokes();
    return drawing;
  }
  return grease_pencil.insert_frame(layer, frame_number);
}

static void grease_pencil_interpolate_update(bContext &C, const wmOperator &op)
{
  using bke::greasepencil::Drawing;
  using bke::greasepencil::Layer;

  const auto &opdata = *static_cast<InterpolateOpData *>(op.customdata);
  const Scene &scene = *CTX_data_scene(&C);
  const int current_frame = scene.r.cfra;
  Object &object = *CTX_data_active_object(&C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);
  const auto flip_mode = InterpolateFlipMode(RNA_enum_get(op.ptr, "flip"));

  opdata.layer_mask.foreach_index([&](const int layer_index) {
    Layer &layer = *grease_pencil.layer(layer_index);
    const InterpolateOpData::LayerData &layer_data = opdata.layer_data[layer_index];

    /* Drawings must be created on operator invoke. */
    Drawing *dst_drawing = get_drawing_at_exact_frame(grease_pencil, layer, current_frame);
    if (dst_drawing == nullptr) {
      return;
    }

    const float mix_factor = opdata.init_factor + opdata.shift;
    bke::CurvesGeometry interpolated_curves = interpolate_between_curves(
        grease_pencil, layer, layer_data.curve_pairs, mix_factor, flip_mode);

    if (opdata.smooth_factor > 0.0f && opdata.smooth_steps > 0) {
      MutableSpan<float3> positions = interpolated_curves.positions_for_write();
      geometry::smooth_curve_attribute(
          interpolated_curves.curves_range(),
          interpolated_curves.points_by_curve(),
          VArray<bool>::ForSingle(true, interpolated_curves.points_num()),
          interpolated_curves.cyclic(),
          opdata.smooth_steps,
          opdata.smooth_factor,
          false,
          false,
          positions);
      interpolated_curves.tag_positions_changed();
    }

    dst_drawing->strokes_for_write() = std::move(interpolated_curves);
    dst_drawing->tag_topology_changed();
  });

  grease_pencil_interpolate_status_indicators(C, opdata);

  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(&C, NC_GPENCIL | NA_EDITED, nullptr);
}

/* Restore timeline changes when canceled. */
static void grease_pencil_interpolate_restore(bContext &C, wmOperator &op)
{
  using bke::greasepencil::Drawing;
  using bke::greasepencil::Layer;

  if (op.customdata == nullptr) {
    return;
  }

  const auto &opdata = *static_cast<InterpolateOpData *>(op.customdata);
  const Scene &scene = *CTX_data_scene(&C);
  const int current_frame = scene.r.cfra;
  Object &object = *CTX_data_active_object(&C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);

  opdata.layer_mask.foreach_index([&](const int layer_index) {
    Layer &layer = *grease_pencil.layer(layer_index);
    const InterpolateOpData::LayerData &layer_data = opdata.layer_data[layer_index];

    if (layer_data.orig_curves) {
      /* Keyframe existed before the operator, restore geometry. */
      Drawing *drawing = grease_pencil.get_editable_drawing_at(layer, current_frame);
      if (drawing) {
        drawing->strokes_for_write() = *layer_data.orig_curves;
        drawing->tag_topology_changed();
        DEG_id_tag_update(&grease_pencil.id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
        WM_event_add_notifier(&C, NC_GPENCIL | NA_EDITED, nullptr);
      }
    }
    else {
      /* Frame was empty, remove the added drawing. */
      grease_pencil.remove_frames(layer, {current_frame});
      DEG_id_tag_update(&grease_pencil.id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
      WM_event_add_notifier(&C, NC_GPENCIL | NA_EDITED, nullptr);
    }
  });
}

static bool grease_pencil_interpolate_init(const bContext &C, wmOperator &op)
{
  using bke::greasepencil::Drawing;
  using bke::greasepencil::Layer;

  const Scene &scene = *CTX_data_scene(&C);
  const int current_frame = scene.r.cfra;
  Object &object = *CTX_data_active_object(&C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);

  BLI_assert(grease_pencil.has_active_layer());
  const Layer &active_layer = *grease_pencil.get_active_layer();

  op.customdata = MEM_new<InterpolateOpData>(__func__);
  InterpolateOpData &data = *static_cast<InterpolateOpData *>(op.customdata);

  data.shift = RNA_float_get(op.ptr, "shift");
  data.exclude_breakdowns = RNA_boolean_get(op.ptr, "exclude_breakdowns");
  data.flipmode = InterpolateFlipMode(RNA_enum_get(op.ptr, "flip"));
  data.smooth_factor = RNA_float_get(op.ptr, "smooth_factor");
  data.smooth_steps = RNA_int_get(op.ptr, "smooth_steps");
  data.active_layer_index = *grease_pencil.get_layer_index(active_layer);

  const auto layer_mode = InterpolateLayerMode(RNA_enum_get(op.ptr, "layers"));
  switch (layer_mode) {
    case InterpolateLayerMode::Active:
      data.layer_mask = IndexRange::from_single(data.active_layer_index);
      break;
    case InterpolateLayerMode::All:
      data.layer_mask = IndexMask::from_predicate(
          grease_pencil.layers().index_range(),
          GrainSize(1024),
          data.layer_mask_memory,
          [&](const int layer_index) { return grease_pencil.layer(layer_index)->is_editable(); });
      break;
  }

  data.layer_data.reinitialize(grease_pencil.layers().size());
  data.layer_mask.foreach_index([&](const int layer_index) {
    Layer &layer = *grease_pencil.layer(layer_index);
    InterpolateOpData::LayerData &layer_data = data.layer_data[layer_index];

    /* Pair from/to curves by index. */
    find_curve_mapping_from_index(
        grease_pencil, layer, current_frame, data.exclude_breakdowns, layer_data.curve_pairs);

    ensure_drawing_at_exact_frame(grease_pencil, layer, layer_data, current_frame);
  });

  const std::optional<FramesMapKeyIntervalT> active_layer_interval = find_frames_interval(
      active_layer, current_frame, data.exclude_breakdowns);
  data.init_factor = active_layer_interval ?
                         float(current_frame - active_layer_interval->first) /
                             (active_layer_interval->second - active_layer_interval->first + 1) :
                         0.5f;

  return true;
}

/* Exit and free memory. */
static void grease_pencil_interpolate_exit(bContext &C, wmOperator &op)
{
  ScrArea &area = *CTX_wm_area(&C);

  if (op.customdata == nullptr) {
    return;
  }

  ED_area_status_text(&area, nullptr);
  ED_workspace_status_text(&C, nullptr);

  MEM_delete(static_cast<InterpolateOpData *>(op.customdata));
  op.customdata = nullptr;
}

static bool grease_pencil_interpolate_poll(bContext *C)
{
  if (!ed::greasepencil::active_grease_pencil_poll(C)) {
    return false;
  }
  ToolSettings *ts = CTX_data_tool_settings(C);
  if (!ts || !ts->gp_paint) {
    return false;
  }
  /* Only 3D view */
  ScrArea *area = CTX_wm_area(C);
  if (area && area->spacetype != SPACE_VIEW3D) {
    return false;
  }

  return true;
}

/* Invoke handler: Initialize the operator */
static int grease_pencil_interpolate_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  wmWindow &win = *CTX_wm_window(C);

  if (!grease_pencil_interpolate_init(*C, *op)) {
    grease_pencil_interpolate_exit(*C, *op);
    return OPERATOR_CANCELLED;
  }
  InterpolateOpData &opdata = *static_cast<InterpolateOpData *>(op->customdata);

  /* Set cursor to indicate modal operator. */
  WM_cursor_modal_set(&win, WM_CURSOR_EW_SCROLL);

  grease_pencil_interpolate_status_indicators(*C, opdata);

  WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, nullptr);

  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

enum class InterpolateToolModalEvent : int8_t {
  Cancel = 1,
  Confirm,
  Increase,
  Decrease,
};

/* Modal handler: Events handling during interactive part */
static int grease_pencil_interpolate_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmWindow &win = *CTX_wm_window(C);
  const ARegion &region = *CTX_wm_region(C);
  ScrArea &area = *CTX_wm_area(C);
  InterpolateOpData &opdata = *static_cast<InterpolateOpData *>(op->customdata);
  const bool has_numinput = hasNumInput(&opdata.numeric_input);

  switch (event->type) {
    case EVT_MODAL_MAP: {
      switch (InterpolateToolModalEvent(event->val)) {
        case InterpolateToolModalEvent::Cancel:
          ED_area_status_text(&area, nullptr);
          ED_workspace_status_text(C, nullptr);
          WM_cursor_modal_restore(&win);

          grease_pencil_interpolate_restore(*C, *op);
          grease_pencil_interpolate_exit(*C, *op);
          return OPERATOR_CANCELLED;
        case InterpolateToolModalEvent::Confirm:
          ED_area_status_text(&area, nullptr);
          ED_workspace_status_text(C, nullptr);
          WM_cursor_modal_restore(&win);

          /* Write current factor to properties for the next execution. */
          RNA_float_set(op->ptr, "shift", opdata.shift);

          grease_pencil_interpolate_exit(*C, *op);
          return OPERATOR_FINISHED;
        case InterpolateToolModalEvent::Increase:
          opdata.shift = std::clamp(opdata.init_factor + opdata.shift + 0.01f,
                                    interpolate_factor_min,
                                    interpolate_factor_max) -
                         opdata.init_factor;
          grease_pencil_interpolate_update(*C, *op);
          break;
        case InterpolateToolModalEvent::Decrease:
          opdata.shift = std::clamp(opdata.init_factor + opdata.shift - 0.01f,
                                    interpolate_factor_min,
                                    interpolate_factor_max) -
                         opdata.init_factor;
          grease_pencil_interpolate_update(*C, *op);
          break;
      }
      break;
    }
    case MOUSEMOVE:
      /* Only handle mouse-move if not doing numeric-input. */
      if (!has_numinput) {
        const float mouse_pos = event->mval[0];
        const float factor = std::clamp(
            mouse_pos / region.winx, interpolate_factor_min, interpolate_factor_max);
        opdata.shift = factor - opdata.init_factor;

        grease_pencil_interpolate_update(*C, *op);
      }
      break;
    default: {
      if ((event->val == KM_PRESS) && handleNumInput(C, &opdata.numeric_input, event)) {
        float value = (opdata.init_factor + opdata.shift) * 100.0f;
        applyNumInput(&opdata.numeric_input, &value);
        opdata.shift = std::clamp(value * 0.01f, interpolate_factor_min, interpolate_factor_max) -
                       opdata.init_factor;

        grease_pencil_interpolate_update(*C, *op);
        break;
      }
      /* Unhandled event, allow to pass through. */
      return OPERATOR_RUNNING_MODAL | OPERATOR_PASS_THROUGH;
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

static void grease_pencil_interpolate_cancel(bContext *C, wmOperator *op)
{
  grease_pencil_interpolate_restore(*C, *op);
  grease_pencil_interpolate_exit(*C, *op);
}

static void GREASE_PENCIL_OT_interpolate(wmOperatorType *ot)
{
  static const EnumPropertyItem flip_modes[] = {
      {int(InterpolateFlipMode::None), "NONE", 0, "No Flip", ""},
      {int(InterpolateFlipMode::Flip), "FLIP", 0, "Flip", ""},
      {int(InterpolateFlipMode::FlipAuto), "AUTO", 0, "Automatic", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem gpencil_interpolation_layer_items[] = {
      {int(InterpolateLayerMode::Active), "ACTIVE", 0, "Active", ""},
      {int(InterpolateLayerMode::All), "ALL", 0, "All Layers", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  ot->name = "Grease Pencil Interpolation";
  ot->idname = "GREASE_PENCIL_OT_interpolate";
  ot->description = "Interpolate grease pencil strokes between frames";

  ot->invoke = grease_pencil_interpolate_invoke;
  ot->modal = grease_pencil_interpolate_modal;
  ot->cancel = grease_pencil_interpolate_cancel;
  ot->poll = grease_pencil_interpolate_poll;

  ot->flag = OPTYPE_UNDO | OPTYPE_BLOCKING;

  RNA_def_float_factor(
      ot->srna,
      "shift",
      0.0f,
      -1.0f,
      1.0f,
      "Shift",
      "Bias factor for which frame has more influence on the interpolated strokes",
      -0.9f,
      0.9f);

  RNA_def_enum(ot->srna,
               "layers",
               gpencil_interpolation_layer_items,
               0,
               "Layer",
               "Layers included in the interpolation");

  RNA_def_boolean(ot->srna,
                  "exclude_breakdowns",
                  false,
                  "Exclude Breakdowns",
                  "Exclude existing Breakdowns keyframes as interpolation extremes");

  RNA_def_enum(ot->srna,
               "flip",
               flip_modes,
               int(InterpolateFlipMode::FlipAuto),
               "Flip Mode",
               "Invert destination stroke to match start and end with source stroke");

  RNA_def_int(ot->srna,
              "smooth_steps",
              1,
              1,
              3,
              "Iterations",
              "Number of times to smooth newly created strokes",
              1,
              3);

  RNA_def_float(ot->srna,
                "smooth_factor",
                0.0f,
                0.0f,
                2.0f,
                "Smooth",
                "Amount of smoothing to apply to interpolated strokes, to reduce jitter/noise",
                0.0f,
                2.0f);
}

/** \} */

}  // namespace blender::ed::sculpt_paint::greasepencil

/* -------------------------------------------------------------------- */
/** \name Registration
 * \{ */

void ED_operatortypes_grease_pencil_interpolate()
{
  using namespace blender::ed::sculpt_paint::greasepencil;
  WM_operatortype_append(GREASE_PENCIL_OT_interpolate);
}

void ED_interpolatetool_modal_keymap(wmKeyConfig *keyconf)
{
  using namespace blender::ed::sculpt_paint::greasepencil;
  static const EnumPropertyItem modal_items[] = {
      {int(InterpolateToolModalEvent::Cancel), "CANCEL", 0, "Cancel", ""},
      {int(InterpolateToolModalEvent::Confirm), "CONFIRM", 0, "Confirm", ""},
      {int(InterpolateToolModalEvent::Increase), "INCREASE", 0, "Increase", ""},
      {int(InterpolateToolModalEvent::Decrease), "DECREASE", 0, "Decrease", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, "Interpolate Tool Modal Map");

  /* This function is called for each space-type, only needs to add map once. */
  if (keymap && keymap->modal_items) {
    return;
  }

  keymap = WM_modalkeymap_ensure(keyconf, "Interpolate Tool Modal Map", modal_items);

  WM_modalkeymap_assign(keymap, "GREASE_PENCIL_OT_interpolate");
}

/** \} */
