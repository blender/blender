/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "BLI_vector_set.hh"

#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_grease_pencil.hh"

#include "DNA_object_types.h"

#include "DEG_depsgraph.hh"

#include "ED_curves.hh"
#include "ED_grease_pencil.hh"
#include "ED_screen.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "WM_api.hh"

namespace blender::ed::greasepencil {

static int select_all_exec(bContext *C, wmOperator *op)
{
  int action = RNA_enum_get(op->ptr, "action");
  Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  bke::AttrDomain selection_domain = ED_grease_pencil_selection_domain_get(scene->toolsettings);

  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    IndexMaskMemory memory;
    const IndexMask selectable_elements = retrieve_editable_elements(
        *object, info, selection_domain, memory);
    if (selectable_elements.is_empty()) {
      return;
    }
    blender::ed::curves::select_all(
        info.drawing.strokes_for_write(), selectable_elements, selection_domain, action);
  });

  /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
   * attribute for now. */
  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_select_all(wmOperatorType *ot)
{
  ot->name = "(De)select All Strokes";
  ot->idname = "GREASE_PENCIL_OT_select_all";
  ot->description = "(De)select all visible strokes";

  ot->exec = select_all_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_select_all(ot);
}

static int select_more_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    IndexMaskMemory memory;
    const IndexMask selectable_strokes = ed::greasepencil::retrieve_editable_strokes(
        *object, info.drawing, info.layer_index, memory);
    if (selectable_strokes.is_empty()) {
      return;
    }
    blender::ed::curves::select_adjacent(
        info.drawing.strokes_for_write(), selectable_strokes, false);
  });

  /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
   * attribute for now. */
  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_select_more(wmOperatorType *ot)
{
  ot->name = "Select More";
  ot->idname = "GREASE_PENCIL_OT_select_more";
  ot->description = "Grow the selection by one point";

  ot->exec = select_more_exec;
  ot->poll = editable_grease_pencil_point_selection_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int select_less_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    IndexMaskMemory memory;
    const IndexMask selectable_strokes = ed::greasepencil::retrieve_editable_strokes(
        *object, info.drawing, info.layer_index, memory);
    if (selectable_strokes.is_empty()) {
      return;
    }
    blender::ed::curves::select_adjacent(
        info.drawing.strokes_for_write(), selectable_strokes, true);
  });

  /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
   * attribute for now. */
  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_select_less(wmOperatorType *ot)
{
  ot->name = "Select Less";
  ot->idname = "GREASE_PENCIL_OT_select_less";
  ot->description = "Shrink the selection by one point";

  ot->exec = select_less_exec;
  ot->poll = editable_grease_pencil_point_selection_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int select_linked_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    IndexMaskMemory memory;
    const IndexMask selectable_strokes = ed::greasepencil::retrieve_editable_strokes(
        *object, info.drawing, info.layer_index, memory);
    if (selectable_strokes.is_empty()) {
      return;
    }
    blender::ed::curves::select_linked(info.drawing.strokes_for_write(), selectable_strokes);
  });

  /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
   * attribute for now. */
  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_select_linked(wmOperatorType *ot)
{
  ot->name = "Select Linked";
  ot->idname = "GREASE_PENCIL_OT_select_linked";
  ot->description = "Select all points in curves with any point selection";

  ot->exec = select_linked_exec;
  ot->poll = editable_grease_pencil_point_selection_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int select_random_exec(bContext *C, wmOperator *op)
{
  using namespace blender;
  const float ratio = RNA_float_get(op->ptr, "ratio");
  const int seed = WM_operator_properties_select_random_seed_increment_get(op);
  Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  bke::AttrDomain selection_domain = ED_grease_pencil_selection_domain_get(scene->toolsettings);

  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();

    IndexMaskMemory memory;
    const IndexMask selectable_elements = retrieve_editable_elements(
        *object, info, selection_domain, memory);
    if (selectable_elements.is_empty()) {
      return;
    }

    const IndexMask random_elements = ed::curves::random_mask(
        curves,
        selectable_elements,
        selection_domain,
        blender::get_default_hash<int>(seed, info.layer_index),
        ratio,
        memory);

    const bool was_anything_selected = ed::curves::has_anything_selected(curves,
                                                                         selectable_elements);
    bke::GSpanAttributeWriter selection = ed::curves::ensure_selection_attribute(
        curves, selection_domain, CD_PROP_BOOL);
    if (!was_anything_selected) {
      curves::fill_selection_true(selection.span, selectable_elements);
    }

    curves::fill_selection_false(selection.span, random_elements);
    selection.finish();
  });

  /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
   * attribute for now. */
  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_select_random(wmOperatorType *ot)
{
  ot->name = "Select Random";
  ot->idname = "GREASE_PENCIL_OT_select_random";
  ot->description = "Selects random points from the current strokes selection";

  ot->exec = select_random_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_select_random(ot);
}

static int select_alternate_exec(bContext *C, wmOperator *op)
{
  const bool deselect_ends = RNA_boolean_get(op->ptr, "deselect_ends");
  Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    blender::ed::curves::select_alternate(info.drawing.strokes_for_write(), deselect_ends);
  });

  /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
   * attribute for now. */
  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_select_alternate(wmOperatorType *ot)
{
  ot->name = "Select Alternate";
  ot->idname = "GREASE_PENCIL_OT_select_alternate";
  ot->description = "Select alternated points in strokes with already selected points";

  ot->exec = select_alternate_exec;
  ot->poll = editable_grease_pencil_point_selection_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna,
                  "deselect_ends",
                  false,
                  "Deselect Ends",
                  "(De)select the first and last point of each stroke");
}

static int select_ends_exec(bContext *C, wmOperator *op)
{
  const int amount_start = RNA_int_get(op->ptr, "amount_start");
  const int amount_end = RNA_int_get(op->ptr, "amount_end");
  Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();

    IndexMaskMemory memory;
    const IndexMask selectable_strokes = ed::greasepencil::retrieve_editable_strokes(
        *object, info.drawing, info.layer_index, memory);
    if (selectable_strokes.is_empty()) {
      return;
    }
    const IndexMask inverted_end_points_mask = ed::curves::end_points(
        curves, selectable_strokes, amount_start, amount_end, true, memory);

    const IndexMask selectable_points = ed::greasepencil::retrieve_editable_points(
        *object, info.drawing, info.layer_index, memory);
    const bool was_anything_selected = ed::curves::has_anything_selected(curves,
                                                                         selectable_points);
    bke::GSpanAttributeWriter selection = ed::curves::ensure_selection_attribute(
        curves, bke::AttrDomain::Point, CD_PROP_BOOL);
    if (!was_anything_selected) {
      ed::curves::fill_selection_true(selection.span, selectable_points);
    }

    if (selection.span.type().is<bool>()) {
      index_mask::masked_fill(selection.span.typed<bool>(), false, inverted_end_points_mask);
    }
    if (selection.span.type().is<float>()) {
      index_mask::masked_fill(selection.span.typed<float>(), 0.0f, inverted_end_points_mask);
    }
    selection.finish();
  });

  /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
   * attribute for now. */
  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_select_ends(wmOperatorType *ot)
{
  ot->name = "Select Ends";
  ot->idname = "GREASE_PENCIL_OT_select_ends";
  ot->description = "Select end points of strokes";

  ot->exec = select_ends_exec;
  ot->poll = editable_grease_pencil_point_selection_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_int(ot->srna,
              "amount_start",
              0,
              0,
              INT32_MAX,
              "Amount Start",
              "Number of points to select from the start",
              0,
              INT32_MAX);
  RNA_def_int(ot->srna,
              "amount_end",
              1,
              0,
              INT32_MAX,
              "Amount End",
              "Number of points to select from the end",
              0,
              INT32_MAX);
}

static int select_set_mode_exec(bContext *C, wmOperator *op)
{
  using namespace blender::bke::greasepencil;

  /* Set new selection mode. */
  const int mode_new = RNA_enum_get(op->ptr, "mode");
  ToolSettings *ts = CTX_data_tool_settings(C);

  bool changed = (mode_new != ts->gpencil_selectmode_edit);
  ts->gpencil_selectmode_edit = mode_new;

  /* Convert all drawings of the active GP to the new selection domain. */
  const bke::AttrDomain domain = ED_grease_pencil_selection_domain_get(ts);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  Span<GreasePencilDrawingBase *> drawings = grease_pencil.drawings();

  for (const int index : drawings.index_range()) {
    GreasePencilDrawingBase *drawing_base = drawings[index];
    if (drawing_base->type != GP_DRAWING) {
      continue;
    }

    GreasePencilDrawing *drawing = reinterpret_cast<GreasePencilDrawing *>(drawing_base);
    bke::CurvesGeometry &curves = drawing->wrap().strokes_for_write();
    if (curves.points_num() == 0) {
      continue;
    }

    /* Skip curve when the selection domain already matches, or when there is no selection
     * at all. */
    bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
    const std::optional<bke::AttributeMetaData> meta_data = attributes.lookup_meta_data(
        ".selection");
    if ((!meta_data) || (meta_data->domain == domain)) {
      continue;
    }

    /* When the new selection domain is 'curve', ensure all curves with a point selection
     * are selected. */
    if (domain == bke::AttrDomain::Curve) {
      blender::ed::curves::select_linked(curves);
    }

    /* Convert selection domain. */
    const GVArray src = *attributes.lookup(".selection", domain);
    if (src) {
      const CPPType &type = src.type();
      void *dst = MEM_malloc_arrayN(attributes.domain_size(domain), type.size(), __func__);
      src.materialize(dst);

      attributes.remove(".selection");
      if (!attributes.add(".selection",
                          domain,
                          bke::cpp_type_to_custom_data_type(type),
                          bke::AttributeInitMoveArray(dst)))
      {
        MEM_freeN(dst);
      }

      changed = true;

      /* TODO: expand point selection to segments when in 'segment' mode. */
    }
  }

  if (changed) {
    /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
     * attribute for now. */
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);

    WM_main_add_notifier(NC_SPACE | ND_SPACE_VIEW3D, nullptr);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_set_selection_mode(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Select Mode";
  ot->idname = __func__;
  ot->description = "Change the selection mode for Grease Pencil strokes";

  ot->exec = select_set_mode_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = prop = RNA_def_enum(
      ot->srna, "mode", rna_enum_grease_pencil_selectmode_items, 0, "Mode", "");
  RNA_def_property_flag(prop, (PropertyFlag)(PROP_HIDDEN | PROP_SKIP_SAVE));
}

static int grease_pencil_material_select_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  const bool select = !RNA_boolean_get(op->ptr, "deselect");
  const int material_index = object->actcol - 1;

  if (material_index == -1) {
    return OPERATOR_CANCELLED;
  }

  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();

    IndexMaskMemory memory;
    const IndexMask strokes = retrieve_editable_strokes_by_material(
        *object, info.drawing, material_index, memory);
    if (strokes.is_empty()) {
      return;
    }
    bke::GSpanAttributeWriter selection = ed::curves::ensure_selection_attribute(
        curves, bke::AttrDomain::Curve, CD_PROP_BOOL);
    index_mask::masked_fill(selection.span.typed<bool>(), select, strokes);
    selection.finish();
  });

  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA | NA_EDITED, &grease_pencil);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_material_select(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Material";
  ot->idname = "GREASE_PENCIL_OT_material_select";
  ot->description = "Select/Deselect all Grease Pencil strokes using current material";

  /* callbacks. */
  ot->exec = grease_pencil_material_select_exec;
  ot->poll = editable_grease_pencil_poll;

  /* flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  ot->prop = RNA_def_boolean(ot->srna, "deselect", false, "Deselect", "Unselect strokes");
  RNA_def_property_flag(ot->prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

}  // namespace blender::ed::greasepencil

blender::bke::AttrDomain ED_grease_pencil_selection_domain_get(const ToolSettings *tool_settings)
{
  switch (tool_settings->gpencil_selectmode_edit) {
    case GP_SELECTMODE_POINT:
      return blender::bke::AttrDomain::Point;
    case GP_SELECTMODE_STROKE:
      return blender::bke::AttrDomain::Curve;
    case GP_SELECTMODE_SEGMENT:
      return blender::bke::AttrDomain::Point;
  }
  return blender::bke::AttrDomain::Point;
}

void ED_operatortypes_grease_pencil_select()
{
  using namespace blender::ed::greasepencil;
  WM_operatortype_append(GREASE_PENCIL_OT_select_all);
  WM_operatortype_append(GREASE_PENCIL_OT_select_more);
  WM_operatortype_append(GREASE_PENCIL_OT_select_less);
  WM_operatortype_append(GREASE_PENCIL_OT_select_linked);
  WM_operatortype_append(GREASE_PENCIL_OT_select_random);
  WM_operatortype_append(GREASE_PENCIL_OT_select_alternate);
  WM_operatortype_append(GREASE_PENCIL_OT_select_ends);
  WM_operatortype_append(GREASE_PENCIL_OT_set_selection_mode);
  WM_operatortype_append(GREASE_PENCIL_OT_material_select);
}
