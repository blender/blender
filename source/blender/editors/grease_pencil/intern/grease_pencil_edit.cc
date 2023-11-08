/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "BLI_array_utils.hh"
#include "BLI_index_mask.hh"
#include "BLI_index_range.hh"
#include "BLI_math_geom.h"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_stack.hh"

#include "BKE_context.h"
#include "BKE_grease_pencil.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "DEG_depsgraph.hh"

#include "ED_curves.hh"
#include "ED_grease_pencil.hh"
#include "ED_screen.hh"

#include "WM_api.hh"

namespace blender::ed::greasepencil {

bool active_grease_pencil_poll(bContext *C)
{
  Object *object = CTX_data_active_object(C);
  if (object == nullptr || object->type != OB_GREASE_PENCIL) {
    return false;
  }
  return true;
}

bool editable_grease_pencil_poll(bContext *C)
{
  Object *object = CTX_data_active_object(C);
  if (object == nullptr || object->type != OB_GREASE_PENCIL) {
    return false;
  }
  if (!ED_operator_object_active_editable_ex(C, object)) {
    return false;
  }
  if ((object->mode & OB_MODE_EDIT) == 0) {
    return false;
  }
  return true;
}

bool editable_grease_pencil_point_selection_poll(bContext *C)
{
  if (!editable_grease_pencil_poll(C)) {
    return false;
  }

  /* Allowed: point and segment selection mode, not allowed: stroke selection mode. */
  ToolSettings *ts = CTX_data_tool_settings(C);
  return (ts->gpencil_selectmode_edit != GP_SELECTMODE_STROKE);
}

bool grease_pencil_painting_poll(bContext *C)
{
  if (!active_grease_pencil_poll(C)) {
    return false;
  }
  Object *object = CTX_data_active_object(C);
  if ((object->mode & OB_MODE_PAINT_GREASE_PENCIL) == 0) {
    return false;
  }
  ToolSettings *ts = CTX_data_tool_settings(C);
  if (!ts || !ts->gp_paint) {
    return false;
  }
  return true;
}

static void keymap_grease_pencil_editing(wmKeyConfig *keyconf)
{
  wmKeyMap *keymap = WM_keymap_ensure(
      keyconf, "Grease Pencil Edit Mode", SPACE_EMPTY, RGN_TYPE_WINDOW);
  keymap->poll = editable_grease_pencil_poll;
}

static void keymap_grease_pencil_painting(wmKeyConfig *keyconf)
{
  wmKeyMap *keymap = WM_keymap_ensure(
      keyconf, "Grease Pencil Paint Mode", SPACE_EMPTY, RGN_TYPE_WINDOW);
  keymap->poll = grease_pencil_painting_poll;
}

/* -------------------------------------------------------------------- */
/** \name Smooth Stroke Operator
 * \{ */

template<typename T>
static void gaussian_blur_1D(const Span<T> src,
                             const int64_t iterations,
                             const float influence,
                             const bool smooth_ends,
                             const bool keep_shape,
                             const bool is_cyclic,
                             MutableSpan<T> dst)
{
  /**
   * 1D Gaussian-like smoothing function.
   *
   * NOTE: This is the algorithm used by #BKE_gpencil_stroke_smooth_point (legacy),
   *       but generalized and written in C++.
   *
   * This function uses a binomial kernel, which is the discrete version of gaussian blur.
   * The weight for a value at the relative index is:
   * `w = nCr(n, j + n/2) / 2^n = (n/1 * (n-1)/2 * ... * (n-j-n/2)/(j+n/2)) / 2^n`.
   * All weights together sum up to 1.
   * This is equivalent to doing multiple iterations of averaging neighbors,
   * where: `n = iterations * 2 and -n/2 <= j <= n/2`.
   *
   * Now the problem is that `nCr(n, j + n/2)` is very hard to compute for `n > 500`, since even
   * double precision isn't sufficient. A very good robust approximation for `n > 20` is:
   * `nCr(n, j + n/2) / 2^n = sqrt(2/(pi*n)) * exp(-2*j*j/n)`.
   *
   * `keep_shape` is a new option to stop the points from severely deforming.
   * It uses different partially negative weights.
   * `w = 2 * (nCr(n, j + n/2) / 2^n) - (nCr(3*n, j + n) / 2^(3*n))`
   * `  ~ 2 * sqrt(2/(pi*n)) * exp(-2*j*j/n) - sqrt(2/(pi*3*n)) * exp(-2*j*j/(3*n))`
   * All weights still sum up to 1.
   * Note that these weights only work because the averaging is done in relative coordinates.
   */

  BLI_assert(!src.is_empty());
  BLI_assert(src.size() == dst.size());

  /* Avoid computation if the there is just one point. */
  if (src.size() == 1) {
    return;
  }

  /* Weight Initialization. */
  const int64_t n_half = keep_shape ? (iterations * iterations) / 8 + iterations :
                                      (iterations * iterations) / 4 + 2 * iterations + 12;
  double w = keep_shape ? 2.0 : 1.0;
  double w2 = keep_shape ?
                  (1.0 / M_SQRT3) * exp((2 * iterations * iterations) / double(n_half * 3)) :
                  0.0;
  Array<double> total_weight(src.size(), 0.0);

  const int64_t total_points = src.size();
  const int64_t last_pt = total_points - 1;

  auto is_end_and_fixed = [smooth_ends, is_cyclic, last_pt](int index) {
    return !smooth_ends && !is_cyclic && ELEM(index, 0, last_pt);
  };

  /* Initialize at zero. */
  threading::parallel_for(dst.index_range(), 256, [&](const IndexRange range) {
    for (const int64_t index : range) {
      if (!is_end_and_fixed(index)) {
        dst[index] = T(0);
      }
    }
  });

  for (const int64_t step : IndexRange(iterations)) {
    const int64_t offset = iterations - step;
    threading::parallel_for(dst.index_range(), 256, [&](const IndexRange range) {
      for (const int64_t index : range) {
        /* Filter out endpoints. */
        if (is_end_and_fixed(index)) {
          continue;
        }

        double w_before = w - w2;
        double w_after = w - w2;

        /* Compute the neighboring points. */
        int64_t before = index - offset;
        int64_t after = index + offset;
        if (is_cyclic) {
          before = (before % total_points + total_points) % total_points;
          after = after % total_points;
        }
        else {
          if (!smooth_ends && (before < 0)) {
            w_before *= -before / float(index);
          }
          before = math::max(before, int64_t(0));

          if (!smooth_ends && (after > last_pt)) {
            w_after *= (after - (total_points - 1)) / float(total_points - 1 - index);
          }
          after = math::min(after, last_pt);
        }

        /* Add the neighboring values. */
        const T bval = src[before];
        const T aval = src[after];
        const T cval = src[index];

        dst[index] += (bval - cval) * w_before;
        dst[index] += (aval - cval) * w_after;

        /* Update the weight values. */
        total_weight[index] += w_before;
        total_weight[index] += w_after;
      }
    });

    w *= (n_half + offset) / double(n_half + 1 - offset);
    w2 *= (n_half * 3 + offset) / double(n_half * 3 + 1 - offset);
  }

  /* Normalize the weights. */
  threading::parallel_for(dst.index_range(), 256, [&](const IndexRange range) {
    for (const int64_t index : range) {
      if (!is_end_and_fixed(index)) {
        total_weight[index] += w - w2;
        dst[index] = src[index] + influence * dst[index] / total_weight[index];
      }
    }
  });
}

void gaussian_blur_1D(const GSpan src,
                      const int64_t iterations,
                      const float influence,
                      const bool smooth_ends,
                      const bool keep_shape,
                      const bool is_cyclic,
                      GMutableSpan dst)
{
  bke::attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
    using T = decltype(dummy);
    /* Reduces unnecessary code generation. */
    if constexpr (std::is_same_v<T, float> || std::is_same_v<T, float2> ||
                  std::is_same_v<T, float3>) {
      gaussian_blur_1D(src.typed<T>(),
                       iterations,
                       influence,
                       smooth_ends,
                       keep_shape,
                       is_cyclic,
                       dst.typed<T>());
    }
  });
}

static void smooth_curve_attribute(const OffsetIndices<int> points_by_curve,
                                   const VArray<bool> &selection,
                                   const VArray<bool> &cyclic,
                                   const int64_t iterations,
                                   const float influence,
                                   const bool smooth_ends,
                                   const bool keep_shape,
                                   GMutableSpan data)
{
  threading::parallel_for(points_by_curve.index_range(), 512, [&](const IndexRange range) {
    Vector<std::byte> orig_data;
    for (const int curve_i : range) {
      const IndexRange points = points_by_curve[curve_i];
      IndexMaskMemory memory;
      const IndexMask selection_mask = IndexMask::from_bools(points, selection, memory);
      if (selection_mask.is_empty()) {
        continue;
      }

      Vector<IndexRange> selection_ranges = selection_mask.to_ranges();
      for (const IndexRange range : selection_ranges) {
        GMutableSpan dst_data = data.slice(range);

        orig_data.resize(dst_data.size_in_bytes());
        dst_data.type().copy_assign_n(dst_data.data(), orig_data.data(), range.size());
        const GSpan src_data(dst_data.type(), orig_data.data(), range.size());

        gaussian_blur_1D(
            src_data, iterations, influence, smooth_ends, keep_shape, cyclic[curve_i], dst_data);
      }
    }
  });
}

static int grease_pencil_stroke_smooth_exec(bContext *C, wmOperator *op)
{
  using namespace blender;
  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  const int iterations = RNA_int_get(op->ptr, "iterations");
  const float influence = RNA_float_get(op->ptr, "factor");
  const bool keep_shape = RNA_boolean_get(op->ptr, "keep_shape");
  const bool smooth_ends = RNA_boolean_get(op->ptr, "smooth_ends");

  const bool smooth_position = RNA_boolean_get(op->ptr, "smooth_position");
  const bool smooth_radius = RNA_boolean_get(op->ptr, "smooth_radius");
  const bool smooth_opacity = RNA_boolean_get(op->ptr, "smooth_opacity");

  if (!(smooth_position || smooth_radius || smooth_opacity)) {
    /* There's nothing to be smoothed, return. */
    return OPERATOR_FINISHED;
  }

  bool changed = false;
  const Array<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
    if (curves.points_num() == 0) {
      return;
    }

    bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
    const OffsetIndices points_by_curve = curves.points_by_curve();
    const VArray<bool> cyclic = curves.cyclic();
    const VArray<bool> selection = *curves.attributes().lookup_or_default<bool>(
        ".selection", ATTR_DOMAIN_POINT, true);

    if (smooth_position) {
      bke::GSpanAttributeWriter positions = attributes.lookup_for_write_span("position");
      smooth_curve_attribute(points_by_curve,
                             selection,
                             cyclic,
                             iterations,
                             influence,
                             smooth_ends,
                             keep_shape,
                             positions.span);
      positions.finish();
      changed = true;
    }
    if (smooth_opacity && info.drawing.opacities().is_span()) {
      bke::GSpanAttributeWriter opacities = attributes.lookup_for_write_span("opacity");
      smooth_curve_attribute(points_by_curve,
                             selection,
                             cyclic,
                             iterations,
                             influence,
                             smooth_ends,
                             false,
                             opacities.span);
      opacities.finish();
      changed = true;
    }
    if (smooth_radius && info.drawing.radii().is_span()) {
      bke::GSpanAttributeWriter radii = attributes.lookup_for_write_span("radius");
      smooth_curve_attribute(points_by_curve,
                             selection,
                             cyclic,
                             iterations,
                             influence,
                             smooth_ends,
                             false,
                             radii.span);
      radii.finish();
      changed = true;
    }
  });

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_stroke_smooth(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "Smooth Stroke";
  ot->idname = "GREASE_PENCIL_OT_stroke_smooth";
  ot->description = "Smooth selected strokes";

  /* Callbacks. */
  ot->exec = grease_pencil_stroke_smooth_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Smooth parameters. */
  prop = RNA_def_int(ot->srna, "iterations", 10, 1, 100, "Iterations", "", 1, 30);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  RNA_def_float(ot->srna, "factor", 1.0f, 0.0f, 1.0f, "Factor", "", 0.0f, 1.0f);
  RNA_def_boolean(ot->srna, "smooth_ends", false, "Smooth Endpoints", "");
  RNA_def_boolean(ot->srna, "keep_shape", false, "Keep Shape", "");

  RNA_def_boolean(ot->srna, "smooth_position", true, "Position", "");
  RNA_def_boolean(ot->srna, "smooth_radius", true, "Radius", "");
  RNA_def_boolean(ot->srna, "smooth_opacity", false, "Opacity", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Simplify Stroke Operator
 * \{ */

static float dist_to_interpolated(
    float3 pos, float3 posA, float3 posB, float val, float valA, float valB)
{
  float dist1 = math::distance_squared(posA, pos);
  float dist2 = math::distance_squared(posB, pos);

  if (dist1 + dist2 > 0) {
    float interpolated_val = interpf(valB, valA, dist1 / (dist1 + dist2));
    return math::distance(interpolated_val, val);
  }
  return 0.0f;
}

static int64_t stroke_simplify(const IndexRange points,
                               const bool cyclic,
                               const float epsilon,
                               const FunctionRef<float(int64_t, int64_t, int64_t)> dist_function,
                               MutableSpan<bool> points_to_delete)
{
  int64_t total_points_to_delete = 0;
  const Span<bool> curve_selection = points_to_delete.slice(points);
  if (!curve_selection.contains(true)) {
    return total_points_to_delete;
  }

  const bool is_last_segment_selected = (curve_selection.first() && curve_selection.last());

  const Vector<IndexRange> selection_ranges = array_utils::find_all_ranges(curve_selection, true);
  threading::parallel_for(
      selection_ranges.index_range(), 1024, [&](const IndexRange range_of_ranges) {
        for (const IndexRange range : selection_ranges.as_span().slice(range_of_ranges)) {
          total_points_to_delete += ramer_douglas_peucker_simplify(
              range.shift(points.start()), epsilon, dist_function, points_to_delete);
        }
      });

  /* For cyclic curves, simplify the last segment. */
  if (cyclic && points.size() > 2 && is_last_segment_selected) {
    const float dist = dist_function(points.last(1), points.first(), points.last());
    if (dist <= epsilon) {
      points_to_delete[points.last()] = true;
      total_points_to_delete++;
    }
  }

  return total_points_to_delete;
}

static int grease_pencil_stroke_simplify_exec(bContext *C, wmOperator *op)
{
  using namespace blender;
  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  const float epsilon = RNA_float_get(op->ptr, "factor");

  bool changed = false;
  const Array<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
    if (curves.points_num() == 0) {
      return;
    }
    if (!ed::curves::has_anything_selected(curves)) {
      return;
    }

    const Span<float3> positions = curves.positions();
    const VArray<float> radii = info.drawing.radii();

    /* Distance functions for `ramer_douglas_peucker_simplify`. */
    const auto dist_function_positions =
        [positions](int64_t first_index, int64_t last_index, int64_t index) {
          const float dist_position = dist_to_line_v3(
              positions[index], positions[first_index], positions[last_index]);
          return dist_position;
        };
    const auto dist_function_positions_and_radii =
        [positions, radii](int64_t first_index, int64_t last_index, int64_t index) {
          const float dist_position = dist_to_line_v3(
              positions[index], positions[first_index], positions[last_index]);
          /* We divide the distance by 2000.0f to convert from "pixels" to an actual
           * distance. For some reason, grease pencil strokes the thickness of strokes in
           * pixels rather than object space distance. */
          const float dist_radii = dist_to_interpolated(positions[index],
                                                        positions[first_index],
                                                        positions[last_index],
                                                        radii[index],
                                                        radii[first_index],
                                                        radii[last_index]) /
                                   2000.0f;
          return math::max(dist_position, dist_radii);
        };

    const VArray<bool> cyclic = curves.cyclic();
    const OffsetIndices<int> points_by_curve = curves.points_by_curve();
    const VArray<bool> selection = *curves.attributes().lookup_or_default<bool>(
        ".selection", ATTR_DOMAIN_POINT, true);

    Array<bool> points_to_delete(curves.points_num());
    selection.materialize(points_to_delete);

    std::atomic<int64_t> total_points_to_delete = 0;
    if (radii.is_single()) {
      threading::parallel_for(curves.curves_range(), 128, [&](const IndexRange range) {
        for (const int curve_i : range) {
          const IndexRange points = points_by_curve[curve_i];
          total_points_to_delete += stroke_simplify(points,
                                                    cyclic[curve_i],
                                                    epsilon,
                                                    dist_function_positions,
                                                    points_to_delete.as_mutable_span());
        }
      });
    }
    else if (radii.is_span()) {
      threading::parallel_for(curves.curves_range(), 128, [&](const IndexRange range) {
        for (const int curve_i : range) {
          const IndexRange points = points_by_curve[curve_i];
          total_points_to_delete += stroke_simplify(points,
                                                    cyclic[curve_i],
                                                    epsilon,
                                                    dist_function_positions_and_radii,
                                                    points_to_delete.as_mutable_span());
        }
      });
    }

    if (total_points_to_delete > 0) {
      IndexMaskMemory memory;
      curves.remove_points(IndexMask::from_bools(points_to_delete, memory));
      info.drawing.tag_topology_changed();
      changed = true;
    }
  });

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }
  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_stroke_simplify(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "Simplify Stroke";
  ot->idname = "GREASE_PENCIL_OT_stroke_simplify";
  ot->description = "Simplify selected strokes";

  /* Callbacks. */
  ot->exec = grease_pencil_stroke_simplify_exec;
  ot->poll = editable_grease_pencil_point_selection_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Simplify parameters. */
  prop = RNA_def_float(ot->srna, "factor", 0.01f, 0.0f, 100.0f, "Factor", "", 0.0f, 100.0f);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dissolve Points Operator
 * \{ */

enum class DissolveMode : int8_t {
  /* Dissolve all selected points. */
  POINTS,
  /* Dissolve between selected points. */
  BETWEEN,
  /* Dissolve unselected points. */
  UNSELECT,
};

static const EnumPropertyItem prop_dissolve_types[] = {
    {int(DissolveMode::POINTS), "POINTS", 0, "Dissolve", "Dissolve selected points"},
    {int(DissolveMode::BETWEEN),
     "BETWEEN",
     0,
     "Dissolve Between",
     "Dissolve points between selected points"},
    {int(DissolveMode::UNSELECT),
     "UNSELECT",
     0,
     "Dissolve Unselect",
     "Dissolve all unselected points"},
    {0, nullptr, 0, nullptr, nullptr},
};

static Array<bool> get_points_to_dissolve(bke::CurvesGeometry &curves, const DissolveMode mode)
{
  const VArray<bool> selection = *curves.attributes().lookup_or_default<bool>(
      ".selection", ATTR_DOMAIN_POINT, true);

  Array<bool> points_to_dissolve(curves.points_num());
  selection.materialize(points_to_dissolve);

  if (mode == DissolveMode::POINTS) {
    return points_to_dissolve;
  }

  /* Both `between` and `unselect` have the unselected point being the ones dissolved so we need
   * to invert. */
  BLI_assert(ELEM(mode, DissolveMode::BETWEEN, DissolveMode::UNSELECT));

  const OffsetIndices<int> points_by_curve = curves.points_by_curve();
  /* Because we are going to invert, these become the points to keep. */
  MutableSpan<bool> points_to_keep = points_to_dissolve.as_mutable_span();

  threading::parallel_for(curves.curves_range(), 128, [&](const IndexRange range) {
    for (const int64_t curve_i : range) {
      const IndexRange points = points_by_curve[curve_i];
      const Span<bool> curve_selection = points_to_dissolve.as_span().slice(points);
      /* The unselected curves should not be dissolved. */
      if (!curve_selection.contains(true)) {
        points_to_keep.slice(points).fill(true);
        continue;
      }

      /* `between` is just `unselect` but with the first and last segments not getting
       * dissolved. */
      if (mode != DissolveMode::BETWEEN) {
        continue;
      }

      const Vector<IndexRange> deselection_ranges = array_utils::find_all_ranges(curve_selection,
                                                                                 false);

      if (deselection_ranges.size() != 0) {
        const IndexRange first_range = deselection_ranges.first().shift(points.first());
        const IndexRange last_range = deselection_ranges.last().shift(points.first());

        /* Ranges should only be fill if the first/last point matches the start/end point
         * of the segment. */
        if (first_range.first() == points.first()) {
          points_to_keep.slice(first_range).fill(true);
        }
        if (last_range.last() == points.last()) {
          points_to_keep.slice(last_range).fill(true);
        }
      }
    }
  });

  array_utils::invert_booleans(points_to_dissolve);

  return points_to_dissolve;
}

static int grease_pencil_dissolve_exec(bContext *C, wmOperator *op)
{
  using namespace blender;
  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  const DissolveMode mode = DissolveMode(RNA_enum_get(op->ptr, "type"));

  bool changed = false;
  const Array<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
    if (curves.points_num() == 0) {
      return;
    }
    if (!ed::curves::has_anything_selected(curves)) {
      return;
    }

    const Array<bool> points_to_dissolve = get_points_to_dissolve(curves, mode);

    if (points_to_dissolve.as_span().contains(true)) {
      IndexMaskMemory memory;
      curves.remove_points(IndexMask::from_bools(points_to_dissolve, memory));
      info.drawing.tag_topology_changed();
      changed = true;
    }
  });

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }
  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_dissolve(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "Dissolve";
  ot->idname = "GREASE_PENCIL_OT_dissolve";
  ot->description = "Delete selected points without splitting strokes";

  /* Callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = grease_pencil_dissolve_exec;
  ot->poll = editable_grease_pencil_point_selection_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Simplify parameters. */
  ot->prop = prop = RNA_def_enum(ot->srna,
                                 "type",
                                 prop_dissolve_types,
                                 0,
                                 "Type",
                                 "Method used for dissolving stroke points");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Delete Frame Operator
 * \{ */

enum class DeleteFrameMode : int8_t {
  /* Delete the active frame for the current layer. */
  ACTIVE_FRAME,
  /* Delete the active frames for all layers. */
  ALL_FRAMES,
};

static const EnumPropertyItem prop_greasepencil_deleteframe_types[] = {
    {int(DeleteFrameMode::ACTIVE_FRAME),
     "ACTIVE_FRAME",
     0,
     "Active Frame",
     "Deletes current frame in the active layer"},
    {int(DeleteFrameMode::ALL_FRAMES),
     "ALL_FRAMES",
     0,
     "All Active Frames",
     "Delete active frames for all layers"},
    {0, nullptr, 0, nullptr, nullptr},
};

static int grease_pencil_delete_frame_exec(bContext *C, wmOperator *op)
{
  using namespace blender;
  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  const int current_frame = scene->r.cfra;

  const DeleteFrameMode mode = DeleteFrameMode(RNA_enum_get(op->ptr, "type"));

  bool changed = false;
  if (mode == DeleteFrameMode::ACTIVE_FRAME && grease_pencil.has_active_layer()) {
    bke::greasepencil::Layer &layer = *grease_pencil.get_active_layer_for_write();
    if (layer.is_editable()) {
      changed |= grease_pencil.remove_frames(layer, {layer.frame_key_at(current_frame)});
    }
  }
  else if (mode == DeleteFrameMode::ALL_FRAMES) {
    for (bke::greasepencil::Layer *layer : grease_pencil.layers_for_write()) {
      if (layer->is_editable()) {
        changed |= grease_pencil.remove_frames(*layer, {layer->frame_key_at(current_frame)});
      }
    }
  }

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA | NA_EDITED, &grease_pencil);
    WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_delete_frame(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "Delete Frame";
  ot->idname = "GREASE_PENCIL_OT_delete_frame";
  ot->description = "Delete Grease Pencil Frame(s)";

  /* Callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = grease_pencil_delete_frame_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = prop = RNA_def_enum(ot->srna,
                                 "type",
                                 prop_greasepencil_deleteframe_types,
                                 0,
                                 "Type",
                                 "Method used for deleting Grease Pencil frames");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Material Set Operator
 * \{ */

static int grease_pencil_stroke_material_set_exec(bContext *C, wmOperator * /*op*/)
{
  using namespace blender;
  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  const int material_index = object->actcol - 1;

  if (material_index == -1) {
    return OPERATOR_CANCELLED;
  }

  const Array<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();

    IndexMaskMemory memory;
    IndexMask selected_curves = ed::curves::retrieve_selected_curves(curves, memory);

    if (selected_curves.is_empty()) {
      return;
    }

    bke::SpanAttributeWriter<int> materials =
        curves.attributes_for_write().lookup_or_add_for_write_span<int>("material_index",
                                                                        ATTR_DOMAIN_CURVE);
    selected_curves.foreach_index(
        [&](const int curve_index) { materials.span[curve_index] = material_index; });

    materials.finish();
  });

  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA | NA_EDITED, &grease_pencil);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_stroke_material_set(wmOperatorType *ot)
{
  ot->name = "Assign Material";
  ot->idname = "GREASE_PENCIL_OT_stroke_material_set";
  ot->description = "Change Stroke material with selected material";

  ot->exec = grease_pencil_stroke_material_set_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Cyclical Set Operator
 * \{ */

enum class CyclicalMode : int8_t {
  /* Sets all strokes to cycle. */
  CLOSE,
  /* Sets all strokes to not cycle. */
  OPEN,
  /* Switches the cyclic state of the strokes. */
  TOGGLE,
};

static const EnumPropertyItem prop_cyclical_types[] = {
    {int(CyclicalMode::CLOSE), "CLOSE", 0, "Close All", ""},
    {int(CyclicalMode::OPEN), "OPEN", 0, "Open All", ""},
    {int(CyclicalMode::TOGGLE), "TOGGLE", 0, "Toggle", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static int grease_pencil_cyclical_set_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  const CyclicalMode mode = CyclicalMode(RNA_enum_get(op->ptr, "type"));

  bool changed = false;
  const Array<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();

    if (mode == CyclicalMode::OPEN && !curves.attributes().contains("cyclic")) {
      /* Avoid creating unneeded attribute. */
      return;
    }

    IndexMaskMemory memory;
    const IndexMask curve_selection = ed::curves::retrieve_selected_curves(curves, memory);
    if (curve_selection.is_empty()) {
      return;
    }

    MutableSpan<bool> cyclic = curves.cyclic_for_write();

    switch (mode) {
      case CyclicalMode::CLOSE:
        index_mask::masked_fill(cyclic, true, curve_selection);
        break;
      case CyclicalMode::OPEN:
        index_mask::masked_fill(cyclic, false, curve_selection);
        break;
      case CyclicalMode::TOGGLE:
        array_utils::invert_booleans(cyclic, curve_selection);
        break;
    }

    /* Remove the attribute if it is empty. */
    if (mode != CyclicalMode::CLOSE) {
      if (array_utils::booleans_mix_calc(curves.cyclic()) == array_utils::BooleanMix::AllFalse) {
        curves.attributes_for_write().remove("cyclic");
      }
    }

    changed = true;
  });

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_cyclical_set(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Set Cyclical State";
  ot->idname = "GREASE_PENCIL_OT_cyclical_set";
  ot->description = "Close or open the selected stroke adding a segment from last to first point";

  /* Callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = grease_pencil_cyclical_set_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Simplify parameters. */
  ot->prop = RNA_def_enum(
      ot->srna, "type", prop_cyclical_types, int(CyclicalMode::TOGGLE), "Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set selected material as active material
 * \{ */
static int grease_pencil_set_active_material_exec(bContext *C, wmOperator * /*op*/)
{
  using namespace blender;
  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  if (object->totcol == 0) {
    return OPERATOR_CANCELLED;
  }

  const Array<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  for (const MutableDrawingInfo &info : drawings) {
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();

    IndexMaskMemory memory;
    IndexMask selected_curves = ed::curves::retrieve_selected_curves(curves, memory);
    if (selected_curves.is_empty()) {
      continue;
    }

    const blender::VArray<int> materials = *curves.attributes().lookup_or_default<int>(
        "material_index", ATTR_DOMAIN_CURVE, 0);
    object->actcol = materials[selected_curves.first()] + 1;
    break;
  };

  WM_event_add_notifier(C, NC_GEOM | ND_DATA | NA_EDITED, &grease_pencil);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_set_active_material(wmOperatorType *ot)
{
  ot->name = "Set Active Material";
  ot->idname = "GREASE_PENCIL_OT_set_active_material";
  ot->description = "Set the selected stroke material as the active material";

  ot->exec = grease_pencil_set_active_material_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
/** \} */

}  // namespace blender::ed::greasepencil

void ED_operatortypes_grease_pencil_edit()
{
  using namespace blender::ed::greasepencil;
  WM_operatortype_append(GREASE_PENCIL_OT_stroke_smooth);
  WM_operatortype_append(GREASE_PENCIL_OT_stroke_simplify);
  WM_operatortype_append(GREASE_PENCIL_OT_dissolve);
  WM_operatortype_append(GREASE_PENCIL_OT_delete_frame);
  WM_operatortype_append(GREASE_PENCIL_OT_stroke_material_set);
  WM_operatortype_append(GREASE_PENCIL_OT_cyclical_set);
  WM_operatortype_append(GREASE_PENCIL_OT_set_active_material);
}

void ED_keymap_grease_pencil(wmKeyConfig *keyconf)
{
  using namespace blender::ed::greasepencil;
  keymap_grease_pencil_editing(keyconf);
  keymap_grease_pencil_painting(keyconf);
}
