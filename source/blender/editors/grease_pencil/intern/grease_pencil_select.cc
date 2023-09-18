/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "BLI_vector_set.hh"

#include "BKE_context.h"
#include "BKE_grease_pencil.hh"

#include "DNA_object_types.h"

#include "DEG_depsgraph.h"

#include "ED_curves.hh"
#include "ED_grease_pencil.hh"
#include "ED_screen.hh"
#include "ED_view3d.hh"

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
  eAttrDomain selection_domain = ED_grease_pencil_selection_domain_get(C);

  grease_pencil.foreach_editable_drawing(
      scene->r.cfra, [&](int /*drawing_index*/, blender::bke::greasepencil::Drawing &drawing) {
        blender::ed::curves::select_all(drawing.strokes_for_write(), selection_domain, action);
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

  grease_pencil.foreach_editable_drawing(
      scene->r.cfra, [](int /*drawing_index*/, blender::bke::greasepencil::Drawing &drawing) {
        blender::ed::curves::select_adjacent(drawing.strokes_for_write(), false);
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

  grease_pencil.foreach_editable_drawing(
      scene->r.cfra, [](int /*drawing_index*/, blender::bke::greasepencil::Drawing &drawing) {
        blender::ed::curves::select_adjacent(drawing.strokes_for_write(), true);
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

  grease_pencil.foreach_editable_drawing(
      scene->r.cfra, [](int /*drawing_index*/, blender::bke::greasepencil::Drawing &drawing) {
        blender::ed::curves::select_linked(drawing.strokes_for_write());
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
  eAttrDomain selection_domain = ED_grease_pencil_selection_domain_get(C);

  grease_pencil.foreach_editable_drawing(
      scene->r.cfra, [&](int drawing_index, bke::greasepencil::Drawing &drawing) {
        bke::CurvesGeometry &curves = drawing.strokes_for_write();

        IndexMaskMemory memory;
        const IndexMask random_elements = ed::curves::random_mask(
            curves,
            selection_domain,
            blender::get_default_hash_2<int>(seed, drawing_index),
            ratio,
            memory);

        const bool was_anything_selected = ed::curves::has_anything_selected(curves);
        bke::GSpanAttributeWriter selection = ed::curves::ensure_selection_attribute(
            curves, selection_domain, CD_PROP_BOOL);
        if (!was_anything_selected) {
          curves::fill_selection_true(selection.span);
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

  grease_pencil.foreach_editable_drawing(
      scene->r.cfra, [&](int /*drawing_index*/, blender::bke::greasepencil::Drawing &drawing) {
        blender::ed::curves::select_alternate(drawing.strokes_for_write(), deselect_ends);
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

  grease_pencil.foreach_editable_drawing(
      scene->r.cfra, [&](int /*drawing_index*/, blender::bke::greasepencil::Drawing &drawing) {
        bke::CurvesGeometry &curves = drawing.strokes_for_write();

        IndexMaskMemory memory;
        const IndexMask inverted_end_points_mask = ed::curves::end_points(
            curves, amount_start, amount_end, true, memory);

        const bool was_anything_selected = ed::curves::has_anything_selected(curves);
        bke::GSpanAttributeWriter selection = ed::curves::ensure_selection_attribute(
            curves, ATTR_DOMAIN_POINT, CD_PROP_BOOL);
        if (!was_anything_selected) {
          ed::curves::fill_selection_true(selection.span);
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
  ts->gpencil_selectmode_edit = mode_new;

  /* Convert all drawings of the active GP to the new selection domain. */
  const eAttrDomain domain = ED_grease_pencil_selection_domain_get(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  Span<GreasePencilDrawingBase *> drawings = grease_pencil.drawings();
  bool changed = false;

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
    if (domain == ATTR_DOMAIN_CURVE) {
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

}  // namespace blender::ed::greasepencil

eAttrDomain ED_grease_pencil_selection_domain_get(bContext *C)
{
  ToolSettings *ts = CTX_data_tool_settings(C);

  switch (ts->gpencil_selectmode_edit) {
    case GP_SELECTMODE_POINT:
      return ATTR_DOMAIN_POINT;
      break;
    case GP_SELECTMODE_STROKE:
      return ATTR_DOMAIN_CURVE;
      break;
    case GP_SELECTMODE_SEGMENT:
      return ATTR_DOMAIN_POINT;
      break;
  }
  return ATTR_DOMAIN_POINT;
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
}
