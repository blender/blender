/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2018 Blender Foundation */

/** \file
 * \ingroup edobj
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_listbase.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_report.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"
#include "RNA_prototypes.h"

#include "ED_object.h"
#include "ED_screen.h"

#include "BLT_translation.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#include "object_intern.h"

/******************************** API ****************************/

GpencilModifierData *ED_object_gpencil_modifier_add(
    ReportList *reports, Main *bmain, Scene *UNUSED(scene), Object *ob, const char *name, int type)
{
  GpencilModifierData *new_md = NULL;
  const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(type);

  if (ob->type != OB_GPENCIL_LEGACY) {
    BKE_reportf(reports, RPT_WARNING, "Modifiers cannot be added to object '%s'", ob->id.name + 2);
    return NULL;
  }

  if (mti->flags & eGpencilModifierTypeFlag_Single) {
    if (BKE_gpencil_modifiers_findby_type(ob, type)) {
      BKE_report(reports, RPT_WARNING, "Only one modifier of this type is allowed");
      return NULL;
    }
  }

  /* get new modifier data to add */
  new_md = BKE_gpencil_modifier_new(type);

  BLI_addtail(&ob->greasepencil_modifiers, new_md);

  if (name) {
    BLI_strncpy_utf8(new_md->name, name, sizeof(new_md->name));
  }

  /* make sure modifier data has unique name */
  BKE_gpencil_modifier_unique_name(&ob->greasepencil_modifiers, new_md);

  /* Enable edit mode visible by default. */
  if (mti->flags & eGpencilModifierTypeFlag_SupportsEditmode) {
    new_md->mode |= eGpencilModifierMode_Editmode;
  }

  bGPdata *gpd = ob->data;
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  DEG_relations_tag_update(bmain);

  return new_md;
}

static bool gpencil_object_modifier_remove(Main *bmain,
                                           Object *ob,
                                           GpencilModifierData *md,
                                           bool *UNUSED(r_sort_depsgraph))
{
  /* It seems on rapid delete it is possible to
   * get called twice on same modifier, so make
   * sure it is in list. */
  if (BLI_findindex(&ob->greasepencil_modifiers, md) == -1) {
    return false;
  }

  DEG_relations_tag_update(bmain);

  BLI_remlink(&ob->greasepencil_modifiers, md);
  BKE_gpencil_modifier_free(md);
  BKE_object_free_derived_caches(ob);

  return true;
}

bool ED_object_gpencil_modifier_remove(ReportList *reports,
                                       Main *bmain,
                                       Object *ob,
                                       GpencilModifierData *md)
{
  bool sort_depsgraph = false;
  bool ok;

  ok = gpencil_object_modifier_remove(bmain, ob, md, &sort_depsgraph);

  if (!ok) {
    BKE_reportf(reports, RPT_ERROR, "Modifier '%s' not in object '%s'", md->name, ob->id.name);
    return false;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  DEG_relations_tag_update(bmain);

  return true;
}

void ED_object_gpencil_modifier_clear(Main *bmain, Object *ob)
{
  GpencilModifierData *md = ob->greasepencil_modifiers.first;
  bool sort_depsgraph = false;

  if (!md) {
    return;
  }

  while (md) {
    GpencilModifierData *next_md;

    next_md = md->next;

    gpencil_object_modifier_remove(bmain, ob, md, &sort_depsgraph);

    md = next_md;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  DEG_relations_tag_update(bmain);
}

bool ED_object_gpencil_modifier_move_up(ReportList *UNUSED(reports),
                                        Object *ob,
                                        GpencilModifierData *md)
{
  if (md->prev) {
    BLI_remlink(&ob->greasepencil_modifiers, md);
    BLI_insertlinkbefore(&ob->greasepencil_modifiers, md->prev, md);
  }

  return true;
}

bool ED_object_gpencil_modifier_move_down(ReportList *UNUSED(reports),
                                          Object *ob,
                                          GpencilModifierData *md)
{
  if (md->next) {
    BLI_remlink(&ob->greasepencil_modifiers, md);
    BLI_insertlinkafter(&ob->greasepencil_modifiers, md->next, md);
  }

  return true;
}

bool ED_object_gpencil_modifier_move_to_index(ReportList *reports,
                                              Object *ob,
                                              GpencilModifierData *md,
                                              const int index)
{
  BLI_assert(md != NULL);
  BLI_assert(index >= 0);
  if (index >= BLI_listbase_count(&ob->greasepencil_modifiers)) {
    BKE_report(reports, RPT_WARNING, "Cannot move modifier beyond the end of the stack");
    return false;
  }

  int md_index = BLI_findindex(&ob->greasepencil_modifiers, md);
  BLI_assert(md_index != -1);
  if (md_index < index) {
    /* Move modifier down in list. */
    for (; md_index < index; md_index++) {
      if (!ED_object_gpencil_modifier_move_down(reports, ob, md)) {
        break;
      }
    }
  }
  else {
    /* Move modifier up in list. */
    for (; md_index > index; md_index--) {
      if (!ED_object_gpencil_modifier_move_up(reports, ob, md)) {
        break;
      }
    }
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, ob);

  return true;
}

static bool gpencil_modifier_apply_obdata(
    ReportList *reports, Main *bmain, Depsgraph *depsgraph, Object *ob, GpencilModifierData *md)
{
  const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md->type);

  if (mti->isDisabled && mti->isDisabled(md, 0)) {
    BKE_report(reports, RPT_ERROR, "Modifier is disabled, skipping apply");
    return false;
  }

  if (ob->type == OB_GPENCIL_LEGACY) {
    if (ELEM(NULL, ob, ob->data)) {
      return false;
    }
    if (mti->bakeModifier == NULL) {
      BKE_report(reports, RPT_ERROR, "Not implemented");
      return false;
    }
    mti->bakeModifier(bmain, depsgraph, md, ob);
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }
  else {
    BKE_report(reports, RPT_ERROR, "Cannot apply modifier for this object type");
    return false;
  }

  return true;
}

bool ED_object_gpencil_modifier_apply(Main *bmain,
                                      ReportList *reports,
                                      Depsgraph *depsgraph,
                                      Object *ob,
                                      GpencilModifierData *md,
                                      int UNUSED(mode))
{

  if (ob->type == OB_GPENCIL_LEGACY) {
    if (ob->mode != OB_MODE_OBJECT) {
      BKE_report(reports, RPT_ERROR, "Modifiers cannot be applied in paint, sculpt or edit mode");
      return false;
    }

    if (((ID *)ob->data)->us > 1) {
      BKE_report(reports, RPT_ERROR, "Modifiers cannot be applied to multi-user data");
      return false;
    }
  }
  else if (((ID *)ob->data)->us > 1) {
    BKE_report(reports, RPT_ERROR, "Modifiers cannot be applied to multi-user data");
    return false;
  }

  if (md != ob->greasepencil_modifiers.first) {
    BKE_report(reports, RPT_INFO, "Applied modifier was not first, result may not be as expected");
  }

  if (!gpencil_modifier_apply_obdata(reports, bmain, depsgraph, ob, md)) {
    return false;
  }

  BLI_remlink(&ob->greasepencil_modifiers, md);
  BKE_gpencil_modifier_free(md);

  return true;
}

bool ED_object_gpencil_modifier_copy(ReportList *reports, Object *ob, GpencilModifierData *md)
{
  GpencilModifierData *nmd;
  const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md->type);
  GpencilModifierType type = md->type;

  if (mti->flags & eGpencilModifierTypeFlag_Single) {
    if (BKE_gpencil_modifiers_findby_type(ob, type)) {
      BKE_report(reports, RPT_WARNING, "Only one modifier of this type is allowed");
      return false;
    }
  }

  nmd = BKE_gpencil_modifier_new(md->type);
  BKE_gpencil_modifier_copydata(md, nmd);
  BLI_insertlinkafter(&ob->greasepencil_modifiers, md, nmd);
  BKE_gpencil_modifier_unique_name(&ob->greasepencil_modifiers, nmd);

  nmd->flag |= eGpencilModifierFlag_OverrideLibrary_Local;

  return true;
}

void ED_object_gpencil_modifier_copy_to_object(Object *ob_dst, GpencilModifierData *md)
{
  BKE_object_copy_gpencil_modifier(ob_dst, md);
  WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, ob_dst);
  DEG_id_tag_update(&ob_dst->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
}

/************************ add modifier operator *********************/

static int gpencil_modifier_add_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = ED_object_active_context(C);
  int type = RNA_enum_get(op->ptr, "type");

  if (!ED_object_gpencil_modifier_add(op->reports, bmain, scene, ob, NULL, type)) {
    return OPERATOR_CANCELLED;
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static const EnumPropertyItem *gpencil_modifier_add_itemf(bContext *C,
                                                          PointerRNA *UNUSED(ptr),
                                                          PropertyRNA *UNUSED(prop),
                                                          bool *r_free)
{
  Object *ob = ED_object_active_context(C);
  EnumPropertyItem *item = NULL;
  const EnumPropertyItem *md_item, *group_item = NULL;
  const GpencilModifierTypeInfo *mti;
  int totitem = 0, a;

  if (!ob) {
    return rna_enum_object_greasepencil_modifier_type_items;
  }

  for (a = 0; rna_enum_object_greasepencil_modifier_type_items[a].identifier; a++) {
    md_item = &rna_enum_object_greasepencil_modifier_type_items[a];
    if (md_item->identifier[0]) {
      mti = BKE_gpencil_modifier_get_info(md_item->value);

      if (mti->flags & eGpencilModifierTypeFlag_NoUserAdd) {
        continue;
      }
    }
    else {
      group_item = md_item;
      md_item = NULL;

      continue;
    }

    if (group_item) {
      RNA_enum_item_add(&item, &totitem, group_item);
      group_item = NULL;
    }

    RNA_enum_item_add(&item, &totitem, md_item);
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

void OBJECT_OT_gpencil_modifier_add(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Add Modifier";
  ot->description = "Add a procedural operation/effect to the active grease pencil object";
  ot->idname = "OBJECT_OT_gpencil_modifier_add";

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = gpencil_modifier_add_exec;
  ot->poll = ED_operator_object_active_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_enum(ot->srna,
                      "type",
                      rna_enum_object_greasepencil_modifier_type_items,
                      eGpencilModifierType_Thick,
                      "Type",
                      "");
  RNA_def_enum_funcs(prop, gpencil_modifier_add_itemf);
  ot->prop = prop;
}

/********** generic functions for operators using mod names and data context *********************/

static bool gpencil_edit_modifier_poll_generic(bContext *C,
                                               StructRNA *rna_type,
                                               int obtype_flag,
                                               const bool is_liboverride_allowed)
{
  Main *bmain = CTX_data_main(C);
  PointerRNA ptr = CTX_data_pointer_get_type(C, "modifier", rna_type);
  Object *ob = (ptr.owner_id) ? (Object *)ptr.owner_id : ED_object_active_context(C);
  GpencilModifierData *mod = ptr.data; /* May be NULL. */

  if (!ob || !BKE_id_is_editable(bmain, &ob->id)) {
    return false;
  }
  if (obtype_flag && ((1 << ob->type) & obtype_flag) == 0) {
    return false;
  }
  if (ptr.owner_id && !BKE_id_is_editable(bmain, ptr.owner_id)) {
    return false;
  }

  if (!is_liboverride_allowed && BKE_gpencil_modifier_is_nonlocal_in_liboverride(ob, mod)) {
    CTX_wm_operator_poll_msg_set(
        C, "Cannot edit modifiers coming from linked data in a library override");
    return false;
  }

  return true;
}

static bool gpencil_edit_modifier_poll(bContext *C)
{
  return gpencil_edit_modifier_poll_generic(C, &RNA_GpencilModifier, 0, false);
}

/* Used by operators performing actions allowed also on modifiers from the overridden linked object
 * (not only from added 'local' ones). */
static bool gpencil_edit_modifier_liboverride_allowed_poll(bContext *C)
{
  return gpencil_edit_modifier_poll_generic(C, &RNA_GpencilModifier, 0, true);
}

static void gpencil_edit_modifier_properties(wmOperatorType *ot)
{
  PropertyRNA *prop = RNA_def_string(
      ot->srna, "modifier", NULL, MAX_NAME, "Modifier", "Name of the modifier to edit");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

static void gpencil_edit_modifier_report_property(wmOperatorType *ot)
{
  PropertyRNA *prop = RNA_def_boolean(
      ot->srna, "report", false, "Report", "Create a notification after the operation");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

/**
 * \param event: If this isn't NULL, the operator will also look for panels underneath
 * the cursor with custom-data set to a modifier.
 * \param r_retval: This should be used if #event is used in order to return
 * #OPERATOR_PASS_THROUGH to check other operators with the same key set.
 */
static bool gpencil_edit_modifier_invoke_properties(bContext *C,
                                                    wmOperator *op,
                                                    const wmEvent *event,
                                                    int *r_retval)
{
  if (RNA_struct_property_is_set(op->ptr, "modifier")) {
    return true;
  }

  PointerRNA ctx_ptr = CTX_data_pointer_get_type(C, "modifier", &RNA_GpencilModifier);
  if (ctx_ptr.data != NULL) {
    GpencilModifierData *md = ctx_ptr.data;
    RNA_string_set(op->ptr, "modifier", md->name);
    return true;
  }

  /* Check the custom data of panels under the mouse for a modifier. */
  if (event != NULL) {
    PointerRNA *panel_ptr = UI_region_panel_custom_data_under_cursor(C, event);

    if (!(panel_ptr == NULL || RNA_pointer_is_null(panel_ptr))) {
      if (RNA_struct_is_a(panel_ptr->type, &RNA_GpencilModifier)) {
        GpencilModifierData *md = panel_ptr->data;
        RNA_string_set(op->ptr, "modifier", md->name);
        return true;
      }

      BLI_assert(r_retval != NULL); /* We need the return value in this case. */
      if (r_retval != NULL) {
        *r_retval = (OPERATOR_PASS_THROUGH | OPERATOR_CANCELLED);
      }
      return false;
    }
  }

  if (r_retval != NULL) {
    *r_retval = OPERATOR_CANCELLED;
  }
  return false;
}

static GpencilModifierData *gpencil_edit_modifier_property_get(wmOperator *op,
                                                               Object *ob,
                                                               int type)
{
  if (ob == NULL) {
    return NULL;
  }

  char modifier_name[MAX_NAME];
  GpencilModifierData *md;
  RNA_string_get(op->ptr, "modifier", modifier_name);

  md = BKE_gpencil_modifiers_findby_name(ob, modifier_name);

  if (md && type != 0 && md->type != type) {
    md = NULL;
  }

  return md;
}

/************************ remove modifier operator *********************/

static int gpencil_modifier_remove_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = ED_object_active_context(C);
  GpencilModifierData *md = gpencil_edit_modifier_property_get(op, ob, 0);

  if (md == NULL) {
    return OPERATOR_CANCELLED;
  }

  /* Store name temporarily for report. */
  char name[MAX_NAME];
  strcpy(name, md->name);

  if (!ED_object_gpencil_modifier_remove(op->reports, bmain, ob, md)) {
    return OPERATOR_CANCELLED;
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  if (RNA_boolean_get(op->ptr, "report")) {
    BKE_reportf(op->reports, RPT_INFO, "Removed modifier: %s", name);
  }

  return OPERATOR_FINISHED;
}

static int gpencil_modifier_remove_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  int retval;
  if (gpencil_edit_modifier_invoke_properties(C, op, event, &retval)) {
    return gpencil_modifier_remove_exec(C, op);
  }
  return retval;
}

void OBJECT_OT_gpencil_modifier_remove(wmOperatorType *ot)
{
  ot->name = "Remove Grease Pencil Modifier";
  ot->description = "Remove a modifier from the active grease pencil object";
  ot->idname = "OBJECT_OT_gpencil_modifier_remove";

  ot->invoke = gpencil_modifier_remove_invoke;
  ot->exec = gpencil_modifier_remove_exec;
  ot->poll = gpencil_edit_modifier_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  gpencil_edit_modifier_properties(ot);
  gpencil_edit_modifier_report_property(ot);
}

/************************ move up modifier operator *********************/

static int gpencil_modifier_move_up_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_active_context(C);
  GpencilModifierData *md = gpencil_edit_modifier_property_get(op, ob, 0);

  if (!md || !ED_object_gpencil_modifier_move_up(op->reports, ob, md)) {
    return OPERATOR_CANCELLED;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static int gpencil_modifier_move_up_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  int retval;
  if (gpencil_edit_modifier_invoke_properties(C, op, event, &retval)) {
    return gpencil_modifier_move_up_exec(C, op);
  }
  return retval;
}

void OBJECT_OT_gpencil_modifier_move_up(wmOperatorType *ot)
{
  ot->name = "Move Up Modifier";
  ot->description = "Move modifier up in the stack";
  ot->idname = "OBJECT_OT_gpencil_modifier_move_up";

  ot->invoke = gpencil_modifier_move_up_invoke;
  ot->exec = gpencil_modifier_move_up_exec;
  ot->poll = gpencil_edit_modifier_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  gpencil_edit_modifier_properties(ot);
}

/************************ move down modifier operator *********************/

static int gpencil_modifier_move_down_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_active_context(C);
  GpencilModifierData *md = gpencil_edit_modifier_property_get(op, ob, 0);

  if (!md || !ED_object_gpencil_modifier_move_down(op->reports, ob, md)) {
    return OPERATOR_CANCELLED;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static int gpencil_modifier_move_down_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  int retval;
  if (gpencil_edit_modifier_invoke_properties(C, op, event, &retval)) {
    return gpencil_modifier_move_down_exec(C, op);
  }
  return retval;
}

void OBJECT_OT_gpencil_modifier_move_down(wmOperatorType *ot)
{
  ot->name = "Move Down Modifier";
  ot->description = "Move modifier down in the stack";
  ot->idname = "OBJECT_OT_gpencil_modifier_move_down";

  ot->invoke = gpencil_modifier_move_down_invoke;
  ot->exec = gpencil_modifier_move_down_exec;
  ot->poll = gpencil_edit_modifier_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  gpencil_edit_modifier_properties(ot);
}

/* ************************* Move to Index Gpencil Modifier Operator ************************* */

static int gpencil_modifier_move_to_index_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_active_context(C);
  GpencilModifierData *md = gpencil_edit_modifier_property_get(op, ob, 0);
  int index = RNA_int_get(op->ptr, "index");
  if (!(md && ED_object_gpencil_modifier_move_to_index(op->reports, ob, md, index))) {
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

static int gpencil_modifier_move_to_index_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  int retval;
  if (gpencil_edit_modifier_invoke_properties(C, op, event, &retval)) {
    return gpencil_modifier_move_to_index_exec(C, op);
  }
  return retval;
}

void OBJECT_OT_gpencil_modifier_move_to_index(wmOperatorType *ot)
{
  ot->name = "Move Active Modifier to Index";
  ot->idname = "OBJECT_OT_gpencil_modifier_move_to_index";
  ot->description =
      "Change the modifier's position in the list so it evaluates after the set number of "
      "others";

  ot->invoke = gpencil_modifier_move_to_index_invoke;
  ot->exec = gpencil_modifier_move_to_index_exec;
  ot->poll = gpencil_edit_modifier_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
  RNA_def_int(
      ot->srna, "index", 0, 0, INT_MAX, "Index", "The index to move the modifier to", 0, INT_MAX);
}

/************************ apply modifier operator *********************/

static int gpencil_modifier_apply_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *ob = ED_object_active_context(C);
  GpencilModifierData *md = gpencil_edit_modifier_property_get(op, ob, 0);
  int apply_as = RNA_enum_get(op->ptr, "apply_as");
  const bool do_report = RNA_boolean_get(op->ptr, "report");

  if (md == NULL) {
    return OPERATOR_CANCELLED;
  }

  int reports_len;
  char name[MAX_NAME];
  if (do_report) {
    reports_len = BLI_listbase_count(&op->reports->list);
    strcpy(name, md->name); /* Store name temporarily since the modifier is removed. */
  }

  if (!ED_object_gpencil_modifier_apply(bmain, op->reports, depsgraph, ob, md, apply_as)) {
    return OPERATOR_CANCELLED;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  if (do_report) {
    /* Only add this report if the operator didn't cause another one. The purpose here is
     * to alert that something happened, and the previous report will do that anyway. */
    if (BLI_listbase_count(&op->reports->list) == reports_len) {
      BKE_reportf(op->reports, RPT_INFO, "Applied modifier: %s", name);
    }
  }

  return OPERATOR_FINISHED;
}

static int gpencil_modifier_apply_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  int retval;
  if (gpencil_edit_modifier_invoke_properties(C, op, event, &retval)) {
    return gpencil_modifier_apply_exec(C, op);
  }
  return retval;
}

static const EnumPropertyItem gpencil_modifier_apply_as_items[] = {
    {MODIFIER_APPLY_DATA, "DATA", 0, "Object Data", "Apply modifier to the object's data"},
    {MODIFIER_APPLY_SHAPE,
     "SHAPE",
     0,
     "New Shape",
     "Apply deform-only modifier to a new shape on this object"},
    {0, NULL, 0, NULL, NULL},
};

void OBJECT_OT_gpencil_modifier_apply(wmOperatorType *ot)
{
  ot->name = "Apply Modifier";
  ot->description = "Apply modifier and remove from the stack";
  ot->idname = "OBJECT_OT_gpencil_modifier_apply";

  ot->invoke = gpencil_modifier_apply_invoke;
  ot->exec = gpencil_modifier_apply_exec;
  ot->poll = gpencil_edit_modifier_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  RNA_def_enum(ot->srna,
               "apply_as",
               gpencil_modifier_apply_as_items,
               MODIFIER_APPLY_DATA,
               "Apply As",
               "How to apply the modifier to the geometry");
  gpencil_edit_modifier_properties(ot);
  gpencil_edit_modifier_report_property(ot);
}

/************************ copy modifier operator *********************/

static int gpencil_modifier_copy_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_active_context(C);
  GpencilModifierData *md = gpencil_edit_modifier_property_get(op, ob, 0);

  if (!md || !ED_object_gpencil_modifier_copy(op->reports, ob, md)) {
    return OPERATOR_CANCELLED;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static int gpencil_modifier_copy_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  int retval;
  if (gpencil_edit_modifier_invoke_properties(C, op, event, &retval)) {
    return gpencil_modifier_copy_exec(C, op);
  }
  return retval;
}

void OBJECT_OT_gpencil_modifier_copy(wmOperatorType *ot)
{
  ot->name = "Copy Modifier";
  ot->description = "Duplicate modifier at the same position in the stack";
  ot->idname = "OBJECT_OT_gpencil_modifier_copy";

  ot->invoke = gpencil_modifier_copy_invoke;
  ot->exec = gpencil_modifier_copy_exec;
  ot->poll = gpencil_edit_modifier_liboverride_allowed_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  gpencil_edit_modifier_properties(ot);
}

/************************ Copy Modifier to Selected Operator *********************/

static int gpencil_modifier_copy_to_selected_exec(bContext *C, wmOperator *op)
{
  Object *obact = ED_object_active_context(C);
  GpencilModifierData *md = gpencil_edit_modifier_property_get(op, obact, 0);

  if (!md) {
    return OPERATOR_CANCELLED;
  }

  if (obact->type != OB_GPENCIL_LEGACY) {
    BKE_reportf(op->reports,
                RPT_ERROR,
                "Source object '%s' is not a grease pencil object",
                obact->id.name + 2);
    return OPERATOR_CANCELLED;
  }

  CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
    if (ob == obact) {
      continue;
    }

    if (ob->type != OB_GPENCIL_LEGACY) {
      BKE_reportf(op->reports,
                  RPT_WARNING,
                  "Destination object '%s' is not a grease pencil object",
                  ob->id.name + 2);
      continue;
    }

    /* This always returns true right now. */
    BKE_object_copy_gpencil_modifier(ob, md);

    WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
  }
  CTX_DATA_END;

  return OPERATOR_FINISHED;
}

static int gpencil_modifier_copy_to_selected_invoke(bContext *C,
                                                    wmOperator *op,
                                                    const wmEvent *event)
{
  int retval;
  if (gpencil_edit_modifier_invoke_properties(C, op, event, &retval)) {
    return gpencil_modifier_copy_to_selected_exec(C, op);
  }
  return retval;
}

static bool gpencil_modifier_copy_to_selected_poll(bContext *C)
{
  Object *obact = ED_object_active_context(C);

  /* This could have a performance impact in the worst case, where there are many objects selected
   * and none of them pass the check. But that should be uncommon, and this operator is only
   * exposed in a drop-down menu anyway. */
  bool found_supported_objects = false;
  CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
    if (ob == obact) {
      continue;
    }

    if (ob->type == OB_GPENCIL_LEGACY) {
      found_supported_objects = true;
      break;
    }
  }
  CTX_DATA_END;

  if (!found_supported_objects) {
    CTX_wm_operator_poll_msg_set(C, "No supported objects were selected");
    return false;
  }
  return true;
}

void OBJECT_OT_gpencil_modifier_copy_to_selected(wmOperatorType *ot)
{
  ot->name = "Copy Modifier to Selected";
  ot->description = "Copy the modifier from the active object to all selected objects";
  ot->idname = "OBJECT_OT_gpencil_modifier_copy_to_selected";

  ot->invoke = gpencil_modifier_copy_to_selected_invoke;
  ot->exec = gpencil_modifier_copy_to_selected_exec;
  ot->poll = gpencil_modifier_copy_to_selected_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  gpencil_edit_modifier_properties(ot);
}

/************************* Time Offset Advanced Modifier *******************************/

static bool time_segment_poll(bContext *C)
{
  return gpencil_edit_modifier_poll_generic(C, &RNA_TimeGpencilModifier, 0, false);
}

static bool time_segment_name_exists_fn(void *arg, const char *name)
{
  const TimeGpencilModifierData *gpmd = (const TimeGpencilModifierData *)arg;
  for (int i = 0; i < gpmd->segments_len; i++) {
    if (STREQ(gpmd->segments[i].name, name) && gpmd->segments[i].name != name) {
      return true;
    }
  }
  return false;
}

static int time_segment_add_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_active_context(C);
  TimeGpencilModifierData *gpmd = (TimeGpencilModifierData *)gpencil_edit_modifier_property_get(
      op, ob, eGpencilModifierType_Time);
  if (gpmd == NULL) {
    return OPERATOR_CANCELLED;
  }
  const int new_active_index = gpmd->segment_active_index + 1;
  TimeGpencilModifierSegment *new_segments = MEM_malloc_arrayN(
      gpmd->segments_len + 1, sizeof(TimeGpencilModifierSegment), __func__);

  if (gpmd->segments_len != 0) {
    /* Copy the segments before the new segment. */
    memcpy(new_segments, gpmd->segments, sizeof(TimeGpencilModifierSegment) * new_active_index);
    /* Copy the segments after the new segment. */
    memcpy(new_segments + new_active_index + 1,
           gpmd->segments + new_active_index,
           sizeof(TimeGpencilModifierSegment) * (gpmd->segments_len - new_active_index));
  }

  /* Create the new segment. */
  TimeGpencilModifierSegment *ds = &new_segments[new_active_index];
  memcpy(
      ds, DNA_struct_default_get(TimeGpencilModifierSegment), sizeof(TimeGpencilModifierSegment));
  BLI_uniquename_cb(
      time_segment_name_exists_fn, gpmd, DATA_("Segment"), '.', ds->name, sizeof(ds->name));
  ds->gpmd = gpmd;

  MEM_SAFE_FREE(gpmd->segments);
  gpmd->segments = new_segments;
  gpmd->segments_len++;
  gpmd->segment_active_index++;

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY | ID_RECALC_COPY_ON_WRITE);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static int time_segment_add_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  if (gpencil_edit_modifier_invoke_properties(C, op, NULL, NULL)) {
    return time_segment_add_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void GPENCIL_OT_time_segment_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Segment";
  ot->description = "Add a segment to the time modifier";
  ot->idname = "GPENCIL_OT_time_segment_add";

  /* api callbacks */
  ot->poll = time_segment_poll;
  ot->invoke = time_segment_add_invoke;
  ot->exec = time_segment_add_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
}

static int time_segment_remove_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_active_context(C);

  TimeGpencilModifierData *gpmd = (TimeGpencilModifierData *)gpencil_edit_modifier_property_get(
      op, ob, eGpencilModifierType_Time);

  if (gpmd == NULL) {
    return OPERATOR_CANCELLED;
  }
  if (gpmd->segment_active_index < 0 || gpmd->segment_active_index >= gpmd->segments_len) {
    return OPERATOR_CANCELLED;
  }

  if (gpmd->segments_len == 1) {
    MEM_SAFE_FREE(gpmd->segments);
    gpmd->segment_active_index = -1;
  }
  else {
    TimeGpencilModifierSegment *new_segments = MEM_malloc_arrayN(
        gpmd->segments_len, sizeof(TimeGpencilModifierSegment), __func__);

    /* Copy the segments before the deleted segment. */
    memcpy(new_segments,
           gpmd->segments,
           sizeof(TimeGpencilModifierSegment) * gpmd->segment_active_index);

    /* Copy the segments after the deleted segment. */
    memcpy(new_segments + gpmd->segment_active_index,
           gpmd->segments + gpmd->segment_active_index + 1,
           sizeof(TimeGpencilModifierSegment) *
               (gpmd->segments_len - gpmd->segment_active_index - 1));

    MEM_freeN(gpmd->segments);
    gpmd->segments = new_segments;
    gpmd->segment_active_index = MAX2(gpmd->segment_active_index - 1, 0);
  }

  gpmd->segments_len--;

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY | ID_RECALC_COPY_ON_WRITE);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static int time_segment_remove_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  if (gpencil_edit_modifier_invoke_properties(C, op, NULL, NULL)) {
    return time_segment_remove_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void GPENCIL_OT_time_segment_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Time Segment";
  ot->description = "Remove the active segment from the time modifier";
  ot->idname = "GPENCIL_OT_time_segment_remove";

  /* api callbacks */
  ot->poll = time_segment_poll;
  ot->invoke = time_segment_remove_invoke;
  ot->exec = time_segment_remove_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);

  RNA_def_int(
      ot->srna, "index", 0, 0, INT_MAX, "Index", "Index of the segment to remove", 0, INT_MAX);
}

enum {
  GP_TIME_SEGEMENT_MOVE_UP = -1,
  GP_TIME_SEGEMENT_MOVE_DOWN = 1,
};

static int time_segment_move_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_active_context(C);

  TimeGpencilModifierData *gpmd = (TimeGpencilModifierData *)gpencil_edit_modifier_property_get(
      op, ob, eGpencilModifierType_Time);

  if (gpmd == NULL) {
    return OPERATOR_CANCELLED;
  }
  if (gpmd->segments_len < 2) {
    return OPERATOR_CANCELLED;
  }

  const int direction = RNA_enum_get(op->ptr, "type");
  if (direction == GP_TIME_SEGEMENT_MOVE_UP) {
    if (gpmd->segment_active_index == 0) {
      return OPERATOR_CANCELLED;
    }

    SWAP(TimeGpencilModifierSegment,
         gpmd->segments[gpmd->segment_active_index],
         gpmd->segments[gpmd->segment_active_index - 1]);

    gpmd->segment_active_index--;
  }
  else if (direction == GP_TIME_SEGEMENT_MOVE_DOWN) {
    if (gpmd->segment_active_index == gpmd->segments_len - 1) {
      return OPERATOR_CANCELLED;
    }

    SWAP(TimeGpencilModifierSegment,
         gpmd->segments[gpmd->segment_active_index],
         gpmd->segments[gpmd->segment_active_index + 1]);

    gpmd->segment_active_index++;
  }
  else {
    return OPERATOR_CANCELLED;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY | ID_RECALC_COPY_ON_WRITE);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static int time_segment_move_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  if (gpencil_edit_modifier_invoke_properties(C, op, NULL, NULL)) {
    return time_segment_move_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void GPENCIL_OT_time_segment_move(wmOperatorType *ot)
{
  static const EnumPropertyItem segment_move[] = {
      {GP_TIME_SEGEMENT_MOVE_UP, "UP", 0, "Up", ""},
      {GP_TIME_SEGEMENT_MOVE_DOWN, "DOWN", 0, "Down", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Move Time Segment";
  ot->description = "Move the active time segment up or down";
  ot->idname = "GPENCIL_OT_time_segment_move";

  /* api callbacks */
  ot->poll = time_segment_poll;
  ot->invoke = time_segment_move_invoke;
  ot->exec = time_segment_move_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);

  ot->prop = RNA_def_enum(ot->srna, "type", segment_move, 0, "Type", "");
}

/************************* Dash Modifier *******************************/

static bool dash_segment_poll(bContext *C)
{
  return gpencil_edit_modifier_poll_generic(C, &RNA_DashGpencilModifierData, 0, false);
}

static bool dash_segment_name_exists_fn(void *arg, const char *name)
{
  const DashGpencilModifierData *dmd = (const DashGpencilModifierData *)arg;
  for (int i = 0; i < dmd->segments_len; i++) {
    if (STREQ(dmd->segments[i].name, name)) {
      return true;
    }
  }
  return false;
}

static int dash_segment_add_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_active_context(C);
  DashGpencilModifierData *dmd = (DashGpencilModifierData *)gpencil_edit_modifier_property_get(
      op, ob, eGpencilModifierType_Dash);

  if (dmd == NULL) {
    return OPERATOR_CANCELLED;
  }
  const int new_active_index = dmd->segment_active_index + 1;
  DashGpencilModifierSegment *new_segments = MEM_malloc_arrayN(
      dmd->segments_len + 1, sizeof(DashGpencilModifierSegment), __func__);

  if (dmd->segments_len != 0) {
    /* Copy the segments before the new segment. */
    memcpy(new_segments, dmd->segments, sizeof(DashGpencilModifierSegment) * new_active_index);
    /* Copy the segments after the new segment. */
    memcpy(new_segments + new_active_index + 1,
           dmd->segments + new_active_index,
           sizeof(DashGpencilModifierSegment) * (dmd->segments_len - new_active_index));
  }

  /* Create the new segment. */
  DashGpencilModifierSegment *ds = &new_segments[new_active_index];
  memcpy(
      ds, DNA_struct_default_get(DashGpencilModifierSegment), sizeof(DashGpencilModifierSegment));
  BLI_uniquename_cb(
      dash_segment_name_exists_fn, dmd, DATA_("Segment"), '.', ds->name, sizeof(ds->name));
  ds->dmd = dmd;

  MEM_SAFE_FREE(dmd->segments);
  dmd->segments = new_segments;
  dmd->segments_len++;
  dmd->segment_active_index++;

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY | ID_RECALC_COPY_ON_WRITE);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static int dash_segment_add_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  if (gpencil_edit_modifier_invoke_properties(C, op, NULL, NULL)) {
    return dash_segment_add_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void GPENCIL_OT_segment_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Segment";
  ot->description = "Add a segment to the dash modifier";
  ot->idname = "GPENCIL_OT_segment_add";

  /* api callbacks */
  ot->poll = dash_segment_poll;
  ot->invoke = dash_segment_add_invoke;
  ot->exec = dash_segment_add_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
}

static int dash_segment_remove_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_active_context(C);

  DashGpencilModifierData *dmd = (DashGpencilModifierData *)gpencil_edit_modifier_property_get(
      op, ob, eGpencilModifierType_Dash);

  if (dmd == NULL) {
    return OPERATOR_CANCELLED;
  }

  if (dmd->segment_active_index < 0 || dmd->segment_active_index >= dmd->segments_len) {
    return OPERATOR_CANCELLED;
  }

  if (dmd->segments_len == 1) {
    MEM_SAFE_FREE(dmd->segments);
    dmd->segment_active_index = -1;
  }
  else {
    DashGpencilModifierSegment *new_segments = MEM_malloc_arrayN(
        dmd->segments_len, sizeof(DashGpencilModifierSegment), __func__);

    /* Copy the segments before the deleted segment. */
    memcpy(new_segments,
           dmd->segments,
           sizeof(DashGpencilModifierSegment) * dmd->segment_active_index);

    /* Copy the segments after the deleted segment. */
    memcpy(new_segments + dmd->segment_active_index,
           dmd->segments + dmd->segment_active_index + 1,
           sizeof(DashGpencilModifierSegment) *
               (dmd->segments_len - dmd->segment_active_index - 1));

    MEM_freeN(dmd->segments);
    dmd->segments = new_segments;
    dmd->segment_active_index = MAX2(dmd->segment_active_index - 1, 0);
  }

  dmd->segments_len--;

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY | ID_RECALC_COPY_ON_WRITE);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static int dash_segment_remove_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  if (gpencil_edit_modifier_invoke_properties(C, op, NULL, NULL)) {
    return dash_segment_remove_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void GPENCIL_OT_segment_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Dash Segment";
  ot->description = "Remove the active segment from the dash modifier";
  ot->idname = "GPENCIL_OT_segment_remove";

  /* api callbacks */
  ot->poll = dash_segment_poll;
  ot->invoke = dash_segment_remove_invoke;
  ot->exec = dash_segment_remove_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);

  RNA_def_int(
      ot->srna, "index", 0, 0, INT_MAX, "Index", "Index of the segment to remove", 0, INT_MAX);
}

enum {
  GP_SEGEMENT_MOVE_UP = -1,
  GP_SEGEMENT_MOVE_DOWN = 1,
};

static int dash_segment_move_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_active_context(C);

  DashGpencilModifierData *dmd = (DashGpencilModifierData *)gpencil_edit_modifier_property_get(
      op, ob, eGpencilModifierType_Dash);

  if (dmd == NULL) {
    return OPERATOR_CANCELLED;
  }

  if (dmd->segments_len < 2) {
    return OPERATOR_CANCELLED;
  }

  const int direction = RNA_enum_get(op->ptr, "type");
  if (direction == GP_SEGEMENT_MOVE_UP) {
    if (dmd->segment_active_index == 0) {
      return OPERATOR_CANCELLED;
    }

    SWAP(DashGpencilModifierSegment,
         dmd->segments[dmd->segment_active_index],
         dmd->segments[dmd->segment_active_index - 1]);

    dmd->segment_active_index--;
  }
  else if (direction == GP_SEGEMENT_MOVE_DOWN) {
    if (dmd->segment_active_index == dmd->segments_len - 1) {
      return OPERATOR_CANCELLED;
    }

    SWAP(DashGpencilModifierSegment,
         dmd->segments[dmd->segment_active_index],
         dmd->segments[dmd->segment_active_index + 1]);

    dmd->segment_active_index++;
  }
  else {
    return OPERATOR_CANCELLED;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY | ID_RECALC_COPY_ON_WRITE);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static int dash_segment_move_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  if (gpencil_edit_modifier_invoke_properties(C, op, NULL, NULL)) {
    return dash_segment_move_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void GPENCIL_OT_segment_move(wmOperatorType *ot)
{
  static const EnumPropertyItem segment_move[] = {
      {GP_SEGEMENT_MOVE_UP, "UP", 0, "Up", ""},
      {GP_SEGEMENT_MOVE_DOWN, "DOWN", 0, "Down", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Move Dash Segment";
  ot->description = "Move the active dash segment up or down";
  ot->idname = "GPENCIL_OT_segment_move";

  /* api callbacks */
  ot->poll = dash_segment_poll;
  ot->invoke = dash_segment_move_invoke;
  ot->exec = dash_segment_move_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);

  ot->prop = RNA_def_enum(ot->srna, "type", segment_move, 0, "Type", "");
}
