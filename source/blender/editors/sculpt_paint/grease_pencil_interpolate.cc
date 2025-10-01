/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_deform.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_paint.hh"

#include "BLI_array_utils.hh"
#include "BLI_easing.h"
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

#include "ED_curves.hh"
#include "ED_grease_pencil.hh"
#include "ED_numinput.hh"
#include "ED_screen.hh"

#include "GEO_interpolate_curves.hh"
#include "GEO_smooth_curves.hh"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include <climits>

namespace blender::ed::sculpt_paint::greasepencil {

using ed::greasepencil::InterpolateFlipMode;
using ed::greasepencil::InterpolateLayerMode;

/* -------------------------------------------------------------------- */
/** \name Common Utilities for Interpolation Operators
 * \{ */

/* Modes for the interpolation tool. */
enum class InterpolationType {
  /** Traditional Linear Interpolation. */
  Linear,
  /** CurveMap Defined Interpolation. */
  CurveMap,
  /* Easing Equations. */
  Back,
  Bounce,
  Circular,
  Cubic,
  Elastic,
  Exponential,
  Quadratic,
  Quartic,
  Quintic,
  Sine,
};

/**
 * \note This is a near exact duplicate of #rna_enum_beztriple_interpolation_mode_items,
 * Changes here will likely apply there too.
 */
static const EnumPropertyItem grease_pencil_interpolation_type_items[] = {
    /* Interpolation. */
    RNA_ENUM_ITEM_HEADING(CTX_N_(BLT_I18NCONTEXT_ID_GPENCIL, "Interpolation"),
                          N_("Standard transitions between keyframes")),
    {int(InterpolationType::Linear),
     "LINEAR",
     ICON_IPO_LINEAR,
     "Linear",
     "Straight-line interpolation between A and B (i.e. no ease in/out)"},
    {int(InterpolationType::CurveMap),
     "CUSTOM",
     ICON_IPO_BEZIER,
     "Custom",
     "Custom interpolation defined using a curve map"},

    /* Easing. */
    RNA_ENUM_ITEM_HEADING(CTX_N_(BLT_I18NCONTEXT_ID_GPENCIL, "Easing (by strength)"),
                          N_("Predefined inertial transitions, useful for motion graphics "
                             "(from least to most \"dramatic\")")),
    {int(InterpolationType::Sine),
     "SINE",
     ICON_IPO_SINE,
     "Sinusoidal",
     "Sinusoidal easing (weakest, almost linear but with a slight curvature)"},
    {int(InterpolationType::Quadratic), "QUAD", ICON_IPO_QUAD, "Quadratic", "Quadratic easing"},
    {int(InterpolationType::Cubic), "CUBIC", ICON_IPO_CUBIC, "Cubic", "Cubic easing"},
    {int(InterpolationType::Quartic), "QUART", ICON_IPO_QUART, "Quartic", "Quartic easing"},
    {int(InterpolationType::Quintic), "QUINT", ICON_IPO_QUINT, "Quintic", "Quintic easing"},
    {int(InterpolationType::Exponential),
     "EXPO",
     ICON_IPO_EXPO,
     "Exponential",
     "Exponential easing (dramatic)"},
    {int(InterpolationType::Circular),
     "CIRC",
     ICON_IPO_CIRC,
     "Circular",
     "Circular easing (strongest and most dynamic)"},

    RNA_ENUM_ITEM_HEADING(CTX_N_(BLT_I18NCONTEXT_ID_GPENCIL, "Dynamic Effects"),
                          N_("Simple physics-inspired easing effects")),
    {int(InterpolationType::Back),
     "BACK",
     ICON_IPO_BACK,
     "Back",
     "Cubic easing with overshoot and settle"},
    {int(InterpolationType::Bounce),
     "BOUNCE",
     ICON_IPO_BOUNCE,
     "Bounce",
     "Exponentially decaying parabolic bounce, like when objects collide"},
    {int(InterpolationType::Elastic),
     "ELASTIC",
     ICON_IPO_ELASTIC,
     "Elastic",
     "Exponentially decaying sine wave, like an elastic band"},

    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem grease_pencil_interpolate_flip_mode_items[] = {
    {int(InterpolateFlipMode::None), "NONE", 0, "No Flip", ""},
    {int(InterpolateFlipMode::Flip), "FLIP", 0, "Flip", ""},
    {int(InterpolateFlipMode::FlipAuto), "AUTO", 0, "Automatic", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem grease_pencil_interpolate_layer_items[] = {
    {int(InterpolateLayerMode::Active), "ACTIVE", 0, "Active", ""},
    {int(InterpolateLayerMode::All), "ALL", 0, "All Layers", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

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

  static InterpolateOpData *from_operator(const bContext &C, const wmOperator &op);
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
static bool find_curve_mapping_from_index(const GreasePencil &grease_pencil,
                                          const bke::greasepencil::Layer &layer,
                                          const int current_frame,
                                          const bool exclude_breakdowns,
                                          const bool only_selected,
                                          InterpolationPairs &pairs)
{
  using bke::greasepencil::Drawing;

  const std::optional<FramesMapKeyIntervalT> interval = find_frames_interval(
      layer, current_frame, exclude_breakdowns);
  if (!interval) {
    return false;
  }

  BLI_assert(layer.has_drawing_at(interval->first));
  BLI_assert(layer.has_drawing_at(interval->second));
  const Drawing &from_drawing = *grease_pencil.get_drawing_at(layer, interval->first);
  const Drawing &to_drawing = *grease_pencil.get_drawing_at(layer, interval->second);
  /* In addition to interpolated pairs, the unselected original strokes are also included, making
   * the total pair count the same as the "from" curve count. */
  const int pairs_num = from_drawing.strokes().curves_num();

  const int old_pairs_num = pairs.from_frames.size();
  pairs.from_frames.append_n_times(interval->first, pairs_num);
  pairs.to_frames.append_n_times(interval->second, pairs_num);
  pairs.from_curves.resize(old_pairs_num + pairs_num);
  pairs.to_curves.resize(old_pairs_num + pairs_num);
  MutableSpan<int> from_curves = pairs.from_curves.as_mutable_span().slice(old_pairs_num,
                                                                           pairs_num);
  MutableSpan<int> to_curves = pairs.to_curves.as_mutable_span().slice(old_pairs_num, pairs_num);

  /* Write source indices into the pair data. If one drawing has more selected curves than the
   * other the remainder is ignored. */

  IndexMaskMemory memory;
  IndexMask from_selection, to_selection;
  if (only_selected && ed::curves::has_anything_selected(from_drawing.strokes()) &&
      ed::curves::has_anything_selected(to_drawing.strokes()))
  {
    from_selection = ed::curves::retrieve_selected_curves(from_drawing.strokes(), memory);
    to_selection = ed::curves::retrieve_selected_curves(to_drawing.strokes(), memory);
  }
  else {
    from_selection = from_drawing.strokes().curves_range();
    to_selection = to_drawing.strokes().curves_range();
  }
  /* Discard additional elements of the larger selection. */
  if (from_selection.size() > to_selection.size()) {
    from_selection = from_selection.slice(0, to_selection.size());
  }
  else if (to_selection.size() > from_selection.size()) {
    to_selection = to_selection.slice(0, from_selection.size());
  }

  /* By default: copy the "from" curve and ignore the "to" curve. */
  array_utils::fill_index_range(from_curves);
  to_curves.fill(-1);
  /* Selected curves are interpolated. */
  IndexMask::foreach_segment_zipped({from_selection, to_selection},
                                    [&](Span<IndexMaskSegment> segments) {
                                      const IndexMaskSegment &from_segment = segments[0];
                                      const IndexMaskSegment &to_segment = segments[1];
                                      BLI_assert(from_segment.size() == to_segment.size());
                                      for (const int i : from_segment.index_range()) {
                                        to_curves[from_segment[i]] = to_segment[i];
                                      }
                                      return true;
                                    });

  return true;
}

InterpolateOpData *InterpolateOpData::from_operator(const bContext &C, const wmOperator &op)
{
  using bke::greasepencil::Drawing;
  using bke::greasepencil::Layer;

  const Scene &scene = *CTX_data_scene(&C);
  const int current_frame = scene.r.cfra;
  const Object &object = *CTX_data_active_object(&C);
  const GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);

  if (!grease_pencil.has_active_layer()) {
    return nullptr;
  }

  const Layer &active_layer = *grease_pencil.get_active_layer();

  InterpolateOpData *data = MEM_new<InterpolateOpData>(__func__);

  if (RNA_struct_find_property(op.ptr, "shift") != nullptr) {
    data->shift = RNA_float_get(op.ptr, "shift");
  }
  data->exclude_breakdowns = RNA_boolean_get(op.ptr, "exclude_breakdowns");
  data->flipmode = InterpolateFlipMode(RNA_enum_get(op.ptr, "flip"));
  data->smooth_factor = RNA_float_get(op.ptr, "smooth_factor");
  data->smooth_steps = RNA_int_get(op.ptr, "smooth_steps");
  data->active_layer_index = *grease_pencil.get_layer_index(active_layer);
  const bool use_selection = RNA_boolean_get(op.ptr, "use_selection");

  const auto layer_mode = InterpolateLayerMode(RNA_enum_get(op.ptr, "layers"));
  switch (layer_mode) {
    case InterpolateLayerMode::Active:
      data->layer_mask = IndexRange::from_single(data->active_layer_index);
      break;
    case InterpolateLayerMode::All:
      data->layer_mask = IndexMask::from_predicate(
          grease_pencil.layers().index_range(),
          GrainSize(1024),
          data->layer_mask_memory,
          [&](const int layer_index) { return grease_pencil.layer(layer_index).is_editable(); });
      break;
  }

  bool found_mapping = false;
  data->layer_data.reinitialize(grease_pencil.layers().size());
  data->layer_mask.foreach_index([&](const int layer_index) {
    const Layer &layer = grease_pencil.layer(layer_index);
    InterpolateOpData::LayerData &layer_data = data->layer_data[layer_index];

    /* Pair from/to curves by index. */
    const bool has_curve_mapping = find_curve_mapping_from_index(grease_pencil,
                                                                 layer,
                                                                 current_frame,
                                                                 data->exclude_breakdowns,
                                                                 use_selection,
                                                                 layer_data.curve_pairs);
    found_mapping = found_mapping || has_curve_mapping;
  });

  /* No mapping between frames was found. */
  if (!found_mapping) {
    MEM_delete(data);
    return nullptr;
  }

  const std::optional<FramesMapKeyIntervalT> active_layer_interval = find_frames_interval(
      active_layer, current_frame, data->exclude_breakdowns);
  data->init_factor = active_layer_interval ?
                          float(current_frame - active_layer_interval->first) /
                              (active_layer_interval->second - active_layer_interval->first + 1) :
                          0.5f;

  return data;
}

/* Find ranges of sorted pairs with the same from/to frame intervals. */
static Vector<int> find_curve_pair_offsets(const InterpolationPairs &curve_pairs,
                                           const Span<int> order)
{
  Vector<int> pair_offsets;

  int prev_from_frame = INT_MIN;
  int prev_to_frame = INT_MIN;
  int current_count = 0;
  for (const int pair_index : order) {
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
    return {};
  }

  /* Extra element for the total size needed for OffsetIndices. */
  pair_offsets.append(0);
  offset_indices::accumulate_counts_to_offsets(pair_offsets);

  return pair_offsets;
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
  Vector<int> pair_offsets = find_curve_pair_offsets(curve_pairs, sorted_pairs);
  const OffsetIndices<int> curves_by_pair(pair_offsets);

  /* Compute curve length and flip mode for each pair. */
  Array<int> dst_curve_offsets(curves_by_pair.size() + 1, 0);
  Array<bool> dst_curve_flip(curves_by_pair.size(), false);
  const OffsetIndices<int> dst_points_by_curve = [&]() {
    /* Last entry for overall size. */
    if (curves_by_pair.is_empty()) {
      return OffsetIndices<int>{};
    }

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

        int curve_size = 0;
        bool curve_flip = false;
        if (from_curve < 0 && to_curve < 0) {
          /* No output curve. */
        }
        else if (from_curve < 0) {
          const IndexRange to_points = to_points_by_curve[to_curve];
          curve_size = to_points.size();
          curve_flip = false;
        }
        else if (to_curve < 0) {
          const IndexRange from_points = from_points_by_curve[from_curve];
          curve_size = from_points.size();
          curve_flip = false;
        }
        else {
          const IndexRange from_points = from_points_by_curve[from_curve];
          const IndexRange to_points = to_points_by_curve[to_curve];

          curve_size = std::max(from_points.size(), to_points.size());
          switch (flip_mode) {
            case InterpolateFlipMode::None:
              curve_flip = false;
              break;
            case InterpolateFlipMode::Flip:
              curve_flip = true;
              break;
            case InterpolateFlipMode::FlipAuto: {
              curve_flip = compute_auto_flip(from_positions.slice(from_points),
                                             to_positions.slice(to_points));
              break;
            }
          }
        }

        dst_curve_offsets[pair_index] = curve_size;
        dst_curve_flip[pair_index] = curve_flip;
      }
    }
    return offset_indices::accumulate_counts_to_offsets(dst_curve_offsets);
  }();
  const int dst_point_num = dst_points_by_curve.total_size();

  bke::CurvesGeometry dst_curves(dst_point_num, dst_curve_num);
  /* Offsets are empty when there are no curves. */
  if (dst_curve_num > 0) {
    dst_curves.offsets_for_write().copy_from(dst_curve_offsets);
  }

  /* Copy vertex group names since we still have other parts of the code depends on vertex group
   * names to be available. */
  BKE_defgroup_copy_list(&dst_curves.vertex_group_names, &grease_pencil.vertex_group_names);

  /* Sorted map arrays that can be passed to the interpolation function directly.
   * These index maps have the same order as the sorted indices, so slices of indices can be used
   * for interpolating all curves of a frame pair at once. */
  Array<int> from_curve_buffer(dst_curve_num);
  Array<int> to_curve_buffer(dst_curve_num);
  Array<int> from_sample_indices(dst_point_num);
  Array<int> to_sample_indices(dst_point_num);
  Array<float> from_sample_factors(dst_point_num);
  Array<float> to_sample_factors(dst_point_num);
  IndexMaskMemory memory;

  for (const int pair_range_i : curves_by_pair.index_range()) {
    const IndexRange pair_range = curves_by_pair[pair_range_i];
    /* Subset of target curves that are filled by this frame pair. Selection is built from pair
     * indices, which correspond to dst curve indices. */
    const IndexMask dst_curve_mask = IndexMask::from_indices(
        sorted_pairs.as_span().slice(pair_range), memory);
    MutableSpan<int> from_indices = from_curve_buffer.as_mutable_span().slice(pair_range);
    MutableSpan<int> to_indices = to_curve_buffer.as_mutable_span().slice(pair_range);

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
    const VArray<bool> from_curves_cyclic = from_drawing->strokes().cyclic();
    const VArray<bool> to_curves_cyclic = to_drawing->strokes().cyclic();

    for (const int i : pair_range.index_range()) {
      const int pair_index = sorted_pairs[pair_range[i]];
      const IndexRange dst_points = dst_points_by_curve[pair_index];
      from_indices[i] = curve_pairs.from_curves[pair_index];
      to_indices[i] = curve_pairs.to_curves[pair_index];

      const int from_curve = curve_pairs.from_curves[pair_index];
      const int to_curve = curve_pairs.to_curves[pair_index];

      BLI_assert(from_curve >= 0 || to_curve >= 0);
      if (to_curve < 0) {
        /* Copy "from" curve. */
        array_utils::fill_index_range(from_sample_indices.as_mutable_span().slice(dst_points));
        from_sample_factors.fill(0.0f);
        continue;
      }
      if (from_curve < 0) {
        /* Copy "to" curve. */
        array_utils::fill_index_range(to_sample_indices.as_mutable_span().slice(dst_points));
        to_sample_factors.fill(0.0f);
        continue;
      }

      const IndexRange from_points = from_points_by_curve[from_curve];
      const IndexRange to_points = to_points_by_curve[to_curve];
      if (from_points.size() >= to_points.size()) {
        /* Target curve samples match 'from' points. */
        BLI_assert(from_points.size() == dst_points.size());
        array_utils::fill_index_range(from_sample_indices.as_mutable_span().slice(dst_points));
        from_sample_factors.as_mutable_span().slice(dst_points).fill(0.0f);
        geometry::sample_curve_padded(to_drawing->strokes(),
                                      to_curve,
                                      to_curves_cyclic[to_curve],
                                      dst_curve_flip[pair_index],
                                      to_sample_indices.as_mutable_span().slice(dst_points),
                                      to_sample_factors.as_mutable_span().slice(dst_points));
      }
      else {
        /* Target curve samples match 'to' points. */
        BLI_assert(to_points.size() == dst_points.size());
        geometry::sample_curve_padded(from_drawing->strokes(),
                                      from_curve,
                                      from_curves_cyclic[from_curve],
                                      dst_curve_flip[pair_index],
                                      from_sample_indices.as_mutable_span().slice(dst_points),
                                      from_sample_factors.as_mutable_span().slice(dst_points));
        array_utils::fill_index_range(to_sample_indices.as_mutable_span().slice(dst_points));
        to_sample_factors.fill(0.0f);
      }
    }

    geometry::interpolate_curves_with_samples(from_drawing->strokes(),
                                              to_drawing->strokes(),
                                              from_indices,
                                              to_indices,
                                              from_sample_indices,
                                              to_sample_indices,
                                              from_sample_factors,
                                              to_sample_factors,
                                              dst_curve_mask,
                                              mix_factor,
                                              dst_curves,
                                              memory);
  }

  return dst_curves;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Interpolate Operator
 * \{ */

static void grease_pencil_interpolate_status_indicators(bContext &C,
                                                        const InterpolateOpData &opdata)
{
  Scene &scene = *CTX_data_scene(&C);
  ScrArea &area = *CTX_wm_area(&C);

  const StringRef msg = IFACE_("GPencil Interpolation: ");

  std::string status;
  if (hasNumInput(&opdata.numeric_input)) {
    char str_ofs[NUM_STR_REP_LEN];
    outputNumInput(&const_cast<NumInput &>(opdata.numeric_input), str_ofs, scene.unit);
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

  static constexpr eBezTriple_KeyframeType keyframe_type = BEZT_KEYTYPE_BREAKDOWN;

  if (Drawing *drawing = get_drawing_at_exact_frame(grease_pencil, layer, frame_number)) {
    layer_data.orig_curves = drawing->strokes();
    return drawing;
  }
  return grease_pencil.insert_frame(layer, frame_number, 0, keyframe_type);
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
    Layer &layer = grease_pencil.layer(layer_index);
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
          VArray<bool>::from_single(true, interpolated_curves.points_num()),
          interpolated_curves.cyclic(),
          opdata.smooth_steps,
          opdata.smooth_factor,
          false,
          true,
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
    Layer &layer = grease_pencil.layer(layer_index);
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
  using bke::greasepencil::Layer;

  op.customdata = InterpolateOpData::from_operator(C, op);
  if (op.customdata == nullptr) {
    return false;
  }
  InterpolateOpData &data = *static_cast<InterpolateOpData *>(op.customdata);

  const Scene &scene = *CTX_data_scene(&C);
  const int current_frame = scene.r.cfra;
  Object &object = *CTX_data_active_object(&C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);

  /* Create target frames. */
  data.layer_mask.foreach_index([&](const int layer_index) {
    Layer &layer = grease_pencil.layer(layer_index);
    InterpolateOpData::LayerData &layer_data = data.layer_data[layer_index];

    ensure_drawing_at_exact_frame(grease_pencil, layer, layer_data, current_frame);
  });

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
static wmOperatorStatus grease_pencil_interpolate_invoke(bContext *C,
                                                         wmOperator *op,
                                                         const wmEvent * /*event*/)
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
static wmOperatorStatus grease_pencil_interpolate_modal(bContext *C,
                                                        wmOperator *op,
                                                        const wmEvent *event)
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
  ot->name = "Grease Pencil Interpolation";
  ot->idname = "GREASE_PENCIL_OT_interpolate";
  ot->description = "Interpolate Grease Pencil strokes between frames";

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
               grease_pencil_interpolate_layer_items,
               0,
               "Layer",
               "Layers included in the interpolation");

  RNA_def_boolean(ot->srna,
                  "exclude_breakdowns",
                  false,
                  "Exclude Breakdowns",
                  "Exclude existing Breakdowns keyframes as interpolation extremes");

  RNA_def_boolean(ot->srna,
                  "use_selection",
                  false,
                  "Use Selection",
                  "Use only selected strokes for interpolating");

  RNA_def_enum(ot->srna,
               "flip",
               grease_pencil_interpolate_flip_mode_items,
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

/* -------------------------------------------------------------------- */
/** \name Interpolate Sequence Operator
 * \{ */

/* Helper: Perform easing equation calculations for GP interpolation operator. */
static float grease_pencil_interpolate_sequence_easing_calc(const eBezTriple_Easing easing,
                                                            const InterpolationType type,
                                                            const float back_easing,
                                                            const float amplitude,
                                                            const float period,
                                                            const CurveMapping &custom_ipo,
                                                            const float time)
{
  constexpr float begin = 0.0f;
  constexpr float change = 1.0f;
  constexpr float duration = 1.0f;

  switch (type) {
    case InterpolationType::Linear:
      return time;

    case InterpolationType::CurveMap:
      return BKE_curvemapping_evaluateF(&custom_ipo, 0, time);

    case InterpolationType::Back:
      switch (easing) {
        case BEZT_IPO_EASE_IN:
          return BLI_easing_back_ease_in(time, begin, change, duration, back_easing);
        case BEZT_IPO_EASE_OUT:
          return BLI_easing_back_ease_out(time, begin, change, duration, back_easing);
        case BEZT_IPO_EASE_IN_OUT:
          return BLI_easing_back_ease_in_out(time, begin, change, duration, back_easing);

        default:
          return BLI_easing_back_ease_out(time, begin, change, duration, back_easing);
      }
      break;

    case InterpolationType::Bounce:
      switch (easing) {
        case BEZT_IPO_EASE_IN:
          return BLI_easing_bounce_ease_in(time, begin, change, duration);
        case BEZT_IPO_EASE_OUT:
          return BLI_easing_bounce_ease_out(time, begin, change, duration);
        case BEZT_IPO_EASE_IN_OUT:
          return BLI_easing_bounce_ease_in_out(time, begin, change, duration);

        default:
          return BLI_easing_bounce_ease_out(time, begin, change, duration);
      }
      break;

    case InterpolationType::Circular:
      switch (easing) {
        case BEZT_IPO_EASE_IN:
          return BLI_easing_circ_ease_in(time, begin, change, duration);
        case BEZT_IPO_EASE_OUT:
          return BLI_easing_circ_ease_out(time, begin, change, duration);
        case BEZT_IPO_EASE_IN_OUT:
          return BLI_easing_circ_ease_in_out(time, begin, change, duration);

        default:
          return BLI_easing_circ_ease_in(time, begin, change, duration);
      }
      break;

    case InterpolationType::Cubic:
      switch (easing) {
        case BEZT_IPO_EASE_IN:
          return BLI_easing_cubic_ease_in(time, begin, change, duration);
        case BEZT_IPO_EASE_OUT:
          return BLI_easing_cubic_ease_out(time, begin, change, duration);
        case BEZT_IPO_EASE_IN_OUT:
          return BLI_easing_cubic_ease_in_out(time, begin, change, duration);

        default:
          return BLI_easing_cubic_ease_in(time, begin, change, duration);
      }
      break;

    case InterpolationType::Elastic:
      switch (easing) {
        case BEZT_IPO_EASE_IN:
          return BLI_easing_elastic_ease_in(time, begin, change, duration, amplitude, period);
        case BEZT_IPO_EASE_OUT:
          return BLI_easing_elastic_ease_out(time, begin, change, duration, amplitude, period);
        case BEZT_IPO_EASE_IN_OUT:
          return BLI_easing_elastic_ease_in_out(time, begin, change, duration, amplitude, period);

        default:
          return BLI_easing_elastic_ease_out(time, begin, change, duration, amplitude, period);
      }
      break;

    case InterpolationType::Exponential:
      switch (easing) {
        case BEZT_IPO_EASE_IN:
          return BLI_easing_expo_ease_in(time, begin, change, duration);
        case BEZT_IPO_EASE_OUT:
          return BLI_easing_expo_ease_out(time, begin, change, duration);
        case BEZT_IPO_EASE_IN_OUT:
          return BLI_easing_expo_ease_in_out(time, begin, change, duration);

        default:
          return BLI_easing_expo_ease_in(time, begin, change, duration);
      }
      break;

    case InterpolationType::Quadratic:
      switch (easing) {
        case BEZT_IPO_EASE_IN:
          return BLI_easing_quad_ease_in(time, begin, change, duration);
        case BEZT_IPO_EASE_OUT:
          return BLI_easing_quad_ease_out(time, begin, change, duration);
        case BEZT_IPO_EASE_IN_OUT:
          return BLI_easing_quad_ease_in_out(time, begin, change, duration);

        default:
          return BLI_easing_quad_ease_in(time, begin, change, duration);
      }
      break;

    case InterpolationType::Quartic:
      switch (easing) {
        case BEZT_IPO_EASE_IN:
          return BLI_easing_quart_ease_in(time, begin, change, duration);
        case BEZT_IPO_EASE_OUT:
          return BLI_easing_quart_ease_out(time, begin, change, duration);
        case BEZT_IPO_EASE_IN_OUT:
          return BLI_easing_quart_ease_in_out(time, begin, change, duration);

        default:
          return BLI_easing_quart_ease_in(time, begin, change, duration);
      }
      break;

    case InterpolationType::Quintic:
      switch (easing) {
        case BEZT_IPO_EASE_IN:
          return BLI_easing_quint_ease_in(time, begin, change, duration);
        case BEZT_IPO_EASE_OUT:
          return BLI_easing_quint_ease_out(time, begin, change, duration);
        case BEZT_IPO_EASE_IN_OUT:
          return BLI_easing_quint_ease_in_out(time, begin, change, duration);

        default:
          return BLI_easing_quint_ease_in(time, begin, change, duration);
      }
      break;

    case InterpolationType::Sine:
      switch (easing) {
        case BEZT_IPO_EASE_IN:
          return BLI_easing_sine_ease_in(time, begin, change, duration);
        case BEZT_IPO_EASE_OUT:
          return BLI_easing_sine_ease_out(time, begin, change, duration);
        case BEZT_IPO_EASE_IN_OUT:
          return BLI_easing_sine_ease_in_out(time, begin, change, duration);

        default:
          return BLI_easing_sine_ease_in(time, begin, change, duration);
      }
      break;

    default:
      BLI_assert_unreachable();
      break;
  }

  return time;
}

static wmOperatorStatus grease_pencil_interpolate_sequence_exec(bContext *C, wmOperator *op)
{
  using bke::greasepencil::Drawing;
  using bke::greasepencil::Layer;

  op->customdata = InterpolateOpData::from_operator(*C, *op);
  if (op->customdata == nullptr) {
    return OPERATOR_FINISHED;
  }
  InterpolateOpData &opdata = *static_cast<InterpolateOpData *>(op->customdata);

  const Scene &scene = *CTX_data_scene(C);
  const int current_frame = scene.r.cfra;
  Object &object = *CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);
  ToolSettings &ts = *CTX_data_tool_settings(C);
  const InterpolationType type = InterpolationType(RNA_enum_get(op->ptr, "type"));
  const eBezTriple_Easing easing = eBezTriple_Easing(RNA_enum_get(op->ptr, "easing"));
  const float back_easing = RNA_float_get(op->ptr, "back");
  const float amplitude = RNA_float_get(op->ptr, "amplitude");
  const float period = RNA_float_get(op->ptr, "period");
  const int step = RNA_int_get(op->ptr, "step");

  GP_Interpolate_Settings &ipo_settings = ts.gp_interpolate;
  if (ipo_settings.custom_ipo == nullptr) {
    ipo_settings.custom_ipo = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
  }
  BKE_curvemapping_init(ipo_settings.custom_ipo);

  opdata.layer_mask.foreach_index([&](const int layer_index) {
    Layer &layer = grease_pencil.layer(layer_index);
    InterpolateOpData::LayerData &layer_data = opdata.layer_data[layer_index];

    std::optional<FramesMapKeyIntervalT> interval = find_frames_interval(
        layer, current_frame, opdata.exclude_breakdowns);
    if (!interval) {
      return;
    }

    const int frame_range_size = interval->second - interval->first + 1;

    /* First and last frame are ignored. */
    for (int cframe = interval->first + step; cframe < interval->second; cframe += step) {
      ensure_drawing_at_exact_frame(grease_pencil, layer, layer_data, cframe);
      Drawing *dst_drawing = get_drawing_at_exact_frame(grease_pencil, layer, cframe);
      if (dst_drawing == nullptr) {
        return;
      }

      const float base_factor = float(cframe - interval->first) /
                                std::max(frame_range_size - 1, 1);
      const float mix_factor = grease_pencil_interpolate_sequence_easing_calc(
          easing, type, back_easing, amplitude, period, *ipo_settings.custom_ipo, base_factor);

      bke::CurvesGeometry interpolated_curves = interpolate_between_curves(
          grease_pencil, layer, layer_data.curve_pairs, mix_factor, opdata.flipmode);

      if (opdata.smooth_factor > 0.0f && opdata.smooth_steps > 0) {
        MutableSpan<float3> positions = interpolated_curves.positions_for_write();
        geometry::smooth_curve_attribute(
            interpolated_curves.curves_range(),
            interpolated_curves.points_by_curve(),
            VArray<bool>::from_single(true, interpolated_curves.points_num()),
            interpolated_curves.cyclic(),
            opdata.smooth_steps,
            opdata.smooth_factor,
            false,
            true,
            positions);
        interpolated_curves.tag_positions_changed();
      }

      dst_drawing->strokes_for_write() = std::move(interpolated_curves);
      dst_drawing->tag_topology_changed();
    }
  });

  /* Notifiers */
  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  MEM_delete(static_cast<InterpolateOpData *>(op->customdata));
  op->customdata = nullptr;

  return OPERATOR_FINISHED;
}

static void grease_pencil_interpolate_sequence_ui(bContext *C, wmOperator *op)
{
  uiLayout *layout = op->layout;
  uiLayout *col, *row;

  const InterpolationType type = InterpolationType(RNA_enum_get(op->ptr, "type"));

  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);
  row = &layout->row(true);
  row->prop(op->ptr, "step", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  row = &layout->row(true);
  row->prop(op->ptr, "layers", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  if (CTX_data_mode_enum(C) == CTX_MODE_EDIT_GPENCIL_LEGACY) {
    row = &layout->row(true);
    row->prop(op->ptr, "interpolate_selected_only", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  row = &layout->row(true);
  row->prop(op->ptr, "exclude_breakdowns", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  row = &layout->row(true);
  row->prop(op->ptr, "use_selection", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  row = &layout->row(true);
  row->prop(op->ptr, "flip", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  col = &layout->column(true);
  col->prop(op->ptr, "smooth_factor", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(op->ptr, "smooth_steps", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  row = &layout->row(true);
  row->prop(op->ptr, "type", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  if (type == InterpolationType::CurveMap) {
    /* Get an RNA pointer to ToolSettings to give to the custom curve. */
    Scene *scene = CTX_data_scene(C);
    ToolSettings *ts = scene->toolsettings;
    PointerRNA gpsettings_ptr = RNA_pointer_create_discrete(
        &scene->id, &RNA_GPencilInterpolateSettings, &ts->gp_interpolate);
    uiTemplateCurveMapping(
        layout, &gpsettings_ptr, "interpolation_curve", 0, false, true, true, false, false);
  }
  else if (type != InterpolationType::Linear) {
    row = &layout->row(false);
    row->prop(op->ptr, "easing", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    if (type == InterpolationType::Back) {
      row = &layout->row(false);
      row->prop(op->ptr, "back", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    }
    else if (type == InterpolationType::Elastic) {
      row = &layout->row(false);
      row->prop(op->ptr, "amplitude", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      row = &layout->row(false);
      row->prop(op->ptr, "period", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    }
  }
}

static void GREASE_PENCIL_OT_interpolate_sequence(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Interpolate Sequence";
  ot->idname = "GREASE_PENCIL_OT_interpolate_sequence";
  ot->translation_context = BLT_I18NCONTEXT_ID_GPENCIL;
  ot->description = "Generate 'in-betweens' to smoothly interpolate between Grease Pencil frames";

  ot->exec = grease_pencil_interpolate_sequence_exec;
  ot->poll = grease_pencil_interpolate_poll;
  ot->ui = grease_pencil_interpolate_sequence_ui;

  RNA_def_int(ot->srna,
              "step",
              1,
              1,
              MAXFRAME,
              "Step",
              "Number of frames between generated interpolated frames",
              1,
              MAXFRAME);

  RNA_def_enum(ot->srna,
               "layers",
               grease_pencil_interpolate_layer_items,
               0,
               "Layer",
               "Layers included in the interpolation");

  RNA_def_boolean(ot->srna,
                  "exclude_breakdowns",
                  false,
                  "Exclude Breakdowns",
                  "Exclude existing Breakdowns keyframes as interpolation extremes");

  RNA_def_boolean(ot->srna,
                  "use_selection",
                  false,
                  "Use Selection",
                  "Use only selected strokes for interpolating");

  RNA_def_enum(ot->srna,
               "flip",
               grease_pencil_interpolate_flip_mode_items,
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

  prop = RNA_def_enum(ot->srna,
                      "type",
                      grease_pencil_interpolation_type_items,
                      0,
                      "Type",
                      "Interpolation method to use the next time 'Interpolate Sequence' is run");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_GPENCIL);

  prop = RNA_def_enum(
      ot->srna,
      "easing",
      rna_enum_beztriple_interpolation_easing_items,
      BEZT_IPO_LIN,
      "Easing",
      "Which ends of the segment between the preceding and following Grease Pencil frames "
      "easing interpolation is applied to");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_GPENCIL);

  prop = RNA_def_float(ot->srna,
                       "back",
                       1.702f,
                       0.0f,
                       FLT_MAX,
                       "Back",
                       "Amount of overshoot for 'back' easing",
                       0.0f,
                       FLT_MAX);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_GPENCIL);

  RNA_def_float(ot->srna,
                "amplitude",
                0.15f,
                0.0f,
                FLT_MAX,
                "Amplitude",
                "Amount to boost elastic bounces for 'elastic' easing",
                0.0f,
                FLT_MAX);

  RNA_def_float(ot->srna,
                "period",
                0.15f,
                -FLT_MAX,
                FLT_MAX,
                "Period",
                "Time between bounces for elastic easing",
                -FLT_MAX,
                FLT_MAX);

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
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
  WM_operatortype_append(GREASE_PENCIL_OT_interpolate_sequence);
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
