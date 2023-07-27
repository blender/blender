/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "BLI_array_utils.hh"
#include "BLI_index_mask.hh"
#include "BLI_index_range.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_stack.hh"

#include "BKE_context.h"
#include "BKE_grease_pencil.hh"

#include "RNA_access.h"
#include "RNA_define.h"

#include "DEG_depsgraph.h"

#include "ED_curves.hh"
#include "ED_grease_pencil.h"
#include "ED_screen.h"

#include "WM_api.h"

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
  wmKeyMap *keymap = WM_keymap_ensure(keyconf, "Grease Pencil Edit Mode", 0, 0);
  keymap->poll = editable_grease_pencil_poll;
}

static void keymap_grease_pencil_painting(wmKeyConfig *keyconf)
{
  wmKeyMap *keymap = WM_keymap_ensure(keyconf, "Grease Pencil Paint Mode", 0, 0);
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
    if constexpr (std::is_same_v<T, float> || std::is_same_v<T, float3>) {
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

  grease_pencil.foreach_editable_drawing(
      scene->r.cfra, [&](int /*drawing_index*/, bke::greasepencil::Drawing &drawing) {
        bke::CurvesGeometry &curves = drawing.strokes_for_write();
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
        }
        if (smooth_opacity && drawing.opacities().is_span()) {
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
        }
        if (smooth_radius && drawing.radii().is_span()) {
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
        }
      });

  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);

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

/**
 * An implementation of the Ramer-Douglas-Peucker algorithm.
 *
 * \param range: The range to simplify.
 * \param epsilon: The threshold distance from the coord between two points for when a point
 * in-between needs to be kept.
 * \param dist_function: A function that computes the distance to a point at an index in the range.
 * The IndexRange is a subrange of \a range and the index is an index relative to the subrange.
 * \param points_to_delete: Writes true to the indecies for which the points should be removed.
 * \returns the total number of points to remove.
 */
int64_t ramer_douglas_peucker_simplify(const IndexRange range,
                                       const float epsilon,
                                       const FunctionRef<float(IndexRange, int64_t)> dist_function,
                                       MutableSpan<bool> points_to_delete)
{
  /* Mark all points to not be removed. */
  points_to_delete.slice(range).fill(false);
  int64_t total_points_to_remove = 0;

  Stack<IndexRange> stack;
  stack.push(range);
  while (!stack.is_empty()) {
    const IndexRange sub_range = stack.pop();

    /* Compute the maximum distance and the corresponding distance. */
    float max_dist = -1.0f;
    int max_index = -1;
    for (const int64_t sub_index : sub_range.index_range().drop_front(1).drop_back(1)) {
      const float dist = dist_function(sub_range, sub_index);
      if (dist > max_dist) {
        max_dist = dist;
        max_index = sub_index;
      }
    }

    if (max_dist > epsilon) {
      /* Found point outside the epsilon-sized strip. Repeat the search on the left & right side.
       */
      stack.push(sub_range.slice(IndexRange(max_index + 1)));
      stack.push(sub_range.slice(IndexRange(max_index, sub_range.size() - max_index)));
    }
    else {
      /* Points in `sub_range` are inside the epsilon-sized strip. Mark them to be deleted. */
      const IndexRange inside_range = sub_range.drop_front(1).drop_back(1);
      total_points_to_remove += inside_range.size();
      points_to_delete.slice(inside_range).fill(true);
    }
  }
  return total_points_to_remove;
}

static int grease_pencil_stroke_simplify_exec(bContext *C, wmOperator *op)
{
  using namespace blender;
  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  const float epsilon = RNA_float_get(op->ptr, "factor");

  bool changed = false;
  grease_pencil.foreach_editable_drawing(
      scene->r.cfra, [&](int /*drawing_index*/, bke::greasepencil::Drawing &drawing) {
        bke::CurvesGeometry &curves = drawing.strokes_for_write();
        if (curves.points_num() == 0) {
          return;
        }

        if (!ed::curves::has_anything_selected(curves)) {
          return;
        }

        const Span<float3> positions = curves.positions();

        /* Distance function for `ramer_douglas_peucker_simplify`. */
        auto dist_func = [&](IndexRange range, int64_t index_in_range) {
          const Span<float3> position_slice = positions.slice(range);
          const float dist_position = dist_to_line_v3(
              position_slice[index_in_range], position_slice.first(), position_slice.last());
          return dist_position;
        };

        const VArray<bool> cyclic = curves.cyclic();
        const OffsetIndices<int> points_by_curve = curves.points_by_curve();
        const VArray<bool> selection = *curves.attributes().lookup_or_default<bool>(
            ".selection", ATTR_DOMAIN_POINT, true);

        Array<bool> points_to_delete(curves.points_num());
        selection.materialize(points_to_delete);

        std::atomic<int64_t> total_points_to_delete = 0;
        threading::parallel_for(curves.curves_range(), 128, [&](const IndexRange range) {
          for (const int curve_i : range) {
            const IndexRange points = points_by_curve[curve_i];
            const Span<bool> curve_selection = points_to_delete.as_span().slice(points);
            if (!curve_selection.contains(true)) {
              continue;
            }

            const bool is_last_segment_selected = (curve_selection.first() &&
                                                   curve_selection.last());

            const Vector<IndexRange> selection_ranges = array_utils::find_all_ranges(
                curve_selection, true);
            threading::parallel_for(
                selection_ranges.index_range(), 1024, [&](const IndexRange range_of_ranges) {
                  for (const IndexRange range : selection_ranges.as_span().slice(range_of_ranges))
                  {
                    total_points_to_delete += ramer_douglas_peucker_simplify(
                        range.shift(points.start()),
                        epsilon,
                        dist_func,
                        points_to_delete.as_mutable_span());
                  }
                });

            /* For cyclic curves, simplify the last segment. */
            if (cyclic[curve_i] && curves.points_num() > 2 && is_last_segment_selected) {
              const float dist = dist_to_line_v3(
                  positions[points.last()], positions[points.last(1)], positions[points.first()]);
              if (dist <= epsilon) {
                points_to_delete[points.last()] = true;
                total_points_to_delete++;
              }
            }
          }
        });

        if (total_points_to_delete > 0) {
          IndexMaskMemory memory;
          curves.remove_points(IndexMask::from_bools(points_to_delete, memory));
          drawing.tag_topology_changed();
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
  prop = RNA_def_float(ot->srna, "factor", 0.001f, 0.0f, 100.0f, "Factor", "", 0.0f, 100.0f);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

}  // namespace blender::ed::greasepencil

void ED_operatortypes_grease_pencil_edit()
{
  using namespace blender::ed::greasepencil;
  WM_operatortype_append(GREASE_PENCIL_OT_stroke_smooth);
  WM_operatortype_append(GREASE_PENCIL_OT_stroke_simplify);
}

void ED_keymap_grease_pencil(wmKeyConfig *keyconf)
{
  using namespace blender::ed::greasepencil;
  keymap_grease_pencil_editing(keyconf);
  keymap_grease_pencil_painting(keyconf);
}
