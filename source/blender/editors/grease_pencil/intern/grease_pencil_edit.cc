/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "BLI_array.hh"
#include "BLI_array_utils.hh"
#include "BLI_assert.h"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_index_mask.hh"
#include "BLI_index_range.hh"
#include "BLI_listbase.h"
#include "BLI_math_base.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_offset_indices.hh"
#include "BLI_span.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"
#include "BLT_translation.hh"

#include "DNA_anim_types.h"
#include "DNA_array_utils.hh"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_anim_data.hh"
#include "BKE_animsys.h"
#include "BKE_attribute.hh"
#include "BKE_attribute_legacy_convert.hh"
#include "BKE_context.hh"
#include "BKE_curves_utils.hh"
#include "BKE_customdata.hh"
#include "BKE_deform.hh"
#include "BKE_fcurve_driver.h"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_material.hh"
#include "BKE_preview_image.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "ED_curves.hh"
#include "ED_grease_pencil.hh"
#include "ED_object.hh"
#include "ED_transform_snap_object_context.hh"
#include "ED_view3d.hh"

#include "GEO_curves_remove_and_split.hh"
#include "GEO_fit_curves.hh"
#include "GEO_join_geometries.hh"
#include "GEO_realize_instances.hh"
#include "GEO_reorder.hh"
#include "GEO_resample_curves.hh"
#include "GEO_set_curve_type.hh"
#include "GEO_simplify_curves.hh"
#include "GEO_smooth_curves.hh"
#include "GEO_subdivide_curves.hh"

#include "UI_interface_c.hh"
#include "UI_interface_layout.hh"

#include "UI_resources.hh"
#include <limits>

namespace blender::ed::greasepencil {

/* -------------------------------------------------------------------- */
/** \name Smooth Stroke Operator
 * \{ */

static wmOperatorStatus grease_pencil_stroke_smooth_exec(bContext *C, wmOperator *op)
{
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
  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
    if (curves.is_empty()) {
      return;
    }

    IndexMaskMemory memory;
    const IndexMask strokes = ed::greasepencil::retrieve_editable_and_selected_strokes(
        *object, info.drawing, info.layer_index, memory);
    if (strokes.is_empty()) {
      return;
    }

    bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
    const OffsetIndices points_by_curve = curves.points_by_curve();
    const VArray<bool> cyclic = curves.cyclic();
    const VArray<bool> point_selection = *curves.attributes().lookup_or_default<bool>(
        ".selection", bke::AttrDomain::Point, true);

    if (smooth_position) {
      bke::GSpanAttributeWriter positions = attributes.lookup_for_write_span("position");
      geometry::smooth_curve_attribute(strokes,
                                       points_by_curve,
                                       point_selection,
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
      geometry::smooth_curve_attribute(strokes,
                                       points_by_curve,
                                       point_selection,
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
      geometry::smooth_curve_attribute(strokes,
                                       points_by_curve,
                                       point_selection,
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

  ot->name = "Smooth Stroke";
  ot->idname = "GREASE_PENCIL_OT_stroke_smooth";
  ot->description = "Smooth selected strokes";

  ot->exec = grease_pencil_stroke_smooth_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

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

enum class SimplifyMode {
  FIXED = 0,
  ADAPTIVE = 1,
  SAMPLE = 2,
  MERGE = 3,
};

static const EnumPropertyItem prop_simplify_modes[] = {
    {int(SimplifyMode::FIXED),
     "FIXED",
     0,
     "Fixed",
     "Delete alternating vertices in the stroke, except extremes"},
    {int(SimplifyMode::ADAPTIVE),
     "ADAPTIVE",
     0,
     "Adaptive",
     "Use a Ramer-Douglas-Peucker algorithm to simplify the stroke preserving main shape"},
    {int(SimplifyMode::SAMPLE),
     "SAMPLE",
     0,
     "Sample",
     "Re-sample the stroke with segments of the specified length"},
    {int(SimplifyMode::MERGE),
     "MERGE",
     0,
     "Merge",
     "Simplify the stroke by merging vertices closer than a given distance"},
    {0, nullptr, 0, nullptr, nullptr},
};

static IndexMask simplify_fixed(const bke::CurvesGeometry &curves,
                                const int step,
                                const IndexMask &stroke_selection,
                                IndexMaskMemory &memory)
{
  const OffsetIndices points_by_curve = curves.points_by_curve();
  const Array<int> point_to_curve_map = curves.point_to_curve_map();

  const IndexMask selected_points = IndexMask::from_ranges(
      points_by_curve, stroke_selection, memory);

  /* Find points to keep among selected points. */
  const IndexMask selected_to_keep = IndexMask::from_predicate(
      selected_points, GrainSize(2048), memory, [&](const int64_t i) {
        const int curve_i = point_to_curve_map[i];
        const IndexRange points = points_by_curve[curve_i];
        if (points.size() <= 2) {
          return true;
        }
        const int local_i = i - points.start();
        return (local_i % int(math::pow(2.0f, float(step))) == 0) || points.last() == i;
      });

  /* All the points that are not selected are also kept. */
  return IndexMask::from_union(
      {selected_to_keep, selected_points.complement(curves.points_range(), memory)}, memory);
}

static wmOperatorStatus grease_pencil_stroke_simplify_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  const SimplifyMode mode = SimplifyMode(RNA_enum_get(op->ptr, "mode"));

  bool changed = false;
  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
    if (curves.is_empty()) {
      return;
    }

    IndexMaskMemory memory;
    const IndexMask strokes = ed::greasepencil::retrieve_editable_and_selected_strokes(
        *object, info.drawing, info.layer_index, memory);
    if (strokes.is_empty()) {
      return;
    }

    switch (mode) {
      case SimplifyMode::FIXED: {
        const int steps = RNA_int_get(op->ptr, "steps");
        const IndexMask points_to_keep = simplify_fixed(curves, steps, strokes, memory);
        if (points_to_keep.is_empty()) {
          info.drawing.strokes_for_write() = {};
          break;
        }
        if (points_to_keep.size() == curves.points_num()) {
          break;
        }
        info.drawing.strokes_for_write() = bke::curves_copy_point_selection(
            curves, points_to_keep, {});
        info.drawing.tag_topology_changed();
        changed = true;
        break;
      }
      case SimplifyMode::ADAPTIVE: {
        const float simplify_factor = RNA_float_get(op->ptr, "factor");
        const IndexMask points_to_delete = geometry::simplify_curve_attribute(
            curves.positions(),
            strokes,
            curves.points_by_curve(),
            curves.cyclic(),
            simplify_factor,
            curves.positions(),
            memory);
        info.drawing.strokes_for_write().remove_points(points_to_delete, {});
        info.drawing.tag_topology_changed();
        changed = true;
        break;
      }
      case SimplifyMode::SAMPLE: {
        const float resample_length = RNA_float_get(op->ptr, "length");
        info.drawing.strokes_for_write() = geometry::resample_to_length(
            curves, strokes, VArray<float>::from_single(resample_length, curves.curves_num()), {});
        info.drawing.tag_topology_changed();
        changed = true;
        break;
      }
      case SimplifyMode::MERGE: {
        const OffsetIndices<int> points_by_curve = curves.points_by_curve();
        const Array<int> point_to_curve_map = curves.point_to_curve_map();
        const float merge_distance = RNA_float_get(op->ptr, "distance");
        const IndexMask selected_points = IndexMask::from_ranges(points_by_curve, strokes, memory);
        const IndexMask filtered_points = IndexMask::from_predicate(
            selected_points, GrainSize(2048), memory, [&](const int64_t i) {
              const int curve_i = point_to_curve_map[i];
              const IndexRange points = points_by_curve[curve_i];
              if (points.drop_front(1).drop_back(1).contains(i)) {
                return true;
              }
              return false;
            });
        info.drawing.strokes_for_write() = ed::greasepencil::curves_merge_by_distance(
            curves, merge_distance, filtered_points, {});
        info.drawing.tag_topology_changed();
        changed = true;
        break;
      }
      default:
        break;
    }
  });

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }
  return OPERATOR_FINISHED;
}

static void grease_pencil_simplify_ui(bContext *C, wmOperator *op)
{
  uiLayout *layout = op->layout;
  wmWindowManager *wm = CTX_wm_manager(C);

  PointerRNA ptr = RNA_pointer_create_discrete(&wm->id, op->type->srna, op->properties);

  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);

  layout->prop(&ptr, "mode", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  const SimplifyMode mode = SimplifyMode(RNA_enum_get(op->ptr, "mode"));

  switch (mode) {
    case SimplifyMode::FIXED:
      layout->prop(&ptr, "steps", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      break;
    case SimplifyMode::ADAPTIVE:
      layout->prop(&ptr, "factor", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      break;
    case SimplifyMode::SAMPLE:
      layout->prop(&ptr, "length", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      break;
    case SimplifyMode::MERGE:
      layout->prop(&ptr, "distance", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      break;
    default:
      break;
  }
}

static void GREASE_PENCIL_OT_stroke_simplify(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Simplify Stroke";
  ot->idname = "GREASE_PENCIL_OT_stroke_simplify";
  ot->description = "Simplify selected strokes";

  ot->exec = grease_pencil_stroke_simplify_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->ui = grease_pencil_simplify_ui;

  prop = RNA_def_float(ot->srna, "factor", 0.01f, 0.0f, 100.0f, "Factor", "", 0.0f, 100.0f);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_float(ot->srna, "length", 0.05f, 0.01f, 100.0f, "Length", "", 0.01f, 1.0f);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_float(ot->srna, "distance", 0.01f, 0.0f, 100.0f, "Distance", "", 0.0f, 1.0f);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_int(ot->srna, "steps", 1, 0, 50, "Steps", "", 0.0f, 10);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_enum(ot->srna,
                      "mode",
                      prop_simplify_modes,
                      0,
                      "Mode",
                      "Method used for simplifying stroke points");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Delete Operator
 * \{ */

static wmOperatorStatus grease_pencil_delete_exec(bContext *C, wmOperator * /*op*/)
{
  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  View3D *v3d = CTX_wm_view3d(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  const bke::AttrDomain selection_domain = ED_grease_pencil_edit_selection_domain_get(
      scene->toolsettings);

  bool changed = false;
  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();

    IndexMaskMemory memory;
    if (selection_domain == bke::AttrDomain::Curve) {
      const IndexMask strokes = ed::greasepencil::retrieve_editable_and_selected_strokes(
          *object, info.drawing, info.layer_index, memory);
      if (strokes.is_empty()) {
        return;
      }
      curves.remove_curves(strokes, {});
    }
    else if (selection_domain == bke::AttrDomain::Point) {
      const IndexMask points = ed::greasepencil::retrieve_editable_and_all_selected_points(
          *object, info.drawing, info.layer_index, v3d->overlay.handle_display, memory);
      if (points.is_empty()) {
        return;
      }
      curves = geometry::remove_points_and_split(curves, points);
    }
    info.drawing.tag_topology_changed();
    changed = true;
  });

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }
  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_delete(wmOperatorType *ot)
{
  ot->name = "Delete";
  ot->idname = "GREASE_PENCIL_OT_delete";
  ot->description = "Delete selected strokes or points";

  ot->exec = grease_pencil_delete_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dissolve Points Operator
 * \{ */

enum class DissolveMode : int8_t {
  /** Dissolve all selected points. */
  POINTS = 0,
  /** Dissolve between selected points. */
  BETWEEN = 1,
  /** Dissolve unselected points. */
  UNSELECT = 2,
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

static Array<bool> get_points_to_dissolve(bke::CurvesGeometry &curves,
                                          const IndexMask &mask,
                                          const DissolveMode mode)
{
  Array<bool> points_to_dissolve(curves.points_num());
  mask.to_bools(points_to_dissolve);

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

static wmOperatorStatus grease_pencil_dissolve_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  View3D *v3d = CTX_wm_view3d(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  const DissolveMode mode = DissolveMode(RNA_enum_get(op->ptr, "type"));

  bool changed = false;
  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
    if (curves.is_empty()) {
      return;
    }

    IndexMaskMemory memory;
    const IndexMask points = ed::greasepencil::retrieve_editable_and_all_selected_points(
        *object, info.drawing, info.layer_index, v3d->overlay.handle_display, memory);
    if (points.is_empty()) {
      return;
    }

    const Array<bool> points_to_dissolve = get_points_to_dissolve(curves, points, mode);
    if (points_to_dissolve.as_span().contains(true)) {
      curves.remove_points(IndexMask::from_bools(points_to_dissolve, memory), {});
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

  ot->name = "Dissolve";
  ot->idname = "GREASE_PENCIL_OT_dissolve";
  ot->description = "Delete selected points without splitting strokes";

  ot->invoke = WM_menu_invoke;
  ot->exec = grease_pencil_dissolve_exec;
  ot->poll = editable_grease_pencil_point_selection_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = prop = RNA_def_enum(ot->srna,
                                 "type",
                                 prop_dissolve_types,
                                 0,
                                 "Type",
                                 "Method used for dissolving stroke points");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_GPENCIL);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Delete Frame Operator
 * \{ */

enum class DeleteFrameMode : int8_t {
  /** Delete the active frame for the current layer. */
  ACTIVE_FRAME = 0,
  /** Delete the active frames for all layers. */
  ALL_FRAMES = 1,
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

static wmOperatorStatus grease_pencil_delete_frame_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  const int current_frame = scene->r.cfra;

  const DeleteFrameMode mode = DeleteFrameMode(RNA_enum_get(op->ptr, "type"));

  bool changed = false;
  if (mode == DeleteFrameMode::ACTIVE_FRAME && grease_pencil.has_active_layer()) {
    bke::greasepencil::Layer &layer = *grease_pencil.get_active_layer();
    if (layer.is_editable() && layer.start_frame_at(current_frame)) {
      changed |= grease_pencil.remove_frames(layer, {*layer.start_frame_at(current_frame)});
    }
  }
  else if (mode == DeleteFrameMode::ALL_FRAMES) {
    for (bke::greasepencil::Layer *layer : grease_pencil.layers_for_write()) {
      if (layer->is_editable() && layer->start_frame_at(current_frame)) {
        changed |= grease_pencil.remove_frames(*layer, {*layer->start_frame_at(current_frame)});
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

  ot->name = "Delete Frame";
  ot->idname = "GREASE_PENCIL_OT_delete_frame";
  ot->description = "Delete Grease Pencil Frame(s)";

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

static wmOperatorStatus grease_pencil_stroke_material_set_exec(bContext *C, wmOperator *op)
{
  using namespace blender;
  Main *bmain = CTX_data_main(C);
  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  Material *ma = nullptr;
  char name[MAX_ID_NAME - 2];
  RNA_string_get(op->ptr, "material", name);

  int material_index = object->actcol - 1;

  if (name[0] != '\0') {
    ma = reinterpret_cast<Material *>(BKE_libblock_find_name(bmain, ID_MA, name));
    if (ma == nullptr) {
      BKE_reportf(op->reports, RPT_WARNING, TIP_("Material '%s' could not be found"), name);
      return OPERATOR_CANCELLED;
    }

    /* Find slot index. */
    material_index = BKE_object_material_index_get(object, ma);
  }

  if (material_index == -1) {
    return OPERATOR_CANCELLED;
  }

  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    IndexMaskMemory memory;
    IndexMask strokes = ed::greasepencil::retrieve_editable_and_selected_strokes(
        *object, info.drawing, info.layer_index, memory);
    if (strokes.is_empty()) {
      return;
    }

    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
    bke::SpanAttributeWriter<int> materials =
        curves.attributes_for_write().lookup_or_add_for_write_span<int>("material_index",
                                                                        bke::AttrDomain::Curve);
    index_mask::masked_fill(materials.span, material_index, strokes);
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
  ot->description = "Assign the active material slot to the selected strokes";

  ot->exec = grease_pencil_stroke_material_set_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_string(
      ot->srna, "material", nullptr, MAX_ID_NAME - 2, "Material", "Name of the material");
  RNA_def_property_flag(ot->prop, PROP_SKIP_SAVE);
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Cyclical Set Operator
 * \{ */

enum class CyclicalMode : int8_t {
  /** Sets all strokes to cycle. */
  CLOSE = 0,
  /** Sets all strokes to not cycle. */
  OPEN = 1,
  /** Switches the cyclic state of the strokes. */
  TOGGLE = 2,
};

static const EnumPropertyItem prop_cyclical_types[] = {
    {int(CyclicalMode::CLOSE), "CLOSE", 0, "Close All", ""},
    {int(CyclicalMode::OPEN), "OPEN", 0, "Open All", ""},
    {int(CyclicalMode::TOGGLE), "TOGGLE", 0, "Toggle", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static bke::CurvesGeometry subdivide_last_segement(const bke::CurvesGeometry &curves,
                                                   const IndexMask &strokes)
{
  const VArray<bool> cyclic = curves.cyclic();
  const Span<float3> positions = curves.positions();
  curves.ensure_evaluated_lengths();

  Array<int> use_cuts(curves.points_num(), 0);
  const OffsetIndices points_by_curve = curves.points_by_curve();

  strokes.foreach_index(GrainSize(4096), [&](const int curve_i) {
    if (cyclic[curve_i]) {
      const IndexRange points = points_by_curve[curve_i];
      const float end_distance = math::distance(positions[points.first()],
                                                positions[points.last()]);

      /* Because the curve is already cyclical the last segment has to be subtracted. */
      const float curve_length = curves.evaluated_length_total_for_curve(curve_i, true) -
                                 end_distance;

      /* Calculate cuts to match the average density. */
      const float point_density = float(points.size()) / curve_length;
      use_cuts[points.last()] = int(point_density * end_distance);
    }
  });

  const VArray<int> cuts = VArray<int>::from_span(use_cuts.as_span());

  return geometry::subdivide_curves(curves, strokes, cuts);
}

static wmOperatorStatus grease_pencil_cyclical_set_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  const CyclicalMode mode = CyclicalMode(RNA_enum_get(op->ptr, "type"));
  const bool subdivide_cyclic_segment = RNA_boolean_get(op->ptr, "subdivide_cyclic_segment");

  bool changed = false;
  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
    if (mode == CyclicalMode::OPEN && !curves.attributes().contains("cyclic")) {
      /* Avoid creating unneeded attribute. */
      return;
    }

    IndexMaskMemory memory;
    const IndexMask strokes = ed::greasepencil::retrieve_editable_and_selected_strokes(
        *object, info.drawing, info.layer_index, memory);
    if (strokes.is_empty()) {
      return;
    }

    MutableSpan<bool> cyclic = curves.cyclic_for_write();
    switch (mode) {
      case CyclicalMode::CLOSE:
        index_mask::masked_fill(cyclic, true, strokes);
        break;
      case CyclicalMode::OPEN:
        index_mask::masked_fill(cyclic, false, strokes);
        break;
      case CyclicalMode::TOGGLE:
        array_utils::invert_booleans(cyclic, strokes);
        break;
    }

    /* Remove the attribute if it is empty. */
    if (mode != CyclicalMode::CLOSE) {
      if (array_utils::booleans_mix_calc(curves.cyclic()) == array_utils::BooleanMix::AllFalse) {
        curves.attributes_for_write().remove("cyclic");
      }
    }

    if (subdivide_cyclic_segment) {
      /* Update to properly calculate the lengths. */
      curves.tag_topology_changed();

      curves = subdivide_last_segement(curves, strokes);
    }

    info.drawing.tag_topology_changed();
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
  ot->name = "Set Cyclical State";
  ot->idname = "GREASE_PENCIL_OT_cyclical_set";
  ot->description = "Close or open the selected stroke adding a segment from last to first point";

  ot->invoke = WM_menu_invoke;
  ot->exec = grease_pencil_cyclical_set_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(
      ot->srna, "type", prop_cyclical_types, int(CyclicalMode::TOGGLE), "Type", "");

  RNA_def_boolean(ot->srna,
                  "subdivide_cyclic_segment",
                  true,
                  "Match Point Density",
                  "Add point in the new segment to keep the same density");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Active Material Operator
 * \{ */

static wmOperatorStatus grease_pencil_set_active_material_exec(bContext *C, wmOperator * /*op*/)
{
  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  if (object->totcol == 0) {
    return OPERATOR_CANCELLED;
  }

  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  for (const MutableDrawingInfo &info : drawings) {
    IndexMaskMemory memory;
    const IndexMask strokes = ed::greasepencil::retrieve_editable_and_selected_strokes(
        *object, info.drawing, info.layer_index, memory);
    if (strokes.is_empty()) {
      continue;
    }
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();

    const VArray<int> materials = *curves.attributes().lookup_or_default<int>(
        "material_index", bke::AttrDomain::Curve, 0);
    object->actcol = materials[strokes.first()] + 1;
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

/* -------------------------------------------------------------------- */
/** \name Set Uniform Thickness Operator
 * \{ */

static wmOperatorStatus grease_pencil_set_uniform_thickness_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  /* Radius is half of the thickness. */
  const float radius = RNA_float_get(op->ptr, "thickness") * 0.5f;

  bool changed = false;
  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    IndexMaskMemory memory;
    const IndexMask strokes = ed::greasepencil::retrieve_editable_and_selected_strokes(
        *object, info.drawing, info.layer_index, memory);
    if (strokes.is_empty()) {
      return;
    }
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();

    const OffsetIndices<int> points_by_curve = curves.points_by_curve();
    MutableSpan<float> radii = info.drawing.radii_for_write();
    bke::curves::fill_points<float>(points_by_curve, strokes, radius, radii);
    changed = true;
  });

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_set_uniform_thickness(wmOperatorType *ot)
{
  ot->name = "Set Uniform Thickness";
  ot->idname = "GREASE_PENCIL_OT_set_uniform_thickness";
  ot->description = "Set all stroke points to same thickness";

  ot->exec = grease_pencil_set_uniform_thickness_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_float(
      ot->srna, "thickness", 0.1f, 0.0f, 1000.0f, "Thickness", "Thickness", 0.0f, 1000.0f);
}

/** \} */
/* -------------------------------------------------------------------- */
/** \name Set Uniform Opacity Operator
 * \{ */

static wmOperatorStatus grease_pencil_set_uniform_opacity_exec(bContext *C, wmOperator *op)
{
  using namespace blender::bke;

  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  const float opacity_stroke = RNA_float_get(op->ptr, "opacity_stroke");
  const float opacity_fill = RNA_float_get(op->ptr, "opacity_fill");

  bool changed = false;
  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    IndexMaskMemory memory;
    const IndexMask strokes = ed::greasepencil::retrieve_editable_and_selected_strokes(
        *object, info.drawing, info.layer_index, memory);
    if (strokes.is_empty()) {
      return;
    }
    CurvesGeometry &curves = info.drawing.strokes_for_write();
    MutableAttributeAccessor attributes = curves.attributes_for_write();
    const OffsetIndices<int> points_by_curve = curves.points_by_curve();

    MutableSpan<float> opacities = info.drawing.opacities_for_write();
    bke::curves::fill_points<float>(points_by_curve, strokes, opacity_stroke, opacities);

    if (SpanAttributeWriter<float> fill_opacities = attributes.lookup_or_add_for_write_span<float>(
            "fill_opacity", AttrDomain::Curve))
    {
      strokes.foreach_index(GrainSize(2048), [&](const int64_t curve) {
        fill_opacities.span[curve] = opacity_fill;
      });
    }

    changed = true;
  });

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_set_uniform_opacity(wmOperatorType *ot)
{
  ot->name = "Set Uniform Opacity";
  ot->idname = "GREASE_PENCIL_OT_set_uniform_opacity";
  ot->description = "Set all stroke points to same opacity";

  ot->exec = grease_pencil_set_uniform_opacity_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Differentiate default opacities for stroke & fills so shapes with same stroke+fill colors will
   * be more readable. */
  RNA_def_float(ot->srna, "opacity_stroke", 1.0f, 0.0f, 1.0f, "Stroke Opacity", "", 0.0f, 1.0f);
  RNA_def_float(ot->srna, "opacity_fill", 0.5f, 0.0f, 1.0f, "Fill Opacity", "", 0.0f, 1.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Switch Direction Operator
 * \{ */

static wmOperatorStatus grease_pencil_stroke_switch_direction_exec(bContext *C,
                                                                   wmOperator * /*op*/)
{
  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  bool changed = false;
  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    IndexMaskMemory memory;
    const IndexMask strokes = ed::greasepencil::retrieve_editable_and_selected_strokes(
        *object, info.drawing, info.layer_index, memory);
    if (strokes.is_empty()) {
      return;
    }
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();

    /* Switch stroke direction. */
    curves.reverse_curves(strokes);

    changed = true;
  });

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_stroke_switch_direction(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Switch Direction";
  ot->idname = "GREASE_PENCIL_OT_stroke_switch_direction";
  ot->description = "Change direction of the points of the selected strokes";

  /* Callbacks. */
  ot->exec = grease_pencil_stroke_switch_direction_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */
/* -------------------------------------------------------------------- */
/** \name Set Start Point Operator
 * \{ */
static bke::CurvesGeometry set_start_point(const bke::CurvesGeometry &curves,
                                           const IndexMask &mask)
{

  const OffsetIndices<int> points_by_curve = curves.points_by_curve();
  const VArray<bool> src_cyclic = curves.cyclic();

  /* Early-return if no cyclic curves. */
  if (array_utils::booleans_mix_calc(src_cyclic) == array_utils::BooleanMix::AllFalse) {
    return curves;
  }

  Array<bool> start_set_points(curves.points_num());
  mask.to_bools(start_set_points.as_mutable_span());

  Array<int> dst_to_src_point(curves.points_num());

  threading::parallel_for(curves.curves_range(), 1024, [&](const IndexRange range) {
    for (const int curve_i : range) {
      const IndexRange points = points_by_curve[curve_i];
      const Span<bool> curve_i_selected_points = start_set_points.as_span().slice(points);
      const int first_selected = curve_i_selected_points.first_index_try(true);

      MutableSpan<int> dst_to_src_slice = dst_to_src_point.as_mutable_span().slice(points);

      array_utils::fill_index_range<int>(dst_to_src_slice, points.start());

      if (first_selected == -1 || src_cyclic[curve_i] == false) {
        continue;
      }

      std::rotate(dst_to_src_slice.begin(),
                  dst_to_src_slice.begin() + first_selected,
                  dst_to_src_slice.end());
    }
  });

  /* New CurvesGeometry to copy to. */
  bke::CurvesGeometry dst_curves(curves.points_num(), curves.curves_num());
  BKE_defgroup_copy_list(&dst_curves.vertex_group_names, &curves.vertex_group_names);

  /* Copy offsets. */
  array_utils::copy(curves.offsets(), dst_curves.offsets_for_write());

  /* Attribute accessors for copying. */
  bke::MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();
  const bke::AttributeAccessor src_attributes = curves.attributes();

  /* Copy curve attrs. */
  bke::copy_attributes(
      src_attributes, bke::AttrDomain::Curve, bke::AttrDomain::Curve, {}, dst_attributes);
  array_utils::copy(src_cyclic, dst_curves.cyclic_for_write());

  /* Copy point attrs */
  gather_attributes(src_attributes,
                    bke::AttrDomain::Point,
                    bke::AttrDomain::Point,
                    {},
                    dst_to_src_point,
                    dst_attributes);

  dst_curves.update_curve_types();
  /* TODO: change to copying knots by point. */
  if (curves.nurbs_has_custom_knots()) {
    bke::curves::nurbs::update_custom_knot_modes(
        dst_curves.curves_range(), NURBS_KNOT_MODE_NORMAL, NURBS_KNOT_MODE_NORMAL, dst_curves);
  }
  return dst_curves;
}

static wmOperatorStatus grease_pencil_set_start_point_exec(bContext *C, wmOperator * /*op*/)
{
  using namespace bke::greasepencil;
  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  std::atomic<bool> changed = false;
  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    IndexMaskMemory memory;
    const IndexMask selection = retrieve_editable_and_selected_points(
        *object, info.drawing, info.layer_index, memory);
    if (selection.is_empty()) {
      return;
    }

    info.drawing.strokes_for_write() = set_start_point(info.drawing.strokes(), selection);

    info.drawing.tag_topology_changed();
    changed = true;
  });

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }
  return OPERATOR_FINISHED;
}
static void GREASE_PENCIL_OT_set_start_point(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Set Start Point";
  ot->idname = "GREASE_PENCIL_OT_set_start_point";
  ot->description = "Select which point is the beginning of the curve";

  /* Callbacks */
  ot->exec = grease_pencil_set_start_point_exec;
  ot->poll = editable_grease_pencil_point_selection_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Curve Caps Operator
 * \{ */

enum class CapsMode : int8_t {
  /** Switches both to Flat. */
  FLAT = 0,
  /** Change only start. */
  START = 1,
  /** Change only end. */
  END = 2,
  /** Switches both to default rounded. */
  ROUND = 3,
};

static void toggle_caps(MutableSpan<int8_t> caps, const IndexMask &strokes)
{
  strokes.foreach_index([&](const int stroke_i) {
    if (caps[stroke_i] == GP_STROKE_CAP_FLAT) {
      caps[stroke_i] = GP_STROKE_CAP_ROUND;
    }
    else {
      caps[stroke_i] = GP_STROKE_CAP_FLAT;
    }
  });
}

static wmOperatorStatus grease_pencil_caps_set_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  const CapsMode mode = CapsMode(RNA_enum_get(op->ptr, "type"));

  bool changed = false;
  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
    IndexMaskMemory memory;
    const IndexMask strokes = ed::greasepencil::retrieve_editable_and_selected_strokes(
        *object, info.drawing, info.layer_index, memory);
    if (strokes.is_empty()) {
      return;
    }

    bke::MutableAttributeAccessor attributes = curves.attributes_for_write();

    if (ELEM(mode, CapsMode::ROUND, CapsMode::FLAT)) {
      const int8_t flag_set = (mode == CapsMode::ROUND) ? int8_t(GP_STROKE_CAP_TYPE_ROUND) :
                                                          int8_t(GP_STROKE_CAP_TYPE_FLAT);
      if (bke::SpanAttributeWriter<int8_t> start_caps =
              attributes.lookup_or_add_for_write_span<int8_t>("start_cap", bke::AttrDomain::Curve))
      {
        index_mask::masked_fill(start_caps.span, flag_set, strokes);
        start_caps.finish();
      }
      if (bke::SpanAttributeWriter<int8_t> end_caps =
              attributes.lookup_or_add_for_write_span<int8_t>("end_cap", bke::AttrDomain::Curve))
      {
        index_mask::masked_fill(end_caps.span, flag_set, strokes);
        end_caps.finish();
      }
    }
    else {
      switch (mode) {
        case CapsMode::START: {
          if (bke::SpanAttributeWriter<int8_t> caps =
                  attributes.lookup_or_add_for_write_span<int8_t>("start_cap",
                                                                  bke::AttrDomain::Curve))
          {
            toggle_caps(caps.span, strokes);
            caps.finish();
          }
          break;
        }
        case CapsMode::END: {
          if (bke::SpanAttributeWriter<int8_t> caps =
                  attributes.lookup_or_add_for_write_span<int8_t>("end_cap",
                                                                  bke::AttrDomain::Curve))
          {
            toggle_caps(caps.span, strokes);
            caps.finish();
          }
          break;
        }
        case CapsMode::ROUND:
        case CapsMode::FLAT:
          break;
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

static void GREASE_PENCIL_OT_caps_set(wmOperatorType *ot)
{
  static const EnumPropertyItem prop_caps_types[] = {
      {int(CapsMode::ROUND), "ROUND", 0, "Rounded", "Set as default rounded"},
      {int(CapsMode::FLAT), "FLAT", 0, "Flat", ""},
      RNA_ENUM_ITEM_SEPR,
      {int(CapsMode::START), "START", 0, "Toggle Start", ""},
      {int(CapsMode::END), "END", 0, "Toggle End", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  ot->name = "Set Curve Caps";
  ot->idname = "GREASE_PENCIL_OT_caps_set";
  ot->description = "Change curve caps mode (rounded or flat)";

  ot->invoke = WM_menu_invoke;
  ot->exec = grease_pencil_caps_set_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna, "type", prop_caps_types, int(CapsMode::ROUND), "Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Active Material Operator
 * \{ */

/* Retry enum items with object materials. */
static const EnumPropertyItem *material_enum_itemf(bContext *C,
                                                   PointerRNA * /*ptr*/,
                                                   PropertyRNA * /*prop*/,
                                                   bool *r_free)
{
  Object *ob = CTX_data_active_object(C);
  EnumPropertyItem *item = nullptr, item_tmp = {0};
  int totitem = 0;

  if (ob == nullptr) {
    return rna_enum_dummy_DEFAULT_items;
  }

  /* Existing materials */
  for (const int i : IndexRange(ob->totcol)) {
    if (Material *ma = BKE_object_material_get(ob, i + 1)) {
      item_tmp.identifier = ma->id.name + 2;
      item_tmp.name = ma->id.name + 2;
      item_tmp.value = i + 1;
      item_tmp.icon = ma->preview ? ma->preview->runtime->icon_id : ICON_NONE;

      RNA_enum_item_add(&item, &totitem, &item_tmp);
    }
  }
  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

static wmOperatorStatus grease_pencil_set_material_exec(bContext *C, wmOperator *op)
{
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  const int slot = RNA_enum_get(op->ptr, "slot");

  /* Try to get material slot. */
  if ((slot < 1) || (slot > object->totcol)) {
    return OPERATOR_CANCELLED;
  }

  /* Set active material. */
  object->actcol = slot;

  WM_event_add_notifier(C, NC_GEOM | ND_DATA | NA_EDITED, &grease_pencil);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_set_material(wmOperatorType *ot)
{
  ot->name = "Set Active Material";
  ot->idname = "GREASE_PENCIL_OT_set_material";
  ot->description = "Set active material";

  ot->exec = grease_pencil_set_material_exec;
  ot->poll = active_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Material to use (dynamic enum) */
  ot->prop = RNA_def_enum(ot->srna, "slot", rna_enum_dummy_DEFAULT_items, 0, "Material Slot", "");
  RNA_def_enum_funcs(ot->prop, material_enum_itemf);
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Duplicate Operator
 * \{ */

static wmOperatorStatus grease_pencil_duplicate_exec(bContext *C, wmOperator * /*op*/)
{
  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  View3D *v3d = CTX_wm_view3d(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  const bke::AttrDomain selection_domain = ED_grease_pencil_edit_selection_domain_get(
      scene->toolsettings);

  std::atomic<bool> changed = false;
  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    IndexMaskMemory memory;

    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
    if (selection_domain == bke::AttrDomain::Curve) {
      const IndexMask strokes = retrieve_editable_and_selected_strokes(
          *object, info.drawing, info.layer_index, memory);
      if (strokes.is_empty()) {
        return;
      }
      curves::duplicate_curves(curves, strokes);
    }
    else if (selection_domain == bke::AttrDomain::Point) {
      const IndexMask points = ed::greasepencil::retrieve_editable_and_all_selected_points(
          *object, info.drawing, info.layer_index, v3d->overlay.handle_display, memory);
      if (points.is_empty()) {
        return;
      }
      curves::duplicate_points(curves, points);
    }
    info.drawing.tag_topology_changed();
    changed.store(true, std::memory_order_relaxed);
  });

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }
  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_duplicate(wmOperatorType *ot)
{
  ot->name = "Duplicate";
  ot->idname = "GREASE_PENCIL_OT_duplicate";
  ot->description = "Duplicate the selected points";

  ot->exec = grease_pencil_duplicate_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus grease_pencil_clean_loose_exec(bContext *C, wmOperator *op)
{
  Object *object = CTX_data_active_object(C);
  Scene &scene = *CTX_data_scene(C);
  const int limit = RNA_int_get(op->ptr, "limit");

  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(scene, grease_pencil);

  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
    const OffsetIndices<int> points_by_curve = curves.points_by_curve();

    IndexMaskMemory memory;
    const IndexMask editable_strokes = ed::greasepencil::retrieve_editable_strokes(
        *object, info.drawing, info.layer_index, memory);

    const IndexMask curves_to_delete = IndexMask::from_predicate(
        editable_strokes, GrainSize(4096), memory, [&](const int i) {
          return points_by_curve[i].size() <= limit;
        });

    curves.remove_curves(curves_to_delete, {});
  });

  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus grease_pencil_clean_loose_invoke(bContext *C,
                                                         wmOperator *op,
                                                         const wmEvent *event)
{
  return WM_operator_props_popup_confirm_ex(
      C, op, event, IFACE_("Remove Loose Points"), IFACE_("Delete"));
}

static void GREASE_PENCIL_OT_clean_loose(wmOperatorType *ot)
{
  ot->name = "Clean Loose Points";
  ot->idname = "GREASE_PENCIL_OT_clean_loose";
  ot->description = "Remove loose points";

  ot->invoke = grease_pencil_clean_loose_invoke;
  ot->exec = grease_pencil_clean_loose_exec;
  ot->poll = active_grease_pencil_layer_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_int(ot->srna,
              "limit",
              1,
              1,
              INT_MAX,
              "Limit",
              "Number of points to consider stroke as loose",
              1,
              INT_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Subdivide Operator
 * \{ */

static wmOperatorStatus gpencil_stroke_subdivide_exec(bContext *C, wmOperator *op)
{
  const int cuts = RNA_int_get(op->ptr, "number_cuts");
  const bool only_selected = RNA_boolean_get(op->ptr, "only_selected");

  std::atomic<bool> changed = false;

  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  View3D *v3d = CTX_wm_view3d(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  const bke::AttrDomain selection_domain = ED_grease_pencil_edit_selection_domain_get(
      scene->toolsettings);

  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);

  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    IndexMaskMemory memory;
    const IndexMask strokes = ed::greasepencil::retrieve_editable_and_selected_strokes(
        *object, info.drawing, info.layer_index, memory);
    if (strokes.is_empty()) {
      return;
    }
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();

    VArray<int> vcuts = {};

    if (selection_domain == bke::AttrDomain::Curve || !only_selected) {
      /* Subdivide entire selected curve, every stroke subdivides to the same cut. */
      vcuts = VArray<int>::from_single(cuts, curves.points_num());
    }
    else if (selection_domain == bke::AttrDomain::Point) {
      /* Subdivide between selected points. Only cut between selected points.
       * Make the cut array the same length as point count for specifying
       * cut/uncut for each segment. */
      const VArray<bool> selection = *curves.attributes().lookup_or_default<bool>(
          ".selection", bke::AttrDomain::Point, true);
      const VArray<bool> selection_left = *curves.attributes().lookup_or_default<bool>(
          ".selection_handle_left", bke::AttrDomain::Point, true);
      const VArray<bool> selection_right = *curves.attributes().lookup_or_default<bool>(
          ".selection_handle_right", bke::AttrDomain::Point, true);
      const VArray<int8_t> curve_types = curves.curve_types();

      auto is_selected = [&](const int point_i, const int curve_i) {
        if (selection[point_i]) {
          return true;
        }
        if (v3d->overlay.handle_display == CURVE_HANDLE_NONE) {
          return false;
        }
        if (curve_types[curve_i] == CURVE_TYPE_BEZIER) {
          return selection_left[point_i] || selection_right[point_i];
        }
        return false;
      };

      const OffsetIndices points_by_curve = curves.points_by_curve();
      const VArray<bool> cyclic = curves.cyclic();

      Array<int> use_cuts(curves.points_num(), 0);

      /* The cut is after each point, so the last point selected wouldn't need to be registered. */
      for (const int curve : curves.curves_range()) {
        /* No need to loop to the last point since the cut is registered on the point before the
         * segment. */
        for (const int point : points_by_curve[curve].drop_back(1)) {
          /* The point itself should be selected. */
          if (!is_selected(point, curve)) {
            continue;
          }
          /* If the next point in the curve is selected, then cut this segment. */
          if (is_selected(point + 1, curve)) {
            use_cuts[point] = cuts;
          }
        }
        /* Check for cyclic and selection. */
        if (cyclic[curve]) {
          const int first_point = points_by_curve[curve].first();
          const int last_point = points_by_curve[curve].last();
          if (is_selected(first_point, curve) && is_selected(last_point, curve)) {
            use_cuts[last_point] = cuts;
          }
        }
      }
      vcuts = VArray<int>::from_container(std::move(use_cuts));
    }

    curves = geometry::subdivide_curves(curves, strokes, vcuts);
    info.drawing.tag_topology_changed();
    changed.store(true, std::memory_order_relaxed);
  });

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_stroke_subdivide(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Subdivide Stroke";
  ot->idname = "GREASE_PENCIL_OT_stroke_subdivide";
  ot->description =
      "Subdivide between continuous selected points of the stroke adding a point half way "
      "between "
      "them";

  ot->exec = gpencil_stroke_subdivide_exec;
  ot->poll = ed::greasepencil::editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  prop = RNA_def_int(ot->srna, "number_cuts", 1, 1, 32, "Number of Cuts", "", 1, 5);
  /* Avoid re-using last var because it can cause _very_ high value and annoy users. */
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  RNA_def_boolean(ot->srna,
                  "only_selected",
                  true,
                  "Selected Points",
                  "Smooth only selected points in the stroke");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Reorder Operator
 * \{ */

enum class ReorderDirection : int8_t {
  /** Move the selected strokes to be drawn in front. */
  TOP = 0,
  /** Increase the draw order of the selected strokes. */
  UP = 1,
  /** Decrease the draw order of the selected strokes. */
  DOWN = 2,
  /** Move the selected strokes to be drawn behind. */
  BOTTOM = 3,
};

static Array<int> get_reordered_indices(const IndexRange universe,
                                        const IndexMask &selected,
                                        const ReorderDirection direction)
{
  Array<int> indices(universe.size());

  if (ELEM(direction, ReorderDirection::UP, ReorderDirection::DOWN)) {
    /* Initialize the indices. */
    array_utils::fill_index_range<int>(indices);
  }

  if (ELEM(direction, ReorderDirection::TOP, ReorderDirection::BOTTOM)) {
    /*
     * Take the selected indices and move them to the start for `Bottom` or the end for `Top`
     * And fill the reset with the unselected indices.
     *
     * Here's a diagram:
     *
     *        Input
     * 0 1 2 3 4 5 6 7 8 9
     *     ^   ^ ^
     *
     *         Top
     * |-----A-----| |-B-|
     * 0 1 3 6 7 8 9 2 4 5
     *               ^ ^ ^
     *
     *        Bottom
     * |-A-| |-----B-----|
     * 2 4 5 0 1 3 6 7 8 9
     * ^ ^ ^
     */

    IndexMaskMemory memory;
    const IndexMask unselected = selected.complement(universe, memory);

    const IndexMask &A = (direction == ReorderDirection::BOTTOM) ? selected : unselected;
    const IndexMask &B = (direction == ReorderDirection::BOTTOM) ? unselected : selected;

    A.to_indices(indices.as_mutable_span().take_front(A.size()));
    B.to_indices(indices.as_mutable_span().take_back(B.size()));
  }
  else if (direction == ReorderDirection::DOWN) {
    selected.foreach_index_optimized<int>([&](const int curve_i, const int pos) {
      /* Check if the curve index is touching the beginning without any gaps. */
      if (curve_i != pos) {
        /* Move a index down by flipping it with the one below it. */
        std::swap(indices[curve_i], indices[curve_i - 1]);
      }
    });
  }
  else if (direction == ReorderDirection::UP) {
    Array<int> selected_indices(selected.size());
    selected.to_indices(selected_indices.as_mutable_span());

    /* Because each index is moving up we need to loop through the indices backwards,
     * starting at the largest. */
    for (const int i : selected_indices.index_range()) {
      const int pos = selected_indices.index_range().last(i);
      const int curve_i = selected_indices[pos];

      /* Check if the curve index is touching the end without any gaps. */
      if (curve_i != universe.last(i)) {
        /* Move a index up by flipping it with the one above it. */
        std::swap(indices[curve_i], indices[curve_i + 1]);
      }
    }
  }

  return indices;
}

static wmOperatorStatus grease_pencil_stroke_reorder_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  const ReorderDirection direction = ReorderDirection(RNA_enum_get(op->ptr, "direction"));

  std::atomic<bool> changed = false;
  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    IndexMaskMemory memory;
    const IndexMask strokes = ed::greasepencil::retrieve_editable_and_selected_strokes(
        *object, info.drawing, info.layer_index, memory);
    if (strokes.is_empty()) {
      return;
    }
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();

    /* Return if everything is selected. */
    if (strokes.size() == curves.curves_num()) {
      return;
    }

    const Array<int> indices = get_reordered_indices(curves.curves_range(), strokes, direction);

    curves = geometry::reorder_curves_geometry(curves, indices, {});
    info.drawing.tag_topology_changed();
    changed.store(true, std::memory_order_relaxed);
  });

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_stroke_reorder(wmOperatorType *ot)
{
  static const EnumPropertyItem prop_reorder_direction[] = {
      {int(ReorderDirection::TOP), "TOP", 0, "Bring to Front", ""},
      {int(ReorderDirection::UP), "UP", 0, "Bring Forward", ""},
      RNA_ENUM_ITEM_SEPR,
      {int(ReorderDirection::DOWN), "DOWN", 0, "Send Backward", ""},
      {int(ReorderDirection::BOTTOM), "BOTTOM", 0, "Send to Back", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  ot->name = "Reorder";
  ot->idname = "GREASE_PENCIL_OT_reorder";
  ot->description = "Change the display order of the selected strokes";

  ot->exec = grease_pencil_stroke_reorder_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(
      ot->srna, "direction", prop_reorder_direction, int(ReorderDirection::TOP), "Direction", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Move To Layer Operator
 * \{ */

static wmOperatorStatus grease_pencil_move_to_layer_exec(bContext *C, wmOperator *op)
{
  using namespace bke::greasepencil;
  const Scene *scene = CTX_data_scene(C);
  bool changed = false;

  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  std::string target_layer_name = RNA_string_get(op->ptr, "target_layer_name");
  const bool add_new_layer = RNA_boolean_get(op->ptr, "add_new_layer");
  TreeNode *target_node = nullptr;

  if (add_new_layer) {
    target_node = &grease_pencil.add_layer(target_layer_name).as_node();
  }
  else {
    target_node = grease_pencil.find_node_by_name(target_layer_name);
  }

  if (target_node == nullptr || !target_node->is_layer()) {
    BKE_reportf(op->reports, RPT_ERROR, "There is no layer '%s'", target_layer_name.c_str());
    return OPERATOR_CANCELLED;
  }

  Layer &layer_dst = target_node->as_layer();
  if (layer_dst.is_locked()) {
    BKE_reportf(op->reports, RPT_ERROR, "'%s' Layer is locked", target_layer_name.c_str());
    return OPERATOR_CANCELLED;
  }

  /* Iterate through all the drawings at current scene frame. */
  const Vector<MutableDrawingInfo> drawings_src = retrieve_editable_drawings(*scene,
                                                                             grease_pencil);
  for (const MutableDrawingInfo &info : drawings_src) {
    bke::CurvesGeometry &curves_src = info.drawing.strokes_for_write();
    IndexMaskMemory memory;
    const IndexMask selected_strokes = ed::curves::retrieve_selected_curves(curves_src, memory);
    if (selected_strokes.is_empty()) {
      continue;
    }

    if (!layer_dst.frames().lookup_ptr(info.frame_number)) {
      /* Move geometry to a new drawing in target layer. */
      Drawing &drawing_dst = *grease_pencil.insert_frame(layer_dst, info.frame_number);
      drawing_dst.strokes_for_write() = bke::curves_copy_curve_selection(
          curves_src, selected_strokes, {});

      curves_src.remove_curves(selected_strokes, {});
      drawing_dst.tag_topology_changed();
    }
    else if (Drawing *drawing_dst = grease_pencil.get_drawing_at(layer_dst, info.frame_number)) {
      /* Append geometry to drawing in target layer. */
      bke::CurvesGeometry selected_elems = curves_copy_curve_selection(
          curves_src, selected_strokes, {});
      Curves *selected_curves = bke::curves_new_nomain(std::move(selected_elems));
      Curves *layer_curves = bke::curves_new_nomain(std::move(drawing_dst->strokes_for_write()));
      std::array<bke::GeometrySet, 2> geometry_sets{
          bke::GeometrySet::from_curves(layer_curves),
          bke::GeometrySet::from_curves(selected_curves)};
      bke::GeometrySet joined = geometry::join_geometries(geometry_sets, {});
      drawing_dst->strokes_for_write() = std::move(joined.get_curves_for_write()->geometry.wrap());

      curves_src.remove_curves(selected_strokes, {});

      drawing_dst->tag_topology_changed();
    }

    info.drawing.tag_topology_changed();
    changed = true;
  }

  if (changed) {
    /* updates */
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus grease_pencil_move_to_layer_invoke(bContext *C,
                                                           wmOperator *op,
                                                           const wmEvent *event)
{
  const bool add_new_layer = RNA_boolean_get(op->ptr, "add_new_layer");
  if (add_new_layer) {
    Object *object = CTX_data_active_object(C);
    GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

    const std::string unique_name = grease_pencil.unique_layer_name("Layer");
    RNA_string_set(op->ptr, "target_layer_name", unique_name.c_str());

    return WM_operator_props_popup_confirm_ex(
        C, op, event, IFACE_("Move to New Layer"), IFACE_("Create"));
  }

  /* Show the move menu if this operator is invoked from operator search without any property
   * pre-set. */
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "target_layer_name");
  if (!RNA_property_is_set(op->ptr, prop)) {
    WM_menu_name_call(C, "GREASE_PENCIL_MT_move_to_layer", wm::OpCallContext::InvokeDefault);
    return OPERATOR_FINISHED;
  }

  return grease_pencil_move_to_layer_exec(C, op);
}

static void GREASE_PENCIL_OT_move_to_layer(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Move to Layer";
  ot->idname = "GREASE_PENCIL_OT_move_to_layer";
  ot->description = "Move selected strokes to another layer";

  ot->invoke = grease_pencil_move_to_layer_invoke;
  ot->exec = grease_pencil_move_to_layer_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  prop = RNA_def_string(
      ot->srna, "target_layer_name", nullptr, INT16_MAX, "Name", "Target Grease Pencil Layer");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(
      ot->srna, "add_new_layer", false, "New Layer", "Move selection to a new layer");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Separate Operator
 * \{ */

enum class SeparateMode : int8_t {
  /* Selected Points/Strokes. */
  SELECTED = 0,
  /* By Material. */
  MATERIAL = 1,
  /* By each Layer. */
  LAYER = 2,
};

static const EnumPropertyItem prop_separate_modes[] = {
    {int(SeparateMode::SELECTED), "SELECTED", 0, "Selection", "Separate selected geometry"},
    {int(SeparateMode::MATERIAL), "MATERIAL", 0, "By Material", "Separate by material"},
    {int(SeparateMode::LAYER), "LAYER", 0, "By Layer", "Separate by layer"},
    {0, nullptr, 0, nullptr, nullptr},
};

static void remove_unused_materials(Main *bmain, Object *object)
{
  int actcol = object->actcol;
  for (int slot = 1; slot <= object->totcol; slot++) {
    while (slot <= object->totcol && !BKE_object_material_slot_used(object, slot)) {
      object->actcol = slot;
      if (!BKE_object_material_slot_remove(bmain, object)) {
        break;
      }

      if (actcol >= slot) {
        actcol--;
      }
    }
  }
  object->actcol = actcol;
}

static Object *duplicate_grease_pencil_object(Main *bmain,
                                              Scene *scene,
                                              ViewLayer *view_layer,
                                              Base *base_prev,
                                              const GreasePencil &grease_pencil_src)
{
  const eDupli_ID_Flags dupflag = eDupli_ID_Flags(U.dupflag & USER_DUP_GPENCIL);
  Base *base_new = object::add_duplicate(bmain, scene, view_layer, base_prev, dupflag);
  Object *object_dst = base_new->object;
  object_dst->mode = OB_MODE_OBJECT;
  GreasePencil *grease_pencil_dst = BKE_grease_pencil_add(bmain, grease_pencil_src.id.name + 2);
  BKE_grease_pencil_copy_parameters(grease_pencil_src, *grease_pencil_dst);
  object_dst->data = grease_pencil_dst;

  return object_dst;
}

static bke::greasepencil::Layer &find_or_create_layer_in_dst_by_name(
    const int layer_index,
    const GreasePencil &grease_pencil_src,
    GreasePencil &grease_pencil_dst,
    Vector<int> &src_to_dst_layer_indices)
{
  using namespace bke::greasepencil;

  /* This assumes that the index is valid. Will cause an assert if it is not. */
  const Layer &layer_src = grease_pencil_src.layer(layer_index);
  if (TreeNode *node = grease_pencil_dst.find_node_by_name(layer_src.name())) {
    return node->as_layer();
  }

  /* If the layer can't be found in `grease_pencil_dst` by name add a new layer. */
  Layer &new_layer = grease_pencil_dst.add_layer(layer_src.name());
  BKE_grease_pencil_copy_layer_parameters(layer_src, new_layer);
  src_to_dst_layer_indices.append(layer_index);

  return new_layer;
}

static bool grease_pencil_separate_selected(bContext &C,
                                            Main &bmain,
                                            Scene &scene,
                                            ViewLayer &view_layer,
                                            Base &base_prev,
                                            Object &object_src)
{
  using namespace bke::greasepencil;
  bool changed = false;

  GreasePencil &grease_pencil_src = *static_cast<GreasePencil *>(object_src.data);
  Object *object_dst = duplicate_grease_pencil_object(
      &bmain, &scene, &view_layer, &base_prev, grease_pencil_src);
  GreasePencil &grease_pencil_dst = *static_cast<GreasePencil *>(object_dst->data);

  /* Iterate through all the drawings at current scene frame. */
  const Vector<MutableDrawingInfo> drawings_src = retrieve_editable_drawings(scene,
                                                                             grease_pencil_src);
  Vector<int> src_to_dst_layer_indices;
  for (const MutableDrawingInfo &info : drawings_src) {
    bke::CurvesGeometry &curves_src = info.drawing.strokes_for_write();
    IndexMaskMemory memory;
    const IndexMask selected_points = ed::curves::retrieve_selected_points(curves_src, memory);
    if (selected_points.is_empty()) {
      continue;
    }

    /* Insert Keyframe at current frame/layer. */
    Layer &layer_dst = find_or_create_layer_in_dst_by_name(
        info.layer_index, grease_pencil_src, grease_pencil_dst, src_to_dst_layer_indices);

    Drawing *drawing_dst = grease_pencil_dst.insert_frame(layer_dst, info.frame_number);
    BLI_assert(drawing_dst != nullptr);

    /* Copy strokes to new CurvesGeometry. */
    drawing_dst->strokes_for_write() = bke::curves_copy_point_selection(
        curves_src, selected_points, {});
    curves_src = geometry::remove_points_and_split(curves_src, selected_points);

    info.drawing.tag_topology_changed();
    drawing_dst->tag_topology_changed();

    changed = true;
  }

  if (changed) {
    /* Transfer layer attributes. */
    bke::gather_attributes(grease_pencil_src.attributes(),
                           bke::AttrDomain::Layer,
                           bke::AttrDomain::Layer,
                           {},
                           src_to_dst_layer_indices.as_span(),
                           grease_pencil_dst.attributes_for_write());

    /* Set the active layer in the target object. */
    if (grease_pencil_src.has_active_layer()) {
      const Layer &active_layer_src = *grease_pencil_src.get_active_layer();
      TreeNode *active_layer_dst = grease_pencil_dst.find_node_by_name(active_layer_src.name());
      if (active_layer_dst && active_layer_dst->is_layer()) {
        grease_pencil_dst.set_active_layer(&active_layer_dst->as_layer());
      }
    }

    /* Add object materials to target object. */
    BKE_object_material_array_assign(&bmain,
                                     object_dst,
                                     BKE_object_material_array_p(&object_src),
                                     *BKE_object_material_len_p(&object_src),
                                     false);

    remove_unused_materials(&bmain, object_dst);
    DEG_id_tag_update(&grease_pencil_dst.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(&C, NC_OBJECT | ND_DRAW, &grease_pencil_dst);
  }
  return changed;
}

static bool grease_pencil_separate_layer(bContext &C,
                                         Main &bmain,
                                         Scene &scene,
                                         ViewLayer &view_layer,
                                         Base &base_prev,
                                         Object &object_src)
{
  using namespace bke::greasepencil;
  bool changed = false;

  GreasePencil &grease_pencil_src = *static_cast<GreasePencil *>(object_src.data);

  /* Create a new object for each layer. */
  for (const int layer_i : grease_pencil_src.layers().index_range()) {
    Layer &layer_src = grease_pencil_src.layer(layer_i);
    if (layer_src.is_locked()) {
      continue;
    }

    Object *object_dst = duplicate_grease_pencil_object(
        &bmain, &scene, &view_layer, &base_prev, grease_pencil_src);
    GreasePencil &grease_pencil_dst = *static_cast<GreasePencil *>(object_dst->data);
    Vector<int> src_to_dst_layer_indices;
    Layer &layer_dst = find_or_create_layer_in_dst_by_name(
        layer_i, grease_pencil_src, grease_pencil_dst, src_to_dst_layer_indices);

    /* Iterate through all the drawings at current frame. */
    const Vector<MutableDrawingInfo> drawings_src = retrieve_editable_drawings_from_layer(
        scene, grease_pencil_src, layer_src);
    for (const MutableDrawingInfo &info : drawings_src) {
      bke::CurvesGeometry &curves_src = info.drawing.strokes_for_write();
      IndexMaskMemory memory;
      const IndexMask strokes = retrieve_editable_strokes(
          object_src, info.drawing, info.layer_index, memory);
      if (strokes.is_empty()) {
        continue;
      }

      /* Add object materials. */
      BKE_object_material_array_assign(&bmain,
                                       object_dst,
                                       BKE_object_material_array_p(&object_src),
                                       *BKE_object_material_len_p(&object_src),
                                       false);

      /* Insert Keyframe at current frame/layer. */
      Drawing *drawing_dst = grease_pencil_dst.insert_frame(layer_dst, info.frame_number);
      /* TODO: Can we assume the insert never fails? */
      BLI_assert(drawing_dst != nullptr);

      /* Copy strokes to new CurvesGeometry. */
      drawing_dst->strokes_for_write() = bke::curves_copy_curve_selection(
          info.drawing.strokes(), strokes, {});
      curves_src.remove_curves(strokes, {});

      info.drawing.tag_topology_changed();
      drawing_dst->tag_topology_changed();

      changed = true;
    }

    /* Transfer layer attributes. */
    bke::gather_attributes(grease_pencil_src.attributes(),
                           bke::AttrDomain::Layer,
                           bke::AttrDomain::Layer,
                           {},
                           src_to_dst_layer_indices.as_span(),
                           grease_pencil_dst.attributes_for_write());

    remove_unused_materials(&bmain, object_dst);

    DEG_id_tag_update(&grease_pencil_dst.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(&C, NC_OBJECT | ND_DRAW, &grease_pencil_dst);
  }

  return changed;
}

static bool grease_pencil_separate_material(bContext &C,
                                            Main &bmain,
                                            Scene &scene,
                                            ViewLayer &view_layer,
                                            Base &base_prev,
                                            Object &object_src)
{
  using namespace blender::bke;
  using namespace bke::greasepencil;
  bool changed = false;

  GreasePencil &grease_pencil_src = *static_cast<GreasePencil *>(object_src.data);

  /* Create a new object for each material. */
  for (const int mat_i : IndexRange(object_src.totcol).drop_front(1)) {
    if (!BKE_object_material_slot_used(&object_src, mat_i + 1)) {
      continue;
    }

    Object *object_dst = duplicate_grease_pencil_object(
        &bmain, &scene, &view_layer, &base_prev, grease_pencil_src);
    GreasePencil &grease_pencil_dst = *static_cast<GreasePencil *>(object_dst->data);

    /* Add object materials. */
    BKE_object_material_array_assign(&bmain,
                                     object_dst,
                                     BKE_object_material_array_p(&object_src),
                                     *BKE_object_material_len_p(&object_src),
                                     false);

    /* Iterate through all the drawings at current scene frame. */
    const Vector<MutableDrawingInfo> drawings_src = retrieve_editable_drawings(scene,
                                                                               grease_pencil_src);
    Vector<int> src_to_dst_layer_indices;
    for (const MutableDrawingInfo &info : drawings_src) {
      bke::CurvesGeometry &curves_src = info.drawing.strokes_for_write();
      IndexMaskMemory memory;
      const IndexMask strokes = retrieve_editable_strokes_by_material(
          object_src, info.drawing, mat_i, memory);
      if (strokes.is_empty()) {
        continue;
      }

      /* Insert Keyframe at current frame/layer. */
      Layer &layer_dst = find_or_create_layer_in_dst_by_name(
          info.layer_index, grease_pencil_src, grease_pencil_dst, src_to_dst_layer_indices);

      Drawing *drawing_dst = grease_pencil_dst.insert_frame(layer_dst, info.frame_number);
      /* TODO: Can we assume the insert never fails? */
      BLI_assert(drawing_dst != nullptr);

      /* Copy strokes to new CurvesGeometry. */
      drawing_dst->strokes_for_write() = bke::curves_copy_curve_selection(curves_src, strokes, {});
      curves_src.remove_curves(strokes, {});

      info.drawing.tag_topology_changed();
      drawing_dst->tag_topology_changed();

      changed = true;
    }

    /* Transfer layer attributes. */
    bke::gather_attributes(grease_pencil_src.attributes(),
                           bke::AttrDomain::Layer,
                           bke::AttrDomain::Layer,
                           {},
                           src_to_dst_layer_indices.as_span(),
                           grease_pencil_dst.attributes_for_write());

    remove_unused_materials(&bmain, object_dst);

    DEG_id_tag_update(&grease_pencil_dst.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(&C, NC_OBJECT | ND_DRAW, &grease_pencil_dst);
  }

  if (changed) {
    remove_unused_materials(&bmain, &object_src);
  }

  return changed;
}

static wmOperatorStatus grease_pencil_separate_exec(bContext *C, wmOperator *op)
{
  using namespace bke::greasepencil;
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Base *base_prev = CTX_data_active_base(C);
  Object *object_src = CTX_data_active_object(C);
  GreasePencil &grease_pencil_src = *static_cast<GreasePencil *>(object_src->data);

  const SeparateMode mode = SeparateMode(RNA_enum_get(op->ptr, "mode"));
  bool changed = false;

  WM_cursor_wait(true);

  switch (mode) {
    case SeparateMode::SELECTED: {
      /* Cancel if nothing selected. */
      const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene,
                                                                             grease_pencil_src);
      const bool has_selection = std::any_of(
          drawings.begin(), drawings.end(), [&](const MutableDrawingInfo &info) {
            return ed::curves::has_anything_selected(info.drawing.strokes());
          });
      if (!has_selection) {
        BKE_report(op->reports, RPT_ERROR, "Nothing selected");
        WM_cursor_wait(false);
        return OPERATOR_CANCELLED;
      }

      changed = grease_pencil_separate_selected(
          *C, *bmain, *scene, *view_layer, *base_prev, *object_src);
      break;
    }
    case SeparateMode::MATERIAL: {
      /* Cancel if the object only has one material. */
      if (object_src->totcol == 1) {
        BKE_report(op->reports, RPT_ERROR, "The object has only one material");
        WM_cursor_wait(false);
        return OPERATOR_CANCELLED;
      }

      changed = grease_pencil_separate_material(
          *C, *bmain, *scene, *view_layer, *base_prev, *object_src);
      break;
    }
    case SeparateMode::LAYER: {
      /* Cancel if the object only has one layer. */
      if (grease_pencil_src.layers().size() == 1) {
        BKE_report(op->reports, RPT_ERROR, "The object has only one layer");
        WM_cursor_wait(false);
        return OPERATOR_CANCELLED;
      }
      changed = grease_pencil_separate_layer(
          *C, *bmain, *scene, *view_layer, *base_prev, *object_src);
      break;
    }
  }

  WM_cursor_wait(false);

  if (changed) {
    DEG_id_tag_update(&grease_pencil_src.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA | NA_EDITED, &grease_pencil_src);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_separate(wmOperatorType *ot)
{
  ot->name = "Separate";
  ot->idname = "GREASE_PENCIL_OT_separate";
  ot->description = "Separate the selected geometry into a new Grease Pencil object";

  ot->invoke = WM_menu_invoke;
  ot->exec = grease_pencil_separate_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(
      ot->srna, "mode", prop_separate_modes, int(SeparateMode::SELECTED), "Mode", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Copy and Paste Operator
 * \{ */

/* Global clipboard for Grease Pencil curves. */
static struct Clipboard {
  struct ClipboardLayer {
    /* Name of the layer. */
    std::string name;
    /* Curves for this layer. */
    bke::CurvesGeometry curves;
  };
  Array<ClipboardLayer> layers;
  /* Object transform of stored curves. */
  float4x4 object_to_world;
  /* We store the material uid's of the copied curves, so we can match those when pasting the
   * clipboard into another object. */
  Vector<std::pair<uint, int>> materials;
  int materials_in_source_num;
} *grease_pencil_clipboard = nullptr;

/** The clone brush accesses the clipboard from multiple threads. Protect from parallel access. */
blender::Mutex grease_pencil_clipboard_lock;

static Clipboard &ensure_grease_pencil_clipboard()
{
  std::scoped_lock lock(grease_pencil_clipboard_lock);

  if (grease_pencil_clipboard == nullptr) {
    grease_pencil_clipboard = MEM_new<Clipboard>(__func__);
  }
  return *grease_pencil_clipboard;
}

void clipboard_free()
{
  std::scoped_lock lock(grease_pencil_clipboard_lock);

  if (grease_pencil_clipboard) {
    MEM_delete(grease_pencil_clipboard);
    grease_pencil_clipboard = nullptr;
  }
}

static Array<int> clipboard_materials_remap(Main &bmain, Object &object)
{
  using namespace blender::ed::greasepencil;

  /* Get a list of all materials in the scene. */
  Map<uint, Material *> scene_materials;
  LISTBASE_FOREACH (Material *, material, &bmain.materials) {
    scene_materials.add(material->id.session_uid, material);
  }

  const Clipboard &clipboard = ensure_grease_pencil_clipboard();
  Array<int> clipboard_material_remap(clipboard.materials_in_source_num, 0);
  for (const int i : clipboard.materials.index_range()) {
    /* Check if the material name exists in the scene. */
    int target_index;
    uint material_id = clipboard.materials[i].first;
    Material *material = scene_materials.lookup_default(material_id, nullptr);
    if (!material) {
      /* Material is removed, so create a new material. */
      BKE_grease_pencil_object_material_new(&bmain, &object, nullptr, &target_index);
      clipboard_material_remap[clipboard.materials[i].second] = target_index;
      continue;
    }

    /* Find or add the material to the target object. */
    target_index = BKE_object_material_ensure(&bmain, &object, material);
    clipboard_material_remap[clipboard.materials[i].second] = target_index;
  }

  return clipboard_material_remap;
}

static bke::GeometrySet join_geometries_with_transforms(Span<bke::GeometrySet> geometries,
                                                        const VArray<float4x4> &transforms)
{
  BLI_assert(geometries.size() == transforms.size());

  std::unique_ptr<bke::Instances> instances = std::make_unique<bke::Instances>();
  instances->resize(geometries.size());
  transforms.materialize(instances->transforms_for_write());
  MutableSpan<int> handles = instances->reference_handles_for_write();
  for (const int i : geometries.index_range()) {
    handles[i] = instances->add_new_reference(bke::InstanceReference{geometries[i]});
  }

  geometry::RealizeInstancesOptions options;
  options.keep_original_ids = true;
  options.realize_instance_attributes = false;
  return realize_instances(bke::GeometrySet::from_instances(instances.release()), options)
      .geometry;
}
static bke::GeometrySet join_geometries_with_transform(Span<bke::GeometrySet> geometries,
                                                       const float4x4 &transform)
{
  return join_geometries_with_transforms(
      geometries, VArray<float4x4>::from_single(transform, geometries.size()));
}

static wmOperatorStatus grease_pencil_copy_strokes_exec(bContext *C, wmOperator *op)
{
  using bke::greasepencil::Layer;

  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  const bke::AttrDomain selection_domain = ED_grease_pencil_edit_selection_domain_get(
      scene->toolsettings);

  Clipboard &clipboard = ensure_grease_pencil_clipboard();

  int num_elements_copied = 0;
  Map<const Layer *, Vector<bke::GeometrySet>> copied_curves_per_layer;

  /* Collect all selected strokes/points on all editable layers. */
  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  for (const MutableDrawingInfo &drawing_info : drawings) {
    const bke::CurvesGeometry &curves = drawing_info.drawing.strokes();
    const Layer &layer = grease_pencil.layer(drawing_info.layer_index);

    if (curves.is_empty()) {
      continue;
    }
    if (!ed::curves::has_anything_selected(curves)) {
      continue;
    }

    /* Get a copy of the selected geometry on this layer. */
    IndexMaskMemory memory;
    bke::CurvesGeometry copied_curves;

    if (selection_domain == bke::AttrDomain::Curve) {
      const IndexMask selected_curves = ed::curves::retrieve_selected_curves(curves, memory);
      copied_curves = curves_copy_curve_selection(curves, selected_curves, {});
      num_elements_copied += copied_curves.curves_num();
    }
    else if (selection_domain == bke::AttrDomain::Point) {
      const IndexMask selected_points = ed::curves::retrieve_selected_points(curves, memory);
      copied_curves = geometry::remove_points_and_split(
          curves, selected_points.complement(curves.points_range(), memory));
      num_elements_copied += copied_curves.points_num();
    }

    /* Add the layer selection to the set of copied curves. */
    copied_curves_per_layer.lookup_or_add_default(&layer).append(
        bke::GeometrySet::from_curves(curves_new_nomain(std::move(copied_curves))));
  }

  if (copied_curves_per_layer.is_empty()) {
    clipboard.layers.reinitialize(0);
    return OPERATOR_CANCELLED;
  }

  clipboard.layers.reinitialize(copied_curves_per_layer.size());

  int i = 0;
  for (auto const &[layer, geometries] : copied_curves_per_layer.items()) {
    const float4x4 layer_to_object = layer->to_object_space(*object);
    Clipboard::ClipboardLayer &cliplayer = clipboard.layers[i];

    bke::GeometrySet joined_copied_curves = join_geometries_with_transform(geometries.as_span(),
                                                                           layer_to_object);
    cliplayer.curves = joined_copied_curves.get_curves()->geometry.wrap();
    cliplayer.name = layer->name();
    i++;
  }
  clipboard.object_to_world = object->object_to_world();

  /* Store the session uid of the materials used by the curves in the clipboard. We use the uid to
   * remap the material indices when pasting. */
  clipboard.materials.clear();
  clipboard.materials_in_source_num = grease_pencil.material_array_num;

  const auto is_material_index_used = [&](const int material_index) -> bool {
    for (const Clipboard::ClipboardLayer &layer : clipboard.layers) {
      const bke::AttributeAccessor attributes = layer.curves.attributes();
      const VArraySpan<int> material_indices = *attributes.lookup_or_default<int>(
          "material_index", bke::AttrDomain::Curve, 0);
      if (material_indices.contains(material_index)) {
        return true;
      }
    }
    return false;
  };

  for (const int material_index : IndexRange(grease_pencil.material_array_num)) {
    if (!is_material_index_used(material_index)) {
      continue;
    }
    const Material *material = BKE_object_material_get(object, material_index + 1);
    clipboard.materials.append({material ? material->id.session_uid : 0, material_index});
  }

  /* Report the numbers. */
  if (selection_domain == bke::AttrDomain::Curve) {
    BKE_reportf(op->reports, RPT_INFO, "Copied %d selected curve(s)", num_elements_copied);
  }
  else if (selection_domain == bke::AttrDomain::Point) {
    BKE_reportf(op->reports, RPT_INFO, "Copied %d selected point(s)", num_elements_copied);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_copy(wmOperatorType *ot)
{
  ot->name = "Copy Strokes";
  ot->idname = "GREASE_PENCIL_OT_copy";
  ot->description = "Copy the selected Grease Pencil points or strokes to the internal clipboard";

  ot->exec = grease_pencil_copy_strokes_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER;
}

static IndexRange clipboard_paste_strokes_ex(Main &bmain,
                                             Object &object,
                                             const bke::CurvesGeometry &curves_to_paste,
                                             const float4x4 &object_to_paste_layer,
                                             const float4x4 &clipboard_to_world,
                                             const bool keep_world_transform,
                                             const bool paste_back,
                                             bke::greasepencil::Drawing &drawing)
{
  /* Get a list of all materials in the scene. */
  const Array<int> clipboard_material_remap = ed::greasepencil::clipboard_materials_remap(bmain,
                                                                                          object);

  /* Get the index range of the pasted curves in the target layer. */
  const IndexRange pasted_curves_range = paste_back ? IndexRange(0, curves_to_paste.curves_num()) :
                                                      IndexRange(drawing.strokes().curves_num(),
                                                                 curves_to_paste.curves_num());

  /* Append the geometry from the clipboard to the target layer. */
  Curves *clipboard_id = bke::curves_new_nomain(curves_to_paste);
  Curves *target_id = curves_new_nomain(std::move(drawing.strokes_for_write()));

  const Array<bke::GeometrySet> geometry_sets = {
      bke::GeometrySet::from_curves(paste_back ? clipboard_id : target_id),
      bke::GeometrySet::from_curves(paste_back ? target_id : clipboard_id)};

  const float4x4 transform = object_to_paste_layer *
                             (keep_world_transform ?
                                  object.world_to_object() * clipboard_to_world :
                                  float4x4::identity());
  const Array<float4x4> transforms = paste_back ? Span<float4x4>{transform, float4x4::identity()} :
                                                  Span<float4x4>{float4x4::identity(), transform};
  bke::GeometrySet joined_curves = join_geometries_with_transforms(
      geometry_sets, VArray<float4x4>::from_container(transforms));

  drawing.strokes_for_write() = std::move(joined_curves.get_curves_for_write()->geometry.wrap());

  /* Remap the material indices of the pasted curves to the target object material indices. */
  bke::MutableAttributeAccessor attributes = drawing.strokes_for_write().attributes_for_write();
  bke::SpanAttributeWriter<int> material_indices = attributes.lookup_or_add_for_write_span<int>(
      "material_index", bke::AttrDomain::Curve);
  if (material_indices) {
    for (const int i : pasted_curves_range) {
      material_indices.span[i] = clipboard_material_remap[material_indices.span[i]];
    }
    material_indices.finish();
  }

  drawing.tag_topology_changed();

  return pasted_curves_range;
}

enum class PasteType {
  Active = 0,
  ByLayer = 1,
};

static wmOperatorStatus grease_pencil_paste_strokes_exec(bContext *C, wmOperator *op)
{
  using namespace bke::greasepencil;
  Main *bmain = CTX_data_main(C);
  const Scene &scene = *CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  const bke::AttrDomain selection_domain = ED_grease_pencil_edit_selection_domain_get(
      scene.toolsettings);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  const PasteType type = PasteType(RNA_enum_get(op->ptr, "type"));

  const bool keep_world_transform = RNA_boolean_get(op->ptr, "keep_world_transform");
  const bool paste_on_back = RNA_boolean_get(op->ptr, "paste_back");

  Clipboard &clipboard = ensure_grease_pencil_clipboard();
  if (clipboard.layers.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  /* Make sure everything on the clipboard is selected, in the correct selection domain. */
  threading::parallel_for_each(clipboard.layers, [&](Clipboard::ClipboardLayer &layer) {
    bke::GSpanAttributeWriter selection = ed::curves::ensure_selection_attribute(
        layer.curves, selection_domain, bke::AttrType::Bool);
    selection.finish();
  });

  if (type == PasteType::Active) {
    Layer *active_layer = grease_pencil.get_active_layer();
    if (!active_layer) {
      BKE_report(op->reports, RPT_ERROR, "No active Grease Pencil layer to paste into");
      return OPERATOR_CANCELLED;
    }
    if (!active_layer->is_editable()) {
      BKE_report(op->reports, RPT_ERROR, "Active layer is not editable");
      return OPERATOR_CANCELLED;
    }

    /* Deselect everything from editable drawings. The pasted strokes are the only ones then after
     * the paste. That's convenient for the user. */
    const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(scene, grease_pencil);
    threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
      bke::GSpanAttributeWriter selection_in_target = ed::curves::ensure_selection_attribute(
          info.drawing.strokes_for_write(), selection_domain, bke::AttrType::Bool);
      ed::curves::fill_selection_false(selection_in_target.span);
      selection_in_target.finish();
    });

    const float4x4 object_to_layer = math::invert(active_layer->to_object_space(*object));

    /* Ensure active keyframe. */
    bool inserted_keyframe = false;
    if (!ensure_active_keyframe(scene, grease_pencil, *active_layer, false, inserted_keyframe)) {
      BKE_report(op->reports, RPT_ERROR, "No Grease Pencil frame to draw on");
      return OPERATOR_CANCELLED;
    }

    Vector<MutableDrawingInfo> drawing_infos =
        ed::greasepencil::retrieve_editable_drawings_from_layer(
            scene, grease_pencil, *active_layer);
    for (const MutableDrawingInfo info : drawing_infos) {
      paste_all_strokes_from_clipboard(
          *bmain, *object, object_to_layer, keep_world_transform, paste_on_back, info.drawing);
    }

    if (inserted_keyframe) {
      WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, nullptr);
    }
  }
  else if (type == PasteType::ByLayer) {
    Layer *active_layer = grease_pencil.get_active_layer();
    /* Find layers to paste strokes into. */
    Array<Layer *> layers_to_paste_into(clipboard.layers.size());
    for (const int clip_layer_i : clipboard.layers.index_range()) {
      const Clipboard::ClipboardLayer &layer = clipboard.layers[clip_layer_i];
      bke::greasepencil::TreeNode *node = grease_pencil.find_node_by_name(layer.name);
      const bool found_layer = node && node->is_layer() && node->as_layer().is_editable();
      if (found_layer) {
        layers_to_paste_into[clip_layer_i] = &node->as_layer();
        continue;
      }
      if (active_layer && active_layer->is_editable()) {
        /* Fall back to active layer. */
        BKE_report(
            op->reports, RPT_WARNING, "Couldn't find matching layer, pasting into active layer");
        layers_to_paste_into[clip_layer_i] = active_layer;
        continue;
      }

      if (!active_layer) {
        BKE_report(op->reports, RPT_ERROR, "No active Grease Pencil layer to paste into");
      }
      if (!active_layer->is_editable()) {
        BKE_report(op->reports, RPT_ERROR, "Active layer is not editable");
      }
      return OPERATOR_CANCELLED;
    }

    /* Deselect everything from editable drawings. The pasted strokes are the only ones then after
     * the paste. That's convenient for the user. */
    const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(scene, grease_pencil);
    threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
      bke::GSpanAttributeWriter selection_in_target = ed::curves::ensure_selection_attribute(
          info.drawing.strokes_for_write(), selection_domain, bke::AttrType::Bool);
      ed::curves::fill_selection_false(selection_in_target.span);
      selection_in_target.finish();
    });

    for (const int clip_layer_i : clipboard.layers.index_range()) {
      const Clipboard::ClipboardLayer &clip_layer = clipboard.layers[clip_layer_i];
      const bke::CurvesGeometry &curves_to_paste = clip_layer.curves;

      BLI_assert(layers_to_paste_into[clip_layer_i] != nullptr);
      Layer &paste_layer = *layers_to_paste_into[clip_layer_i];
      const float4x4 object_to_paste_layer = math::invert(paste_layer.to_object_space(*object));

      /* Ensure active keyframe. */
      bool inserted_keyframe = false;
      if (!ensure_active_keyframe(scene, grease_pencil, paste_layer, false, inserted_keyframe)) {
        BKE_report(op->reports, RPT_ERROR, "No Grease Pencil frame to draw on");
        return OPERATOR_CANCELLED;
      }

      Vector<MutableDrawingInfo> drawing_infos =
          ed::greasepencil::retrieve_editable_drawings_from_layer(
              scene, grease_pencil, paste_layer);
      for (const MutableDrawingInfo info : drawing_infos) {
        clipboard_paste_strokes_ex(*bmain,
                                   *object,
                                   curves_to_paste,
                                   object_to_paste_layer,
                                   clipboard.object_to_world,
                                   keep_world_transform,
                                   paste_on_back,
                                   info.drawing);
      }

      if (inserted_keyframe) {
        WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, nullptr);
      }
    }
  }
  else {
    BLI_assert_unreachable();
  }

  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);

  return OPERATOR_FINISHED;
}

static bool grease_pencil_paste_strokes_poll(bContext *C)
{
  if (!editable_grease_pencil_poll(C)) {
    return false;
  }

  std::scoped_lock lock(grease_pencil_clipboard_lock);
  /* Check for curves in the Grease Pencil clipboard. */
  return (grease_pencil_clipboard && grease_pencil_clipboard->layers.size() > 0);
}

static void GREASE_PENCIL_OT_paste(wmOperatorType *ot)
{
  PropertyRNA *prop;

  static const EnumPropertyItem rna_paste_items[] = {
      {int(PasteType::Active), "ACTIVE", 0, "Paste to Active", ""},
      {int(PasteType::ByLayer), "LAYER", 0, "Paste by Layer", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  ot->name = "Paste Strokes";
  ot->idname = "GREASE_PENCIL_OT_paste";
  ot->description =
      "Paste Grease Pencil points or strokes from the internal clipboard to the active layer";

  ot->exec = grease_pencil_paste_strokes_exec;
  ot->poll = grease_pencil_paste_strokes_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna, "type", rna_paste_items, int(PasteType::Active), "Type", "");

  prop = RNA_def_boolean(
      ot->srna, "paste_back", false, "Paste on Back", "Add pasted strokes behind all strokes");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna,
                         "keep_world_transform",
                         false,
                         "Keep World Transform",
                         "Keep the world transform of strokes from the clipboard unchanged");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

IndexRange paste_all_strokes_from_clipboard(Main &bmain,
                                            Object &object,
                                            const float4x4 &object_to_paste_layer,
                                            const bool keep_world_transform,
                                            const bool paste_back,
                                            bke::greasepencil::Drawing &drawing)
{
  Clipboard &clipboard = ensure_grease_pencil_clipboard();
  if (clipboard.layers.is_empty()) {
    return {};
  }

  Vector<bke::GeometrySet> geometries_to_join;
  for (Clipboard::ClipboardLayer &layer : clipboard.layers) {
    geometries_to_join.append(bke::GeometrySet::from_curves(curves_new_nomain(layer.curves)));
  }
  bke::GeometrySet joined_clipboard_set = geometry::join_geometries(geometries_to_join.as_span(),
                                                                    {});
  BLI_assert(joined_clipboard_set.has_curves());
  const bke::CurvesGeometry &joined_clipboard_curves =
      joined_clipboard_set.get_curves()->geometry.wrap();

  return clipboard_paste_strokes_ex(bmain,
                                    object,
                                    joined_clipboard_curves,
                                    object_to_paste_layer,
                                    clipboard.object_to_world,
                                    keep_world_transform,
                                    paste_back,
                                    drawing);
}

/* -------------------------------------------------------------------- */
/** \name Merge Stroke Operator
 * \{ */
static wmOperatorStatus grease_pencil_stroke_merge_by_distance_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  const float threshold = RNA_float_get(op->ptr, "threshold");
  const bool use_unselected = RNA_boolean_get(op->ptr, "use_unselected");

  std::atomic<bool> changed = false;

  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    bke::greasepencil::Drawing &drawing = info.drawing;
    IndexMaskMemory memory;
    const IndexMask points = use_unselected ?
                                 ed::greasepencil::retrieve_editable_points(
                                     *object, drawing, info.layer_index, memory) :
                                 ed::greasepencil::retrieve_editable_and_selected_points(
                                     *object, info.drawing, info.layer_index, memory);
    if (points.is_empty()) {
      return;
    }
    drawing.strokes_for_write() = curves_merge_by_distance(
        drawing.strokes(), threshold, points, {});
    drawing.tag_topology_changed();
    changed.store(true, std::memory_order_relaxed);
  });
  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }
  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_stroke_merge_by_distance(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Merge by Distance";
  ot->idname = "GREASE_PENCIL_OT_stroke_merge_by_distance";
  ot->description = "Merge points by distance";

  ot->exec = grease_pencil_stroke_merge_by_distance_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  prop = RNA_def_float(ot->srna, "threshold", 0.001f, 0.0f, 100.0f, "Threshold", "", 0.0f, 100.0f);
  /* Avoid re-using last var. */
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna,
                         "use_unselected",
                         false,
                         "Unselected",
                         "Use whole stroke, not only selected points");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Extrude Operator
 * \{ */

static bke::CurvesGeometry extrude_grease_pencil_curves(const bke::CurvesGeometry &src,
                                                        const IndexMask &points_to_extrude)
{
  const OffsetIndices<int> points_by_curve = src.points_by_curve();

  const int old_curves_num = src.curves_num();
  const int old_points_num = src.points_num();

  Vector<int> dst_to_src_points(old_points_num);
  array_utils::fill_index_range(dst_to_src_points.as_mutable_span());

  Vector<int> dst_to_src_curves(old_curves_num);
  array_utils::fill_index_range(dst_to_src_curves.as_mutable_span());

  Vector<bool> dst_selected(old_points_num, false);

  Vector<int> dst_curve_counts(old_curves_num);
  offset_indices::copy_group_sizes(
      points_by_curve, src.curves_range(), dst_curve_counts.as_mutable_span());

  const VArray<bool> &src_cyclic = src.cyclic();

  /* Point offset keeps track of the points inserted. */
  int point_offset = 0;
  for (const int curve_index : src.curves_range()) {
    const IndexRange curve_points = points_by_curve[curve_index];
    const IndexMask curve_points_to_extrude = points_to_extrude.slice_content(curve_points);
    const bool curve_cyclic = src_cyclic[curve_index];

    curve_points_to_extrude.foreach_index([&](const int src_point_index) {
      if (!curve_cyclic && (src_point_index == curve_points.first())) {
        /* Start-point extruded, we insert a new point at the beginning of the curve.
         * NOTE: all points of a cyclic curve behave like an inner-point. */
        dst_to_src_points.insert(src_point_index + point_offset, src_point_index);
        dst_selected.insert(src_point_index + point_offset, true);
        ++dst_curve_counts[curve_index];
        ++point_offset;
        return;
      }
      if (!curve_cyclic && (src_point_index == curve_points.last())) {
        /* End-point extruded, we insert a new point at the end of the curve.
         * NOTE: all points of a cyclic curve behave like an inner-point. */
        dst_to_src_points.insert(src_point_index + point_offset + 1, src_point_index);
        dst_selected.insert(src_point_index + point_offset + 1, true);
        ++dst_curve_counts[curve_index];
        ++point_offset;
        return;
      }

      /* Inner-point extruded: we create a new curve made of two points located at the same
       * position. Only one of them is selected so that the other one remains stuck to the curve.
       */
      dst_to_src_points.append(src_point_index);
      dst_selected.append(false);
      dst_to_src_points.append(src_point_index);
      dst_selected.append(true);
      dst_to_src_curves.append(curve_index);
      dst_curve_counts.append(2);
    });
  }

  const int new_points_num = dst_to_src_points.size();
  const int new_curves_num = dst_to_src_curves.size();

  bke::CurvesGeometry dst(new_points_num, new_curves_num);
  BKE_defgroup_copy_list(&dst.vertex_group_names, &src.vertex_group_names);

  /* Setup curve offsets, based on the number of points in each curve. */
  MutableSpan<int> new_curve_offsets = dst.offsets_for_write();
  array_utils::copy(dst_curve_counts.as_span(), new_curve_offsets.drop_back(1));
  offset_indices::accumulate_counts_to_offsets(new_curve_offsets);

  /* Attributes. */
  const bke::AttributeAccessor src_attributes = src.attributes();
  bke::MutableAttributeAccessor dst_attributes = dst.attributes_for_write();

  /* Selection attribute. */
  /* Copy the value of control point selections to all selection attributes.
   *
   * This will lead to the extruded control point always having both handles selected, if it's a
   * bezier type stroke. This is to circumvent the issue of source curves handles not being
   * deselected when the user extrudes a bezier control point with both handles selected. */
  for (const StringRef selection_attribute_name :
       ed::curves::get_curves_selection_attribute_names(src))
  {
    bke::GSpanAttributeWriter selection = ed::curves::ensure_selection_attribute(
        dst, bke::AttrDomain::Point, bke::AttrType::Bool, selection_attribute_name);
    selection.span.copy_from(dst_selected.as_span());
    selection.finish();
  }

  bke::gather_attributes(src_attributes,
                         bke::AttrDomain::Curve,
                         bke::AttrDomain::Curve,
                         {},
                         dst_to_src_curves,
                         dst_attributes);

  /* Cyclic attribute : newly created curves cannot be cyclic. */
  dst.cyclic_for_write().drop_front(old_curves_num).fill(false);

  bke::gather_attributes(src_attributes,
                         bke::AttrDomain::Point,
                         bke::AttrDomain::Point,
                         bke::attribute_filter_from_skip_ref(
                             {".selection", ".selection_handle_left", ".selection_handle_right"}),
                         dst_to_src_points,
                         dst_attributes);

  dst.update_curve_types();
  if (src.nurbs_has_custom_knots()) {
    IndexMaskMemory memory;
    const VArray<int8_t> curve_types = src.curve_types();
    const VArray<int8_t> knot_modes = dst.nurbs_knots_modes();
    const OffsetIndices<int> dst_points_by_curve = dst.points_by_curve();
    const IndexMask include_curves = IndexMask::from_predicate(
        src.curves_range(), GrainSize(512), memory, [&](const int64_t curve_index) {
          return curve_types[curve_index] == CURVE_TYPE_NURBS &&
                 knot_modes[curve_index] == NURBS_KNOT_MODE_CUSTOM &&
                 points_by_curve[curve_index].size() == dst_points_by_curve[curve_index].size();
        });
    bke::curves::nurbs::update_custom_knot_modes(
        include_curves.complement(dst.curves_range(), memory),
        NURBS_KNOT_MODE_ENDPOINT,
        NURBS_KNOT_MODE_NORMAL,
        dst);
    bke::curves::nurbs::gather_custom_knots(src, include_curves, 0, dst);
  }
  return dst;
}

static wmOperatorStatus grease_pencil_extrude_exec(bContext *C, wmOperator * /*op*/)
{
  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  View3D *v3d = CTX_wm_view3d(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  std::atomic<bool> changed = false;
  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    IndexMaskMemory memory;
    const IndexMask points_to_extrude =
        ed::greasepencil::retrieve_editable_and_all_selected_points(
            *object, info.drawing, info.layer_index, v3d->overlay.handle_display, memory);
    if (points_to_extrude.is_empty()) {
      return;
    }

    const bke::CurvesGeometry &curves = info.drawing.strokes();
    info.drawing.strokes_for_write() = extrude_grease_pencil_curves(curves, points_to_extrude);

    info.drawing.tag_topology_changed();
    changed.store(true, std::memory_order_relaxed);
  });

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_extrude(wmOperatorType *ot)
{
  ot->name = "Extrude Stroke Points";
  ot->idname = "GREASE_PENCIL_OT_extrude";
  ot->description = "Extrude the selected points";

  ot->exec = grease_pencil_extrude_exec;
  ot->poll = editable_grease_pencil_point_selection_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Reproject Strokes Operator
 * \{ */

/* Determine how much the radius needs to be scaled to look the same from the view. */
static float calculate_radius_projection_factor(const RegionView3D *rv3d,
                                                const float3 &old_pos,
                                                const float3 &new_pos)
{
  /* Don't scale the radius when the view is orthographic. */
  if (!rv3d->is_persp) {
    return 1.0f;
  }

  const float3 view_center = float3(rv3d->viewinv[3]);
  return math::length(new_pos - view_center) / math::length(old_pos - view_center);
}

static wmOperatorStatus grease_pencil_reproject_exec(bContext *C, wmOperator *op)
{
  Scene &scene = *CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  View3D *v3d = CTX_wm_view3d(C);
  ARegion *region = CTX_wm_region(C);

  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

  const ReprojectMode mode = ReprojectMode(RNA_enum_get(op->ptr, "type"));
  const bool keep_original = RNA_boolean_get(op->ptr, "keep_original");

  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  const float offset = RNA_float_get(op->ptr, "offset");

  /* Init snap context for geometry projection. */
  threading::EnumerableThreadSpecific<transform::SnapObjectContext *> thread_snap_contexts(
      [&]() -> transform::SnapObjectContext * {
        if (mode == ReprojectMode::Surface) {
          return transform::snap_object_context_create(&scene, 0);
        }
        return nullptr;
      });

  const bke::AttrDomain selection_domain = ED_grease_pencil_edit_selection_domain_get(
      scene.toolsettings);

  const int oldframe = int(DEG_get_ctime(depsgraph));
  if (keep_original) {
    const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(scene, grease_pencil);
    threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
      IndexMaskMemory memory;
      const IndexMask elements = retrieve_editable_and_selected_elements(
          *object, info.drawing, info.layer_index, selection_domain, memory);
      if (elements.is_empty()) {
        return;
      }

      bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
      if (selection_domain == bke::AttrDomain::Curve) {
        curves::duplicate_curves(curves, elements);
      }
      else if (selection_domain == bke::AttrDomain::Point) {
        curves::duplicate_points(curves, elements);
      }
      info.drawing.tag_topology_changed();
    });
  }

  /* TODO: This can probably be optimized further for the non-Surface projection use case by
   * considering all drawings for the parallel loop instead of having to partition by frame number.
   */
  std::atomic<bool> changed = false;
  Array<Vector<MutableDrawingInfo>> drawings_per_frame =
      retrieve_editable_drawings_grouped_per_frame(scene, grease_pencil);
  for (const Span<MutableDrawingInfo> drawings : drawings_per_frame) {
    if (drawings.is_empty()) {
      continue;
    }
    const int current_frame_number = drawings.first().frame_number;

    if (mode == ReprojectMode::Surface) {
      scene.r.cfra = current_frame_number;
      BKE_scene_graph_update_for_newframe(depsgraph);
    }

    threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
      bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
      MutableSpan<float> radii = curves.radius_for_write();

      IndexMaskMemory memory;
      const IndexMask editable_points = retrieve_editable_points(
          *object, info.drawing, info.layer_index, memory);

      const IndexMask bezier_points = bke::curves::curve_type_point_selection(
          curves, CURVE_TYPE_BEZIER, memory);

      for (const StringRef selection_name :
           ed::curves::get_curves_selection_attribute_names(curves))
      {
        const IndexMask selected_points = ed::curves::retrieve_selected_points(
            curves, selection_name, bezier_points, memory);
        const IndexMask points_to_reproject = IndexMask::from_intersection(
            editable_points, selected_points, memory);

        if (points_to_reproject.is_empty()) {
          return;
        }

        MutableSpan<float3> positions = curves.positions_for_write();
        if (selection_name == ".selection_handle_left") {
          positions = curves.handle_positions_left_for_write();
        }
        else if (selection_name == ".selection_handle_right") {
          positions = curves.handle_positions_right_for_write();
        }

        const bke::greasepencil::Layer &layer = grease_pencil.layer(info.layer_index);
        if (mode == ReprojectMode::Surface) {
          const float4x4 layer_space_to_world_space = layer.to_world_space(*object);
          const float4x4 world_space_to_layer_space = math::invert(layer_space_to_world_space);
          points_to_reproject.foreach_index(GrainSize(4096), [&](const int point_i) {
            float3 &position = positions[point_i];
            const float3 world_pos = math::transform_point(layer_space_to_world_space, position);
            float2 screen_co;
            if (ED_view3d_project_float_global(region, world_pos, screen_co, V3D_PROJ_TEST_NOP) !=
                eV3DProjStatus::V3D_PROJ_RET_OK)
            {
              return;
            }

            float3 ray_start, ray_direction;
            if (!ED_view3d_win_to_ray_clipped(
                    depsgraph, region, v3d, screen_co, ray_start, ray_direction, true))
            {
              return;
            }

            float hit_depth = std::numeric_limits<float>::max();
            float3 hit_position(0.0f);
            float3 hit_normal(0.0f);

            transform::SnapObjectParams params{};
            params.snap_target_select = SCE_SNAP_TARGET_ALL;
            transform::SnapObjectContext *snap_context = thread_snap_contexts.local();
            if (transform::snap_object_project_ray(snap_context,
                                                   depsgraph,
                                                   v3d,
                                                   &params,
                                                   ray_start,
                                                   ray_direction,
                                                   &hit_depth,
                                                   hit_position,
                                                   hit_normal))
            {
              /* Apply offset over surface. */
              const float3 new_pos = math::transform_point(
                  world_space_to_layer_space,
                  hit_position + math::normalize(ray_start - hit_position) * offset);

              if (selection_name == ".selection") {
                radii[point_i] *= calculate_radius_projection_factor(rv3d, position, new_pos);
              }
              position = new_pos;
            }
          });
        }
        else {
          const DrawingPlacement drawing_placement(
              scene, *region, *v3d, *object, &layer, mode, offset, nullptr);
          points_to_reproject.foreach_index(GrainSize(4096), [&](const int point_i) {
            const float3 new_pos = drawing_placement.reproject(positions[point_i]);
            if (selection_name == ".selection") {
              radii[point_i] *= calculate_radius_projection_factor(
                  rv3d, positions[point_i], new_pos);
            }
            positions[point_i] = new_pos;
          });
        }

        info.drawing.tag_positions_changed();
        changed.store(true, std::memory_order_relaxed);
      }
    });
  }

  for (transform::SnapObjectContext *snap_context : thread_snap_contexts) {
    if (snap_context != nullptr) {
      transform::snap_object_context_destroy(snap_context);
    }
  }

  if (mode == ReprojectMode::Surface) {
    scene.r.cfra = oldframe;
    BKE_scene_graph_update_for_newframe(depsgraph);
  }

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }

  return OPERATOR_FINISHED;
}

static void grease_pencil_reproject_ui(bContext * /*C*/, wmOperator *op)
{
  uiLayout *layout = op->layout;
  uiLayout *row;

  const ReprojectMode type = ReprojectMode(RNA_enum_get(op->ptr, "type"));

  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);
  row = &layout->row(true);
  row->prop(op->ptr, "type", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  if (type == ReprojectMode::Surface) {
    row = &layout->row(true);
    row->prop(op->ptr, "offset", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
  row = &layout->row(true);
  row->prop(op->ptr, "keep_original", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void GREASE_PENCIL_OT_reproject(wmOperatorType *ot)
{
  static const EnumPropertyItem reproject_type[] = {
      {int(ReprojectMode::Front),
       "FRONT",
       0,
       "Front",
       "Reproject the strokes using the X-Z plane"},
      {int(ReprojectMode::Side), "SIDE", 0, "Side", "Reproject the strokes using the Y-Z plane"},
      {int(ReprojectMode::Top), "TOP", 0, "Top", "Reproject the strokes using the X-Y plane"},
      {int(ReprojectMode::View),
       "VIEW",
       0,
       "View",
       "Reproject the strokes to end up on the same plane, as if drawn from the current "
       "viewpoint "
       "using 'Cursor' Stroke Placement"},
      {int(ReprojectMode::Surface),
       "SURFACE",
       0,
       "Surface",
       "Reproject the strokes on to the scene geometry, as if drawn using 'Surface' placement"},
      {int(ReprojectMode::Cursor),
       "CURSOR",
       0,
       "Cursor",
       "Reproject the strokes using the orientation of 3D cursor"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Reproject Strokes";
  ot->idname = "GREASE_PENCIL_OT_reproject";
  ot->description =
      "Reproject the selected strokes from the current viewpoint as if they had been newly "
      "drawn "
      "(e.g. to fix problems from accidental 3D cursor movement or accidental viewport changes, "
      "or for matching deforming geometry)";

  /* callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = grease_pencil_reproject_exec;
  ot->poll = editable_grease_pencil_with_region_view3d_poll;
  ot->ui = grease_pencil_reproject_ui;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(
      ot->srna, "type", reproject_type, int(ReprojectMode::View), "Projection Type", "");

  PropertyRNA *prop = RNA_def_boolean(
      ot->srna,
      "keep_original",
      false,
      "Keep Original",
      "Keep original strokes and create a copy before reprojecting");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MOVIECLIP);

  RNA_def_float(ot->srna, "offset", 0.0f, 0.0f, 10.0f, "Surface Offset", "", 0.0f, 10.0f);
}

/** \} */
/* -------------------------------------------------------------------- */
/** \name Snapping Selection to Grid Operator
 * \{ */

/* Poll callback for snap operators */
/* NOTE: For now, we only allow these in the 3D view, as other editors do not
 *       define a cursor or grid-step which can be used.
 */
static bool grease_pencil_snap_poll(bContext *C)
{
  if (!editable_grease_pencil_poll(C)) {
    return false;
  }

  const ScrArea *area = CTX_wm_area(C);
  if (!(area && area->spacetype == SPACE_VIEW3D)) {
    return false;
  }
  const ARegion *region = CTX_wm_region(C);
  if (!(region && region->regiontype == RGN_TYPE_WINDOW)) {
    return false;
  }

  return true;
}

static wmOperatorStatus grease_pencil_snap_to_grid_exec(bContext *C, wmOperator * /*op*/)
{
  using bke::greasepencil::Layer;

  const Scene &scene = *CTX_data_scene(C);
  Object &object = *CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);
  const View3D &v3d = *CTX_wm_view3d(C);
  const ARegion &region = *CTX_wm_region(C);
  const float grid_size = ED_view3d_grid_view_scale(&scene, &v3d, &region, nullptr);

  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(scene, grease_pencil);
  for (const MutableDrawingInfo &drawing_info : drawings) {
    bke::CurvesGeometry &curves = drawing_info.drawing.strokes_for_write();
    if (curves.is_empty()) {
      continue;
    }
    if (!ed::curves::has_anything_selected(curves)) {
      continue;
    }

    IndexMaskMemory memory;
    const IndexMask bezier_points = bke::curves::curve_type_point_selection(
        curves, CURVE_TYPE_BEZIER, memory);

    for (const StringRef selection_name : ed::curves::get_curves_selection_attribute_names(curves))
    {
      const IndexMask selected_points = ed::curves::retrieve_selected_points(
          curves, selection_name, bezier_points, memory);

      const Layer &layer = grease_pencil.layer(drawing_info.layer_index);
      const float4x4 layer_to_world = layer.to_world_space(object);
      const float4x4 world_to_layer = math::invert(layer_to_world);

      MutableSpan<float3> positions = curves.positions_for_write();
      if (selection_name == ".selection_handle_left") {
        positions = curves.handle_positions_left_for_write();
      }
      else if (selection_name == ".selection_handle_right") {
        positions = curves.handle_positions_right_for_write();
      }
      selected_points.foreach_index(GrainSize(4096), [&](const int point_i) {
        const float3 pos_world = math::transform_point(layer_to_world, positions[point_i]);
        const float3 pos_snapped = grid_size * math::floor(pos_world / grid_size + 0.5f);
        positions[point_i] = math::transform_point(world_to_layer, pos_snapped);
      });
    }

    drawing_info.drawing.tag_positions_changed();
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    DEG_id_tag_update(&object.id, ID_RECALC_SYNC_TO_EVAL);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, &grease_pencil);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_snap_to_grid(wmOperatorType *ot)
{
  ot->name = "Snap Selection to Grid";
  ot->idname = "GREASE_PENCIL_OT_snap_to_grid";
  ot->description = "Snap selected points to the nearest grid points";

  ot->exec = grease_pencil_snap_to_grid_exec;
  ot->poll = grease_pencil_snap_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snapping Selection to Cursor Operator
 * \{ */

static wmOperatorStatus grease_pencil_snap_to_cursor_exec(bContext *C, wmOperator *op)
{
  using bke::greasepencil::Layer;

  const Scene &scene = *CTX_data_scene(C);
  Object &object = *CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);
  const bool use_offset = RNA_boolean_get(op->ptr, "use_offset");
  const float3 cursor_world = scene.cursor.location;

  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(scene, grease_pencil);
  for (const MutableDrawingInfo &drawing_info : drawings) {
    bke::CurvesGeometry &curves = drawing_info.drawing.strokes_for_write();
    if (curves.is_empty()) {
      continue;
    }
    if (!ed::curves::has_anything_selected(curves)) {
      continue;
    }

    IndexMaskMemory selected_points_memory;
    const IndexMask selected_points = ed::curves::retrieve_selected_points(curves,
                                                                           selected_points_memory);

    const Layer &layer = grease_pencil.layer(drawing_info.layer_index);
    const float4x4 layer_to_world = layer.to_world_space(object);
    const float4x4 world_to_layer = math::invert(layer_to_world);
    const float3 cursor_layer = math::transform_point(world_to_layer, cursor_world);

    MutableSpan<float3> positions = curves.positions_for_write();
    if (use_offset) {
      const OffsetIndices points_by_curve = curves.points_by_curve();
      IndexMaskMemory selected_curves_memory;
      const IndexMask selected_curves = ed::curves::retrieve_selected_curves(
          curves, selected_curves_memory);

      selected_curves.foreach_index(GrainSize(512), [&](const int curve_i) {
        const IndexRange points = points_by_curve[curve_i];

        /* Offset from first point of the curve. */
        const float3 offset = cursor_layer - positions[points.first()];
        selected_points.slice_content(points).foreach_index(
            GrainSize(4096), [&](const int point_i) { positions[point_i] += offset; });
      });
    }
    else {
      /* Set all selected positions to the cursor location. */
      index_mask::masked_fill(positions, cursor_layer, selected_points);
    }

    curves.calculate_bezier_auto_handles();
    drawing_info.drawing.tag_positions_changed();
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    DEG_id_tag_update(&object.id, ID_RECALC_SYNC_TO_EVAL);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, &grease_pencil);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_snap_to_cursor(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Snap Selection to Cursor";
  ot->idname = "GREASE_PENCIL_OT_snap_to_cursor";
  ot->description = "Snap selected points/strokes to the cursor";

  /* callbacks */
  ot->exec = grease_pencil_snap_to_cursor_exec;
  ot->poll = grease_pencil_snap_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  ot->prop = RNA_def_boolean(ot->srna,
                             "use_offset",
                             true,
                             "With Offset",
                             "Offset the entire stroke instead of selected points only");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snapping Cursor to Selection Operator
 * \{ */

static bool grease_pencil_snap_compute_centroid(const Scene &scene,
                                                const Object &object,
                                                const GreasePencil &grease_pencil,
                                                float3 &r_centroid,
                                                float3 &r_min,
                                                float3 &r_max)
{
  using bke::greasepencil::Layer;

  int num_selected = 0;
  r_centroid = float3(0.0f);
  r_min = float3(std::numeric_limits<float>::max());
  r_max = float3(std::numeric_limits<float>::lowest());

  const Vector<DrawingInfo> drawings = retrieve_visible_drawings(scene, grease_pencil, false);
  for (const DrawingInfo &drawing_info : drawings) {
    const Layer &layer = grease_pencil.layer(drawing_info.layer_index);
    if (layer.is_locked()) {
      continue;
    }
    const bke::CurvesGeometry &curves = drawing_info.drawing.strokes();
    if (curves.is_empty()) {
      continue;
    }
    if (!ed::curves::has_anything_selected(curves)) {
      continue;
    }

    IndexMaskMemory selected_points_memory;
    const IndexMask selected_points = ed::curves::retrieve_selected_points(curves,
                                                                           selected_points_memory);
    const float4x4 layer_to_world = layer.to_world_space(object);

    Span<float3> positions = curves.positions();
    selected_points.foreach_index(GrainSize(4096), [&](const int point_i) {
      const float3 pos_world = math::transform_point(layer_to_world, positions[point_i]);
      r_centroid += pos_world;
      math::min_max(pos_world, r_min, r_max);
    });
    num_selected += selected_points.size();
  }
  if (num_selected == 0) {
    r_min = r_max = float3(0.0f);
    return false;
  }

  r_centroid /= num_selected;
  return true;
}

static wmOperatorStatus grease_pencil_snap_cursor_to_sel_exec(bContext *C, wmOperator * /*op*/)
{
  Scene &scene = *CTX_data_scene(C);
  const Object &object = *CTX_data_active_object(C);
  const GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);
  float3 &cursor = reinterpret_cast<float3 &>(scene.cursor.location);

  float3 centroid, points_min, points_max;
  if (!grease_pencil_snap_compute_centroid(
          scene, object, grease_pencil, centroid, points_min, points_max))
  {
    return OPERATOR_FINISHED;
  }

  switch (scene.toolsettings->transform_pivot_point) {
    case V3D_AROUND_CENTER_BOUNDS:
      cursor = math::midpoint(points_min, points_max);
      break;
    case V3D_AROUND_CENTER_MEDIAN:
    case V3D_AROUND_CURSOR:
    case V3D_AROUND_LOCAL_ORIGINS:
    case V3D_AROUND_ACTIVE:
      cursor = centroid;
      break;
    default:
      BLI_assert_unreachable();
  }

  DEG_id_tag_update(&scene.id, ID_RECALC_SYNC_TO_EVAL);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_snap_cursor_to_selected(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Snap Cursor to Selected Points";
  ot->idname = "GREASE_PENCIL_OT_snap_cursor_to_selected";
  ot->description = "Snap cursor to center of selected points";

  /* callbacks */
  ot->exec = grease_pencil_snap_cursor_to_sel_exec;
  ot->poll = grease_pencil_snap_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static float4x3 expand_4x2_mat(float4x2 strokemat)
{
  float4x3 strokemat4x3 = float4x3(strokemat);

  /*
   * We need the diagonal of ones to start from the bottom right instead top left to properly
   * apply the two matrices.
   *
   * i.e.
   *          # # # #              # # # #
   * We need  # # # #  Instead of  # # # #
   *          0 0 0 1              0 0 1 0
   *
   */
  strokemat4x3[2][2] = 0.0f;
  strokemat4x3[3][2] = 1.0f;

  return strokemat4x3;
}

static wmOperatorStatus grease_pencil_texture_gradient_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  ARegion *region = CTX_wm_region(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  std::atomic<bool> changed = false;
  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    IndexMaskMemory memory;
    const IndexMask strokes = ed::greasepencil::retrieve_editable_and_selected_strokes(
        *object, info.drawing, info.layer_index, memory);
    if (strokes.is_empty()) {
      return;
    }

    const bke::greasepencil::Layer &layer = grease_pencil.layer(info.layer_index);
    const float4x4 layer_space_to_world_space = layer.to_world_space(*object);

    /* Calculate screen space points. */
    const float2 screen_start(RNA_int_get(op->ptr, "xstart"), RNA_int_get(op->ptr, "ystart"));
    const float2 screen_end(RNA_int_get(op->ptr, "xend"), RNA_int_get(op->ptr, "yend"));
    const float2 screen_direction = screen_end - screen_start;
    const float2 screen_tangent = screen_start + float2(-screen_direction[1], screen_direction[0]);

    const bke::CurvesGeometry &curves = info.drawing.strokes();
    const OffsetIndices<int> points_by_curve = curves.points_by_curve();
    const Span<float3> positions = curves.positions();
    const Span<float3> normals = info.drawing.curve_plane_normals();
    const VArray<int> materials = *curves.attributes().lookup_or_default<int>(
        "material_index", bke::AttrDomain::Curve, 0);

    Array<float4x2> texture_matrices(strokes.size());

    strokes.foreach_index([&](const int curve_i, const int pos) {
      const int material_index = materials[curve_i];

      const MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(object,
                                                                           material_index + 1);
      const bool is_radial = gp_style->gradient_type == GP_MATERIAL_GRADIENT_RADIAL;

      const float texture_angle = gp_style->texture_angle;
      const float2 texture_scale = float2(gp_style->texture_scale);
      const float2 texture_offset = float2(gp_style->texture_offset);

      const float2x2 texture_rotation = math::from_rotation<float2x2>(
          math::AngleRadian(texture_angle));

      const float3 point = math::transform_point(layer_space_to_world_space,
                                                 positions[points_by_curve[curve_i].first()]);
      const float3 normal = math::transform_direction(layer_space_to_world_space,
                                                      normals[curve_i]);

      const float4 plane = float4(normal, -math::dot(normal, point));

      float3 start;
      float3 tangent;
      float3 end;
      ED_view3d_win_to_3d_on_plane(region, plane, screen_start, false, start);
      ED_view3d_win_to_3d_on_plane(region, plane, screen_tangent, false, tangent);
      ED_view3d_win_to_3d_on_plane(region, plane, screen_end, false, end);

      const float3 origin = start;
      /* Invert the length by dividing by the length squared. */
      const float3 u_dir = (end - origin) / math::length_squared(end - origin);
      float3 v_dir = math::cross(u_dir, normal);

      /* Flip the texture if need so that it is not mirrored. */
      if (math::dot(tangent - start, v_dir) < 0.0f) {
        v_dir = -v_dir;
      }

      /* Calculate the texture space before the texture offset transformation. */
      const float4x2 base_texture_space = math::transpose(float2x4(
          float4(u_dir, -math::dot(u_dir, origin)), float4(v_dir, -math::dot(v_dir, origin))));

      float3x2 offset_matrix = float3x2::identity();

      if (is_radial) {
        /* Radial gradients are scaled down by a factor of 2 and have the center at 0.5 */
        offset_matrix *= 0.5f;
        offset_matrix[2] += float2(0.5f, 0.5f);
      }

      /* For some reason 0.5 is added to the offset before being rendered, so remove it here. */
      offset_matrix[2] -= float2(0.5f, 0.5f);

      offset_matrix = math::from_scale<float2x2>(texture_scale) * offset_matrix;
      offset_matrix = texture_rotation * offset_matrix;
      offset_matrix[2] -= texture_offset;

      texture_matrices[pos] = (offset_matrix * expand_4x2_mat(base_texture_space)) *
                              layer_space_to_world_space;
    });

    info.drawing.set_texture_matrices(texture_matrices, strokes);

    changed.store(true, std::memory_order_relaxed);
  });

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }

  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus grease_pencil_texture_gradient_modal(bContext *C,
                                                             wmOperator *op,
                                                             const wmEvent *event)
{
  wmOperatorStatus ret = WM_gesture_straightline_modal(C, op, event);

  /* Check for mouse release. */
  if ((ret & OPERATOR_RUNNING_MODAL) != 0 && event->type == LEFTMOUSE && event->val == KM_RELEASE)
  {
    WM_gesture_straightline_cancel(C, op);
    ret &= ~OPERATOR_RUNNING_MODAL;
    ret |= OPERATOR_FINISHED;
  }

  return ret;
}

static wmOperatorStatus grease_pencil_texture_gradient_invoke(bContext *C,
                                                              wmOperator *op,
                                                              const wmEvent *event)
{
  /* Invoke interactive line drawing (representing the gradient) in viewport. */
  const wmOperatorStatus ret = WM_gesture_straightline_invoke(C, op, event);

  if ((ret & OPERATOR_RUNNING_MODAL) != 0) {
    ARegion *region = CTX_wm_region(C);
    if (region->regiontype == RGN_TYPE_WINDOW && event->type == LEFTMOUSE &&
        event->val == KM_PRESS)
    {
      wmGesture *gesture = static_cast<wmGesture *>(op->customdata);
      gesture->is_active = true;
    }
  }

  return ret;
}

static void GREASE_PENCIL_OT_texture_gradient(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Texture Gradient";
  ot->idname = "GREASE_PENCIL_OT_texture_gradient";
  ot->description = "Draw a line to set the fill material gradient for the selected strokes";

  /* API callbacks. */
  ot->invoke = grease_pencil_texture_gradient_invoke;
  ot->modal = grease_pencil_texture_gradient_modal;
  ot->exec = grease_pencil_texture_gradient_exec;
  ot->poll = editable_grease_pencil_with_region_view3d_poll;
  ot->cancel = WM_gesture_straightline_cancel;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_gesture_straightline(ot, WM_CURSOR_EDIT);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Curve Type Operator
 * \{ */

static wmOperatorStatus grease_pencil_set_curve_type_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  const CurveType dst_type = CurveType(RNA_enum_get(op->ptr, "type"));
  const bool use_handles = RNA_boolean_get(op->ptr, "use_handles");

  bool changed = false;
  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
    IndexMaskMemory memory;
    const IndexMask strokes = ed::greasepencil::retrieve_editable_and_selected_strokes(
        *object, info.drawing, info.layer_index, memory);
    if (strokes.is_empty()) {
      return;
    }

    geometry::ConvertCurvesOptions options;
    options.convert_bezier_handles_to_poly_points = use_handles;
    options.convert_bezier_handles_to_catmull_rom_points = use_handles;
    options.keep_bezier_shape_as_nurbs = use_handles;
    options.keep_catmull_rom_shape_as_nurbs = use_handles;

    curves = geometry::convert_curves(curves, strokes, dst_type, {}, options);
    info.drawing.tag_topology_changed();

    changed = true;
  });

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_set_curve_type(wmOperatorType *ot)
{
  ot->name = "Set Curve Type";
  ot->idname = "GREASE_PENCIL_OT_set_curve_type";
  ot->description = "Set type of selected curves";

  ot->invoke = WM_menu_invoke;
  ot->exec = grease_pencil_set_curve_type_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(
      ot->srna, "type", rna_enum_curves_type_items, CURVE_TYPE_POLY, "Type", "Curve type");

  RNA_def_boolean(ot->srna,
                  "use_handles",
                  false,
                  "Handles",
                  "Take handle information into account in the conversion");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Handle Type Operator
 * \{ */

static wmOperatorStatus grease_pencil_set_handle_type_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  using namespace ed::curves;
  const SetHandleType dst_type = SetHandleType(RNA_enum_get(op->ptr, "type"));

  auto new_handle_type = [&](const int8_t handle_type) {
    switch (dst_type) {
      case SetHandleType::Free:
        return int8_t(BEZIER_HANDLE_FREE);
      case SetHandleType::Auto:
        return int8_t(BEZIER_HANDLE_AUTO);
      case SetHandleType::Vector:
        return int8_t(BEZIER_HANDLE_VECTOR);
      case SetHandleType::Align:
        return int8_t(BEZIER_HANDLE_ALIGN);
      case SetHandleType::Toggle:
        return int8_t(handle_type == BEZIER_HANDLE_FREE ? BEZIER_HANDLE_ALIGN :
                                                          BEZIER_HANDLE_FREE);
    }
    BLI_assert_unreachable();
    return int8_t(0);
  };

  bool changed = false;
  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
    if (!curves.has_curve_with_type(CURVE_TYPE_BEZIER)) {
      return;
    }
    IndexMaskMemory memory;
    const IndexMask editable_strokes = ed::greasepencil::retrieve_editable_strokes(
        *object, info.drawing, info.layer_index, memory);
    const IndexMask bezier_curves = curves.indices_for_curve_type(
        CURVE_TYPE_BEZIER, editable_strokes, memory);

    const bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
    const VArraySpan<bool> selection = *attributes.lookup_or_default<bool>(
        ".selection", bke::AttrDomain::Point, true);
    const VArraySpan<bool> selection_left = *attributes.lookup_or_default<bool>(
        ".selection_handle_left", bke::AttrDomain::Point, true);
    const VArraySpan<bool> selection_right = *attributes.lookup_or_default<bool>(
        ".selection_handle_right", bke::AttrDomain::Point, true);

    const OffsetIndices<int> points_by_curve = curves.points_by_curve();
    MutableSpan<int8_t> handle_types_left = curves.handle_types_left_for_write();
    MutableSpan<int8_t> handle_types_right = curves.handle_types_right_for_write();
    bezier_curves.foreach_index(GrainSize(256), [&](const int curve_i) {
      const IndexRange points = points_by_curve[curve_i];
      for (const int point_i : points) {
        if (selection_left[point_i] || selection[point_i]) {
          handle_types_left[point_i] = new_handle_type(handle_types_left[point_i]);
        }
        if (selection_right[point_i] || selection[point_i]) {
          handle_types_right[point_i] = new_handle_type(handle_types_right[point_i]);
        }
      }
    });

    curves.calculate_bezier_auto_handles();
    curves.tag_topology_changed();
    info.drawing.tag_topology_changed();

    changed = true;
  });

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_set_handle_type(wmOperatorType *ot)
{
  ot->name = "Set Handle Type";
  ot->idname = "GREASE_PENCIL_OT_set_handle_type";
  ot->description = "Set the handle type for Bézier curves";

  ot->invoke = WM_menu_invoke;
  ot->exec = grease_pencil_set_handle_type_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna,
                          "type",
                          ed::curves::rna_enum_set_handle_type_items,
                          int(ed::curves::SetHandleType::Auto),
                          "Type",
                          nullptr);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Curve Resolution Operator
 * \{ */

static wmOperatorStatus grease_pencil_set_curve_resolution_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  const int resolution = RNA_int_get(op->ptr, "resolution");

  bool changed = false;
  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
    IndexMaskMemory memory;
    const IndexMask editable_strokes = ed::greasepencil::retrieve_editable_and_selected_strokes(
        *object, info.drawing, info.layer_index, memory);
    if (editable_strokes.is_empty()) {
      return;
    }

    if (curves.is_single_type(CURVE_TYPE_POLY)) {
      return;
    }

    index_mask::masked_fill(curves.resolution_for_write(), resolution, editable_strokes);
    info.drawing.tag_topology_changed();
    changed = true;
  });

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_set_curve_resolution(wmOperatorType *ot)
{
  ot->name = "Set Curve Resolution";
  ot->idname = "GREASE_PENCIL_OT_set_curve_resolution";
  ot->description = "Set resolution of selected curves";

  ot->exec = grease_pencil_set_curve_resolution_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_int(ot->srna,
              "resolution",
              12,
              0,
              10000,
              "Resolution",
              "The resolution to use for each curve segment",
              1,
              64);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Curve Resolution Operator
 * \{ */

static wmOperatorStatus grease_pencil_reset_uvs_exec(bContext *C, wmOperator * /*op*/)
{
  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  bool changed = false;
  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
    bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
    IndexMaskMemory memory;
    const IndexMask editable_strokes = ed::greasepencil::retrieve_editable_and_selected_strokes(
        *object, info.drawing, info.layer_index, memory);
    if (editable_strokes.is_empty()) {
      return;
    }

    if (attributes.contains("uv_rotation")) {
      if (editable_strokes.size() == curves.curves_num()) {
        attributes.remove("uv_rotation");
      }
      else {
        bke::SpanAttributeWriter<float> uv_rotations = attributes.lookup_for_write_span<float>(
            "uv_rotation");
        index_mask::masked_fill(uv_rotations.span, 0.0f, editable_strokes);
        uv_rotations.finish();
      }
    }

    if (attributes.contains("uv_translation")) {
      if (editable_strokes.size() == curves.curves_num()) {
        attributes.remove("uv_translation");
      }
      else {
        bke::SpanAttributeWriter<float2> uv_translations =
            attributes.lookup_for_write_span<float2>("uv_translation");
        index_mask::masked_fill(uv_translations.span, float2(0.0f, 0.0f), editable_strokes);
        uv_translations.finish();
      }
    }

    if (attributes.contains("uv_scale")) {
      if (editable_strokes.size() == curves.curves_num()) {
        attributes.remove("uv_scale");
      }
      else {
        bke::SpanAttributeWriter<float2> uv_scales = attributes.lookup_for_write_span<float2>(
            "uv_scale");
        index_mask::masked_fill(uv_scales.span, float2(1.0f, 1.0f), editable_strokes);
        uv_scales.finish();
      }
    }

    if (attributes.contains("uv_shear")) {
      if (editable_strokes.size() == curves.curves_num()) {
        attributes.remove("uv_shear");
      }
      else {
        bke::SpanAttributeWriter<float> uv_shears = attributes.lookup_for_write_span<float>(
            "uv_shear");
        index_mask::masked_fill(uv_shears.span, 0.0f, editable_strokes);
        uv_shears.finish();
      }
    }

    info.drawing.tag_positions_changed();
    changed = true;
  });

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_reset_uvs(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Reset UVs";
  ot->idname = "GREASE_PENCIL_OT_reset_uvs";
  ot->description = "Reset UV transformation to default values";

  /* Callbacks. */
  ot->exec = grease_pencil_reset_uvs_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus grease_pencil_stroke_split_exec(bContext *C, wmOperator * /*op*/)
{
  const Scene &scene = *CTX_data_scene(C);
  Object &object = *CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);
  std::atomic<bool> changed = false;

  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    IndexMaskMemory memory;
    const IndexMask selected_points =
        blender::ed::greasepencil::retrieve_editable_and_selected_points(
            object, info.drawing, info.layer_index, memory);

    if (selected_points.is_empty()) {
      return;
    }

    info.drawing.strokes_for_write() = ed::curves::split_points(info.drawing.strokes(),
                                                                selected_points);
    info.drawing.tag_topology_changed();
    changed.store(true, std::memory_order_relaxed);
  });

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

static void GREASE_PENCIL_OT_stroke_split(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Split stroke";
  ot->idname = "GREASE_PENCIL_OT_stroke_split";
  ot->description = "Split selected points to a new stroke";

  /* Callbacks. */
  ot->exec = grease_pencil_stroke_split_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remove Fill Guide Strokes Operator
 * \{ */

enum class RemoveFillGuidesMode : int8_t { ActiveFrame = 0, AllFrames = 1 };

static wmOperatorStatus grease_pencil_remove_fill_guides_exec(bContext *C, wmOperator *op)
{
  using namespace blender::bke::greasepencil;
  const Scene &scene = *CTX_data_scene(C);
  Object &object = *CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);

  const RemoveFillGuidesMode mode = RemoveFillGuidesMode(RNA_enum_get(op->ptr, "mode"));

  std::atomic<bool> changed = false;
  Vector<MutableDrawingInfo> drawings;
  if (mode == RemoveFillGuidesMode::ActiveFrame) {
    for (const int layer_i : grease_pencil.layers().index_range()) {
      const Layer &layer = grease_pencil.layer(layer_i);
      if (Drawing *drawing = grease_pencil.get_drawing_at(layer, scene.r.cfra)) {
        drawings.append({*drawing, layer_i, scene.r.cfra, 1.0f});
      }
    }
  }
  else if (mode == RemoveFillGuidesMode::AllFrames) {
    for (const int layer_i : grease_pencil.layers().index_range()) {
      const Layer &layer = grease_pencil.layer(layer_i);
      for (const auto [frame_number, frame] : layer.frames().items()) {
        if (Drawing *drawing = grease_pencil.get_drawing_at(layer, frame_number)) {
          drawings.append({*drawing, layer_i, frame_number, 1.0f});
        }
      }
    }
  }
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    if (ed::greasepencil::remove_fill_guides(info.drawing.strokes_for_write())) {
      info.drawing.tag_topology_changed();
      changed.store(true, std::memory_order_relaxed);
    }
  });

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

static void GREASE_PENCIL_OT_remove_fill_guides(wmOperatorType *ot)
{
  static const EnumPropertyItem rna_mode_items[] = {
      {int(RemoveFillGuidesMode::ActiveFrame), "ACTIVE_FRAME", 0, "Active Frame", ""},
      {int(RemoveFillGuidesMode::AllFrames), "ALL_FRAMES", 0, "All Frames", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* Identifiers. */
  ot->name = "Remove Fill Guides";
  ot->idname = "GREASE_PENCIL_OT_remove_fill_guides";
  ot->description = "Remove all the strokes that were created from the fill tool as guides";

  /* Callbacks. */
  ot->exec = grease_pencil_remove_fill_guides_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(
      ot->srna, "mode", rna_mode_items, int(RemoveFillGuidesMode::AllFrames), "Mode", "");
}

/* -------------------------------------------------------------------- */
/** \name Outline Operator
 * \{ */

enum class OutlineMode : int8_t {
  View = 0,
  Front = 1,
  Side = 2,
  Top = 3,
  Cursor = 4,
  Camera = 5,
};

static const EnumPropertyItem prop_outline_modes[] = {
    {int(OutlineMode::View), "VIEW", 0, "View", ""},
    {int(OutlineMode::Front), "FRONT", 0, "Front", ""},
    {int(OutlineMode::Side), "SIDE", 0, "Side", ""},
    {int(OutlineMode::Top), "TOP", 0, "Top", ""},
    {int(OutlineMode::Cursor), "CURSOR", 0, "Cursor", ""},
    {int(OutlineMode::Camera), "CAMERA", 0, "Camera", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static wmOperatorStatus grease_pencil_outline_exec(bContext *C, wmOperator *op)
{
  using bke::greasepencil::Layer;

  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  const float radius = RNA_float_get(op->ptr, "radius");
  const float offset_factor = RNA_float_get(op->ptr, "offset_factor");
  const int corner_subdivisions = RNA_int_get(op->ptr, "corner_subdivisions");
  const float outline_offset = radius * offset_factor;
  const int mat_nr = -1;

  const OutlineMode mode = OutlineMode(RNA_enum_get(op->ptr, "type"));

  float4x4 viewinv = float4x4::identity();
  switch (mode) {
    case OutlineMode::View: {
      RegionView3D *rv3d = CTX_wm_region_view3d(C);
      viewinv = float4x4(rv3d->viewmat);
      break;
    }
    case OutlineMode::Front:
      viewinv = float4x4({1.0f, 0.0f, 0.0f, 0.0f},
                         {0.0f, 0.0f, 1.0f, 0.0f},
                         {0.0f, 1.0f, 0.0f, 0.0f},
                         {0.0f, 0.0f, 0.0f, 1.0f});
      break;
    case OutlineMode::Side:
      viewinv = float4x4({0.0f, 0.0f, 1.0f, 0.0f},
                         {0.0f, 1.0f, 0.0f, 0.0f},
                         {1.0f, 0.0f, 0.0f, 0.0f},
                         {0.0f, 0.0f, 0.0f, 1.0f});
      break;
    case OutlineMode::Top:
      viewinv = float4x4::identity();
      break;
    case OutlineMode::Cursor: {
      viewinv = scene->cursor.matrix<float4x4>();
      break;
    }
    case OutlineMode::Camera:
      viewinv = scene->camera->world_to_object();
      break;
    default:
      BLI_assert_unreachable();
      break;
  }

  bool changed = false;
  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    IndexMaskMemory memory;
    const IndexMask editable_strokes = ed::greasepencil::retrieve_editable_and_selected_strokes(
        *object, info.drawing, info.layer_index, memory);
    if (editable_strokes.is_empty()) {
      return;
    }

    const Layer &layer = grease_pencil.layer(info.layer_index);
    const float4x4 viewmat = viewinv * layer.to_world_space(*object);

    const bke::CurvesGeometry outline = create_curves_outline(info.drawing,
                                                              editable_strokes,
                                                              viewmat,
                                                              corner_subdivisions,
                                                              radius,
                                                              outline_offset,
                                                              mat_nr);

    info.drawing.strokes_for_write().remove_curves(editable_strokes, {});

    /* Join the outline stroke into the drawing. */
    Curves *strokes = bke::curves_new_nomain(std::move(outline));

    Curves *other_curves = bke::curves_new_nomain(std::move(info.drawing.strokes_for_write()));
    const std::array<bke::GeometrySet, 2> geometry_sets = {
        bke::GeometrySet::from_curves(other_curves), bke::GeometrySet::from_curves(strokes)};

    info.drawing.strokes_for_write() = std::move(
        geometry::join_geometries(geometry_sets, {}).get_curves_for_write()->geometry.wrap());

    info.drawing.tag_topology_changed();
    changed = true;
  });

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_outline(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Outline";
  ot->idname = "GREASE_PENCIL_OT_outline";
  ot->description = "Convert selected strokes to perimeter";

  /* Callbacks. */
  ot->exec = grease_pencil_outline_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties */
  ot->prop = RNA_def_enum(
      ot->srna, "type", prop_outline_modes, int(OutlineMode::View), "Projection Mode", "");
  RNA_def_float_distance(ot->srna, "radius", 0.01f, 0.0f, 10.0f, "Radius", "", 0.0f, 10.0f);
  RNA_def_float_factor(
      ot->srna, "offset_factor", -1.0f, -1.0f, 1.0f, "Offset Factor", "", -1.0f, 1.0f);
  RNA_def_int(ot->srna, "corner_subdivisions", 2, 0, 10, "Corner Subdivisions", "", 0, 5);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Convert Curve Type Operator
 * \{ */

static bke::CurvesGeometry fit_poly_curves(bke::CurvesGeometry &curves,
                                           const IndexMask &selection,
                                           const float threshold)
{
  const VArray<float> thresholds = VArray<float>::from_single(threshold, curves.curves_num());
  /* TODO: Detect or manually provide corners. */
  const VArray<bool> corners = VArray<bool>::from_single(false, curves.points_num());
  return geometry::fit_poly_to_bezier_curves(
      curves, selection, thresholds, corners, geometry::FitMethod::Refit, {});
}

static void convert_to_catmull_rom(bke::CurvesGeometry &curves,
                                   const IndexMask &selection,
                                   const float threshold)
{
  if (curves.is_single_type(CURVE_TYPE_CATMULL_ROM)) {
    return;
  }
  IndexMaskMemory memory;
  const IndexMask non_catmull_rom_curves_selection =
      curves.indices_for_curve_type(CURVE_TYPE_CATMULL_ROM, selection, memory)
          .complement(selection, memory);
  if (non_catmull_rom_curves_selection.is_empty()) {
    return;
  }

  curves = geometry::resample_to_evaluated(curves, non_catmull_rom_curves_selection);

  /* To avoid having too many control points, simplify the position attribute based on the
   * threshold. This doesn't replace an actual curve fitting (which would be better), but
   * is a decent approximation for the meantime. */
  const IndexMask points_to_remove = geometry::simplify_curve_attribute(
      curves.positions(),
      non_catmull_rom_curves_selection,
      curves.points_by_curve(),
      curves.cyclic(),
      threshold,
      curves.positions(),
      memory);
  curves.remove_points(points_to_remove, {});

  geometry::ConvertCurvesOptions options;
  options.convert_bezier_handles_to_poly_points = false;
  options.convert_bezier_handles_to_catmull_rom_points = false;
  options.keep_bezier_shape_as_nurbs = true;
  options.keep_catmull_rom_shape_as_nurbs = true;
  curves = geometry::convert_curves(
      curves, non_catmull_rom_curves_selection, CURVE_TYPE_CATMULL_ROM, {}, options);
}

static void convert_to_poly(bke::CurvesGeometry &curves, const IndexMask &selection)
{
  if (curves.is_single_type(CURVE_TYPE_POLY)) {
    return;
  }
  IndexMaskMemory memory;
  const IndexMask non_poly_curves_selection = curves
                                                  .indices_for_curve_type(
                                                      CURVE_TYPE_POLY, selection, memory)
                                                  .complement(selection, memory);
  if (non_poly_curves_selection.is_empty()) {
    return;
  }

  curves = geometry::resample_to_evaluated(curves, non_poly_curves_selection);
}

static void convert_to_bezier(bke::CurvesGeometry &curves,
                              const IndexMask &selection,
                              const float threshold)
{
  if (curves.is_single_type(CURVE_TYPE_BEZIER)) {
    return;
  }
  IndexMaskMemory memory;
  const IndexMask poly_curves_selection = curves.indices_for_curve_type(
      CURVE_TYPE_POLY, selection, memory);
  if (!poly_curves_selection.is_empty()) {
    curves = fit_poly_curves(curves, poly_curves_selection, threshold);
  }

  geometry::ConvertCurvesOptions options;
  options.convert_bezier_handles_to_poly_points = false;
  options.convert_bezier_handles_to_catmull_rom_points = false;
  options.keep_bezier_shape_as_nurbs = true;
  options.keep_catmull_rom_shape_as_nurbs = true;
  curves = geometry::convert_curves(curves, selection, CURVE_TYPE_BEZIER, {}, options);
}

static void convert_to_nurbs(bke::CurvesGeometry &curves,
                             const IndexMask &selection,
                             const float threshold)
{
  if (curves.is_single_type(CURVE_TYPE_NURBS)) {
    return;
  }

  IndexMaskMemory memory;
  const IndexMask poly_curves_selection = curves.indices_for_curve_type(
      CURVE_TYPE_POLY, selection, memory);
  if (!poly_curves_selection.is_empty()) {
    curves = fit_poly_curves(curves, poly_curves_selection, threshold);
  }

  geometry::ConvertCurvesOptions options;
  options.convert_bezier_handles_to_poly_points = false;
  options.convert_bezier_handles_to_catmull_rom_points = false;
  options.keep_bezier_shape_as_nurbs = true;
  options.keep_catmull_rom_shape_as_nurbs = true;
  curves = geometry::convert_curves(curves, selection, CURVE_TYPE_NURBS, {}, options);
}

static wmOperatorStatus grease_pencil_convert_curve_type_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  const CurveType dst_type = CurveType(RNA_enum_get(op->ptr, "type"));
  const float threshold = RNA_float_get(op->ptr, "threshold");

  std::atomic<bool> changed = false;
  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
    IndexMaskMemory memory;
    const IndexMask strokes = ed::greasepencil::retrieve_editable_and_selected_strokes(
        *object, info.drawing, info.layer_index, memory);
    if (strokes.is_empty()) {
      return;
    }

    switch (dst_type) {
      case CURVE_TYPE_CATMULL_ROM:
        convert_to_catmull_rom(curves, strokes, threshold);
        break;
      case CURVE_TYPE_POLY:
        convert_to_poly(curves, strokes);
        break;
      case CURVE_TYPE_BEZIER:
        convert_to_bezier(curves, strokes, threshold);
        break;
      case CURVE_TYPE_NURBS:
        convert_to_nurbs(curves, strokes, threshold);
        break;
    }

    info.drawing.tag_topology_changed();
    changed.store(true, std::memory_order_relaxed);
  });

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }

  return OPERATOR_FINISHED;
}

static void grease_pencil_convert_curve_type_ui(bContext *C, wmOperator *op)
{
  uiLayout *layout = op->layout;
  wmWindowManager *wm = CTX_wm_manager(C);

  PointerRNA ptr = RNA_pointer_create_discrete(&wm->id, op->type->srna, op->properties);

  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);

  layout->prop(&ptr, "type", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  const CurveType dst_type = CurveType(RNA_enum_get(op->ptr, "type"));

  if (dst_type == CURVE_TYPE_POLY) {
    return;
  }

  layout->prop(&ptr, "threshold", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void GREASE_PENCIL_OT_convert_curve_type(wmOperatorType *ot)
{
  ot->name = "Convert Curve Type";
  ot->idname = "GREASE_PENCIL_OT_convert_curve_type";
  ot->description = "Convert type of selected curves";

  ot->invoke = WM_menu_invoke;
  ot->exec = grease_pencil_convert_curve_type_exec;
  ot->poll = editable_grease_pencil_poll;
  ot->ui = grease_pencil_convert_curve_type_ui;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(
      ot->srna, "type", rna_enum_curves_type_items, CURVE_TYPE_POLY, "Type", "");
  RNA_def_property_flag(ot->prop, PROP_SKIP_SAVE);

  PropertyRNA *prop = RNA_def_float(
      ot->srna,
      "threshold",
      0.01f,
      0.0f,
      100.0f,
      "Threshold",
      "The distance that the resulting points are allowed to be within",
      0.0f,
      100.0f);
  RNA_def_property_subtype(prop, PROP_DISTANCE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Corner Type Operator
 * \{ */

enum class CornerType : uint8_t {
  Round = 0,
  Bevel = 1,
  Miter = 2,
};

static const EnumPropertyItem prop_corner_types[] = {
    {int(CornerType::Round), "ROUND", 0, "Round", ""},
    {int(CornerType::Bevel), "FLAT", 0, "Flat", ""},
    {int(CornerType::Miter), "SHARP", 0, "Sharp", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static wmOperatorStatus grease_pencil_set_corner_type_exec(bContext *C, wmOperator *op)
{
  using bke::greasepencil::Layer;

  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  View3D *v3d = CTX_wm_view3d(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  const CornerType corner_type = CornerType(RNA_enum_get(op->ptr, "corner_type"));
  float miter_angle = RNA_float_get(op->ptr, "miter_angle");

  if (corner_type == CornerType::Round) {
    miter_angle = GP_STROKE_MITER_ANGLE_ROUND;
  }
  else if (corner_type == CornerType::Bevel) {
    miter_angle = GP_STROKE_MITER_ANGLE_BEVEL;
  }
  else if (corner_type == CornerType::Miter) {
    /* Prevent the angle from being set to zero, and becoming the `Round` type.*/
    if (miter_angle == 0.0f) {
      miter_angle = DEG2RADF(1.0f);
    }
  }

  std::atomic<bool> changed = false;
  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    IndexMaskMemory memory;
    const IndexMask selection = ed::greasepencil::retrieve_editable_and_all_selected_points(
        *object, info.drawing, info.layer_index, v3d->overlay.handle_display, memory);
    if (selection.is_empty()) {
      return;
    }

    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
    bke::MutableAttributeAccessor attributes = curves.attributes_for_write();

    /* Only create the attribute if we are not storing the default. */
    if (miter_angle == GP_STROKE_MITER_ANGLE_ROUND && !attributes.contains("miter_angle")) {
      return;
    }

    /* Remove the attribute if we are storing all default. */
    if (miter_angle == GP_STROKE_MITER_ANGLE_ROUND && selection == curves.points_range()) {
      attributes.remove("miter_angle");
      changed.store(true, std::memory_order_relaxed);
      return;
    }

    if (bke::SpanAttributeWriter<float> miter_angles =
            attributes.lookup_or_add_for_write_span<float>(
                "miter_angle",
                bke::AttrDomain::Point,
                bke::AttributeInitVArray(
                    VArray<float>::from_single(GP_STROKE_MITER_ANGLE_ROUND, curves.points_num()))))
    {
      index_mask::masked_fill(miter_angles.span, miter_angle, selection);
      miter_angles.finish();
      changed.store(true, std::memory_order_relaxed);
    }
  });

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }

  return OPERATOR_FINISHED;
}

static void grease_pencil_set_corner_type_ui(bContext *C, wmOperator *op)
{
  uiLayout *layout = op->layout;
  wmWindowManager *wm = CTX_wm_manager(C);

  PointerRNA ptr = RNA_pointer_create_discrete(&wm->id, op->type->srna, op->properties);

  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);

  layout->prop(&ptr, "corner_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  const CornerType corner_type = CornerType(RNA_enum_get(op->ptr, "corner_type"));

  if (corner_type != CornerType::Miter) {
    return;
  }

  layout->prop(&ptr, "miter_angle", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void GREASE_PENCIL_OT_set_corner_type(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Set Corner Type";
  ot->idname = "GREASE_PENCIL_OT_set_corner_type";
  ot->description = "Set the corner type of the selected points";

  /* Callbacks. */
  ot->exec = grease_pencil_set_corner_type_exec;
  ot->poll = editable_grease_pencil_poll;
  ot->ui = grease_pencil_set_corner_type_ui;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties */
  ot->prop = RNA_def_enum(
      ot->srna, "corner_type", prop_corner_types, int(CornerType::Miter), "Corner Type", "");
  ot->prop = RNA_def_float_distance(ot->srna,
                                    "miter_angle",
                                    DEG2RADF(45.0f),
                                    0.0f,
                                    M_PI,
                                    "Miter Cut Angle",
                                    "All corners sharper than the Miter angle will be cut flat",
                                    0.0f,
                                    M_PI);
  RNA_def_property_subtype(ot->prop, PROP_ANGLE);
}

/** \} */

}  // namespace blender::ed::greasepencil

void ED_operatortypes_grease_pencil_edit()
{
  using namespace blender::ed::greasepencil;
  WM_operatortype_append(GREASE_PENCIL_OT_stroke_smooth);
  WM_operatortype_append(GREASE_PENCIL_OT_stroke_simplify);
  WM_operatortype_append(GREASE_PENCIL_OT_delete);
  WM_operatortype_append(GREASE_PENCIL_OT_dissolve);
  WM_operatortype_append(GREASE_PENCIL_OT_delete_frame);
  WM_operatortype_append(GREASE_PENCIL_OT_stroke_material_set);
  WM_operatortype_append(GREASE_PENCIL_OT_cyclical_set);
  WM_operatortype_append(GREASE_PENCIL_OT_set_active_material);
  WM_operatortype_append(GREASE_PENCIL_OT_stroke_switch_direction);
  WM_operatortype_append(GREASE_PENCIL_OT_set_start_point);
  WM_operatortype_append(GREASE_PENCIL_OT_set_uniform_thickness);
  WM_operatortype_append(GREASE_PENCIL_OT_set_uniform_opacity);
  WM_operatortype_append(GREASE_PENCIL_OT_caps_set);
  WM_operatortype_append(GREASE_PENCIL_OT_duplicate);
  WM_operatortype_append(GREASE_PENCIL_OT_set_material);
  WM_operatortype_append(GREASE_PENCIL_OT_clean_loose);
  WM_operatortype_append(GREASE_PENCIL_OT_separate);
  WM_operatortype_append(GREASE_PENCIL_OT_stroke_subdivide);
  WM_operatortype_append(GREASE_PENCIL_OT_stroke_reorder);
  WM_operatortype_append(GREASE_PENCIL_OT_move_to_layer);
  WM_operatortype_append(GREASE_PENCIL_OT_copy);
  WM_operatortype_append(GREASE_PENCIL_OT_paste);
  WM_operatortype_append(GREASE_PENCIL_OT_stroke_merge_by_distance);
  WM_operatortype_append(GREASE_PENCIL_OT_stroke_trim);
  WM_operatortype_append(GREASE_PENCIL_OT_extrude);
  WM_operatortype_append(GREASE_PENCIL_OT_reproject);
  WM_operatortype_append(GREASE_PENCIL_OT_snap_to_grid);
  WM_operatortype_append(GREASE_PENCIL_OT_snap_to_cursor);
  WM_operatortype_append(GREASE_PENCIL_OT_snap_cursor_to_selected);
  WM_operatortype_append(GREASE_PENCIL_OT_set_curve_type);
  WM_operatortype_append(GREASE_PENCIL_OT_set_curve_resolution);
  WM_operatortype_append(GREASE_PENCIL_OT_set_handle_type);
  WM_operatortype_append(GREASE_PENCIL_OT_reset_uvs);
  WM_operatortype_append(GREASE_PENCIL_OT_texture_gradient);
  WM_operatortype_append(GREASE_PENCIL_OT_stroke_split);
  WM_operatortype_append(GREASE_PENCIL_OT_remove_fill_guides);
  WM_operatortype_append(GREASE_PENCIL_OT_outline);
  WM_operatortype_append(GREASE_PENCIL_OT_convert_curve_type);
  WM_operatortype_append(GREASE_PENCIL_OT_set_corner_type);
}

/* -------------------------------------------------------------------- */
/** \name Join Objects Operator
 * \{ */

namespace blender::ed::greasepencil {

/* Note: the `duplicate_layer` API would be nicer, but only supports duplicating groups from the
 * same datablock. */
static bke::greasepencil::Layer &copy_layer(GreasePencil &grease_pencil_dst,
                                            bke::greasepencil::LayerGroup &group_dst,
                                            const bke::greasepencil::Layer &layer_src)
{
  using namespace blender::bke::greasepencil;

  Layer &layer_dst = grease_pencil_dst.add_layer(group_dst, layer_src.name());
  BKE_grease_pencil_copy_layer_parameters(layer_src, layer_dst);

  layer_dst.frames_for_write() = layer_src.frames();
  layer_dst.tag_frames_map_changed();

  return layer_dst;
}

static bke::greasepencil::LayerGroup &copy_layer_group_recursive(
    GreasePencil &grease_pencil_dst,
    bke::greasepencil::LayerGroup &parent_dst,
    const bke::greasepencil::LayerGroup &group_src,
    Map<StringRefNull, StringRefNull> &layer_name_map);

static void copy_layer_group_content(GreasePencil &grease_pencil_dst,
                                     bke::greasepencil::LayerGroup &group_dst,
                                     const bke::greasepencil::LayerGroup &group_src,
                                     Map<StringRefNull, StringRefNull> &layer_name_map)
{
  using namespace blender::bke::greasepencil;

  LISTBASE_FOREACH (GreasePencilLayerTreeNode *, child, &group_src.children) {
    switch (child->type) {
      case GP_LAYER_TREE_LEAF: {
        Layer &layer_src = reinterpret_cast<GreasePencilLayer *>(child)->wrap();
        Layer &layer_dst = copy_layer(grease_pencil_dst, group_dst, layer_src);
        layer_name_map.add_new(layer_src.name(), layer_dst.name());
        break;
      }
      case GP_LAYER_TREE_GROUP: {
        LayerGroup &group_src = reinterpret_cast<GreasePencilLayerTreeGroup *>(child)->wrap();
        copy_layer_group_recursive(grease_pencil_dst, group_dst, group_src, layer_name_map);
        break;
      }
    }
  }
}

static bke::greasepencil::LayerGroup &copy_layer_group_recursive(
    GreasePencil &grease_pencil_dst,
    bke::greasepencil::LayerGroup &parent_dst,
    const bke::greasepencil::LayerGroup &group_src,
    Map<StringRefNull, StringRefNull> &layer_name_map)
{
  bke::greasepencil::LayerGroup &group_dst = grease_pencil_dst.add_layer_group(
      parent_dst, group_src.base.name);
  BKE_grease_pencil_copy_layer_group_parameters(group_src, group_dst);

  copy_layer_group_content(grease_pencil_dst, group_dst, group_src, layer_name_map);
  return group_dst;
}

static Array<int> add_materials_to_map(Object &object, VectorSet<Material *> &materials)
{
  BLI_assert(object.type == OB_GREASE_PENCIL);
  Array<int> material_index_map(*BKE_object_material_len_p(&object));
  for (const int i : material_index_map.index_range()) {
    Material *material = BKE_object_material_get(&object, i + 1);
    if (material != nullptr) {
      material_index_map[i] = materials.index_of_or_add(material);
    }
    else {
      material_index_map[i] = 0;
    }
  }
  return material_index_map;
}

static void remap_material_indices(bke::greasepencil::Drawing &drawing,
                                   const Span<int> material_index_map)
{
  bke::CurvesGeometry &curves = drawing.strokes_for_write();
  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  /* Validate material indices and add missing materials. */
  bke::SpanAttributeWriter<int> material_writer = attributes.lookup_or_add_for_write_span<int>(
      "material_index", bke::AttrDomain::Curve);
  threading::parallel_for(curves.curves_range(), 1024, [&](const IndexRange range) {
    for (const int curve_i : range) {
      material_writer.span[curve_i] = material_index_map[material_writer.span[curve_i]];
    }
  });
  material_writer.finish();
}

static Map<StringRefNull, StringRefNull> add_vertex_groups(Object &object,
                                                           GreasePencil &grease_pencil,
                                                           const ListBase &vertex_group_names)
{
  Map<StringRefNull, StringRefNull> vertex_group_map;
  LISTBASE_FOREACH (bDeformGroup *, dg, &vertex_group_names) {
    bDeformGroup *vgroup = static_cast<bDeformGroup *>(MEM_dupallocN(dg));
    BKE_object_defgroup_unique_name(vgroup, &object);
    BLI_addtail(&grease_pencil.vertex_group_names, vgroup);
    vertex_group_map.add_new(dg->name, vgroup->name);
  }
  return vertex_group_map;
}

static void remap_vertex_groups(bke::greasepencil::Drawing &drawing,
                                const Map<StringRefNull, StringRefNull> &vertex_group_map)
{
  LISTBASE_FOREACH (bDeformGroup *, dg, &drawing.strokes_for_write().vertex_group_names) {
    STRNCPY_UTF8(dg->name, vertex_group_map.lookup(dg->name).c_str());
  }

  /* Indices in vertex weights remain valid, they are local to the drawing's vertex groups.
   * Only the names of the groups change. */
}

static bke::AttributeStorage merge_attributes(const bke::AttributeAccessor &a,
                                              const bke::AttributeAccessor &b,
                                              const int dst_size)
{
  Map<std::string, bke::AttrType> new_types;
  const auto add_or_upgrade_types = [&](const bke::AttributeAccessor &attributes) {
    attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
      new_types.add_or_modify(
          iter.name,
          [&](bke::AttrType *value) { *value = iter.data_type; },
          [&](bke::AttrType *value) {
            *value = bke::attribute_data_type_highest_complexity({*value, iter.data_type});
          });
    });
  };
  add_or_upgrade_types(a);
  add_or_upgrade_types(b);
  const int64_t domain_size_a = a.domain_size(bke::AttrDomain::Layer);

  bke::AttributeStorage new_storage;
  for (const auto &[name, type] : new_types.items()) {
    const CPPType &cpp_type = bke::attribute_type_to_cpp_type(type);
    auto new_data = bke::Attribute::ArrayData::from_uninitialized(cpp_type, dst_size);

    const GVArray data_a = *a.lookup_or_default(name, bke::AttrDomain::Layer, type);
    data_a.materialize_to_uninitialized(new_data.data);

    const GVArray data_b = *b.lookup_or_default(name, bke::AttrDomain::Layer, type);
    data_b.materialize_to_uninitialized(
        POINTER_OFFSET(new_data.data, cpp_type.size * domain_size_a));

    new_storage.add(name, bke::AttrDomain::Layer, type, std::move(new_data));
  }

  return new_storage;
}

static void join_object_with_active(Main &bmain,
                                    Object &ob_src,
                                    Object &ob_dst,
                                    VectorSet<Material *> &materials)
{
  using namespace blender::bke::greasepencil;

  /* Skip if the datablock is already used by the active object. */
  if (ob_src.data == ob_dst.data) {
    return;
  }

  BLI_assert(ob_src.type == OB_GREASE_PENCIL);
  BLI_assert(ob_dst.type == OB_GREASE_PENCIL);
  GreasePencil &grease_pencil_src = *static_cast<GreasePencil *>(ob_src.data);
  GreasePencil &grease_pencil_dst = *static_cast<GreasePencil *>(ob_dst.data);
  /* Number of existing layers that don't need to be updated. */
  const int orig_layers_num = grease_pencil_dst.layers().size();

  const Map<StringRefNull, StringRefNull> vertex_group_map = add_vertex_groups(
      ob_dst, grease_pencil_dst, grease_pencil_src.vertex_group_names);
  const Array<int> material_index_map = add_materials_to_map(ob_src, materials);

  /* Concatenate drawing arrays. Existing drawings in dst keep their position, new drawings are
   * mapped to the new index range. */
  const int new_drawing_array_num = grease_pencil_dst.drawing_array_num +
                                    grease_pencil_src.drawing_array_num;
  GreasePencilDrawingBase **new_drawing_array = static_cast<GreasePencilDrawingBase **>(
      MEM_malloc_arrayN(new_drawing_array_num, sizeof(GreasePencilDrawingBase *), __func__));
  MutableSpan<GreasePencilDrawingBase *> new_drawings = {new_drawing_array, new_drawing_array_num};
  const IndexRange new_drawings_dst = IndexRange::from_begin_size(
      0, grease_pencil_dst.drawing_array_num);
  const IndexRange new_drawings_src = IndexRange::from_begin_size(
      grease_pencil_dst.drawing_array_num, grease_pencil_src.drawing_array_num);

  copy_drawing_array(grease_pencil_dst.drawings(), new_drawings.slice(new_drawings_dst));
  copy_drawing_array(grease_pencil_src.drawings(), new_drawings.slice(new_drawings_src));

  /* Free existing drawings array. */
  grease_pencil_dst.resize_drawings(0);
  grease_pencil_dst.drawing_array = new_drawing_array;
  grease_pencil_dst.drawing_array_num = new_drawing_array_num;

  /* Maps original names of source layers to new unique layer names. */
  Map<StringRefNull, StringRefNull> layer_name_map;
  /* Only copy the content of the root group, not the root node itself. */
  copy_layer_group_content(grease_pencil_dst,
                           grease_pencil_dst.root_group(),
                           grease_pencil_src.root_group(),
                           layer_name_map);

  grease_pencil_dst.attribute_storage.wrap() = merge_attributes(grease_pencil_src.attributes(),
                                                                grease_pencil_dst.attributes(),
                                                                grease_pencil_dst.layers().size());

  /* Fix names, indices and transforms to keep relationships valid. */
  for (const int layer_index : grease_pencil_dst.layers().index_range()) {
    Layer &layer = *grease_pencil_dst.layers_for_write()[layer_index];
    const bool is_orig_layer = (layer_index < orig_layers_num);
    const float4x4 old_layer_to_world = (is_orig_layer ? layer.to_world_space(ob_dst) :
                                                         layer.to_world_space(ob_src));

    /* Update newly added layers. */
    if (!is_orig_layer) {
      /* Update name references for masks. */
      LISTBASE_FOREACH (GreasePencilLayerMask *, dst_mask, &layer.masks) {
        const StringRefNull *new_mask_name = layer_name_map.lookup_ptr(dst_mask->layer_name);
        if (new_mask_name) {
          MEM_SAFE_FREE(dst_mask->layer_name);
          dst_mask->layer_name = BLI_strdup(new_mask_name->c_str());
        }
      }
      /* Shift drawing indices to match the new drawings array. */
      for (const int key : layer.frames_for_write().keys()) {
        int &drawing_index = layer.frames_for_write().lookup(key).drawing_index;
        drawing_index = new_drawings_src[drawing_index];
      }
    }

    /* Layer parent object may become invalid. This can be an original layer pointing at the joined
     * object which gets destroyed, or a new layer that points at the target object which is now
     * its owner. */
    if (ELEM(layer.parent, &ob_dst, &ob_src)) {
      layer.parent = nullptr;
    }

    /* Apply relative object transform to new drawings to keep world-space positions unchanged.
     * Be careful where the matrix is computed: changing the parent pointer (above) can affect
     * this! */
    const float4x4 new_layer_to_world = layer.to_world_space(ob_dst);
    for (const int key : layer.frames_for_write().keys()) {
      const int drawing_index = layer.frames_for_write().lookup(key).drawing_index;
      GreasePencilDrawingBase *drawing_base = grease_pencil_dst.drawings()[drawing_index];
      if (drawing_base->type != GP_DRAWING) {
        continue;
      }
      Drawing &drawing = reinterpret_cast<GreasePencilDrawing *>(drawing_base)->wrap();
      bke::CurvesGeometry &curves = drawing.strokes_for_write();
      curves.transform(math::invert(new_layer_to_world) * old_layer_to_world);

      if (!is_orig_layer) {
        remap_vertex_groups(drawing, vertex_group_map);
        remap_material_indices(drawing, material_index_map);
      }
    }
  }

  /* Rename animation paths to layers. */
  BKE_fcurves_main_cb(&bmain, [&](ID *id, FCurve *fcu) {
    if (id == &grease_pencil_src.id && fcu->rna_path && strstr(fcu->rna_path, "layers[")) {
      /* Have to use linear search, the layer name map only contains sub-strings of RNA paths. */
      for (auto [name_src, name_dst] : layer_name_map.items()) {
        if (name_dst != name_src) {
          const char *old_path = fcu->rna_path;
          fcu->rna_path = BKE_animsys_fix_rna_path_rename(
              id, fcu->rna_path, "layers", name_src.c_str(), name_dst.c_str(), 0, 0, false);
          if (old_path != fcu->rna_path) {
            /* Stop after first match. */
            break;
          }
        }
      }
    }
    /* Fix driver targets. */
    if (fcu->driver) {
      LISTBASE_FOREACH (DriverVar *, dvar, &fcu->driver->variables) {
        /* Only change the used targets, since the others will need fixing manually anyway. */
        DRIVER_TARGETS_USED_LOOPER_BEGIN (dvar) {
          if (dtar->id != &grease_pencil_src.id) {
            continue;
          }
          dtar->id = &grease_pencil_dst.id;

          if (dtar->rna_path && strstr(dtar->rna_path, "layers[")) {
            for (auto [name_src, name_dst] : layer_name_map.items()) {
              if (name_dst != name_src) {
                const char *old_path = fcu->rna_path;
                dtar->rna_path = BKE_animsys_fix_rna_path_rename(
                    id, dtar->rna_path, "layers", name_src.c_str(), name_dst.c_str(), 0, 0, false);
                if (old_path != dtar->rna_path) {
                  break;
                }
              }
            }
          }
        }
        DRIVER_TARGETS_LOOPER_END;
      }
    }
  });

  /* Merge animation data of objects and grease pencil datablocks. */
  if (ob_src.adt) {
    if (ob_dst.adt == nullptr) {
      ob_dst.adt = BKE_animdata_copy(&bmain, ob_src.adt, 0);
    }
    else {
      BKE_animdata_merge_copy(&bmain, &ob_dst.id, &ob_src.id, ADT_MERGECOPY_KEEP_DST, false);
    }

    if (ob_dst.adt->action) {
      DEG_id_tag_update(&ob_dst.adt->action->id, ID_RECALC_ANIMATION_NO_FLUSH);
    }
  }
  if (grease_pencil_src.adt) {
    if (grease_pencil_dst.adt == nullptr) {
      grease_pencil_dst.adt = BKE_animdata_copy(&bmain, grease_pencil_src.adt, 0);
    }
    else {
      BKE_animdata_merge_copy(
          &bmain, &grease_pencil_dst.id, &grease_pencil_src.id, ADT_MERGECOPY_KEEP_DST, false);
    }

    if (grease_pencil_dst.adt->action) {
      DEG_id_tag_update(&grease_pencil_dst.adt->action->id, ID_RECALC_ANIMATION_NO_FLUSH);
    }
  }
}

}  // namespace blender::ed::greasepencil

wmOperatorStatus ED_grease_pencil_join_objects_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob_active = CTX_data_active_object(C);

  /* Ensure we're in right mode and that the active object is correct. */
  if (!ob_active || ob_active->type != OB_GREASE_PENCIL) {
    return OPERATOR_CANCELLED;
  }

  bool ok = false;
  CTX_DATA_BEGIN (C, Object *, ob_iter, selected_editable_objects) {
    if (ob_iter == ob_active) {
      ok = true;
      break;
    }
  }
  CTX_DATA_END;
  /* Active object must always selected. */
  if (ok == false) {
    BKE_report(op->reports, RPT_WARNING, "Active object is not a selected Grease Pencil");
    return OPERATOR_CANCELLED;
  }

  Object *ob_dst = ob_active;
  GreasePencil *grease_pencil_dst = static_cast<GreasePencil *>(ob_dst->data);

  blender::VectorSet<Material *> materials;
  blender::Array<int> material_index_map = blender::ed::greasepencil::add_materials_to_map(
      *ob_dst, materials);
  /* Reassign material indices in the original layers, in case materials are deduplicated. */
  for (GreasePencilDrawingBase *drawing_base : grease_pencil_dst->drawings()) {
    if (drawing_base->type != GP_DRAWING) {
      continue;
    }
    blender::bke::greasepencil::Drawing &drawing =
        reinterpret_cast<GreasePencilDrawing *>(drawing_base)->wrap();
    blender::ed::greasepencil::remap_material_indices(drawing, material_index_map);
  }

  /* Loop and join all data. */
  CTX_DATA_BEGIN (C, Object *, ob_iter, selected_editable_objects) {
    if (ob_iter->type != OB_GREASE_PENCIL || ob_iter == ob_active) {
      continue;
    }

    blender::ed::greasepencil::join_object_with_active(*bmain, *ob_iter, *ob_dst, materials);

    /* Free the old object. */
    blender::ed::object::base_free_and_unlink(bmain, scene, ob_iter);
  }
  CTX_DATA_END;

  /* Transfer material pointers. The material indices are updated for each drawing separately. */
  if (!materials.is_empty()) {
    /* Old C API, needs a const_cast but doesn't actually change anything. */
    Material **materials_ptr = const_cast<Material **>(materials.data());
    BKE_object_material_array_assign(
        bmain, DEG_get_original(ob_dst), &materials_ptr, materials.size(), false);
  }

  DEG_id_tag_update(&grease_pencil_dst->id, ID_RECALC_GEOMETRY);
  DEG_relations_tag_update(bmain);

  WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
  WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);

  return OPERATOR_FINISHED;
}

/** \} */
