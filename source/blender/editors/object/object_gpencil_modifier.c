/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2018 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edobj
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_gpencil_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_listbase.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_report.h"
#include "BKE_object.h"
#include "BKE_gpencil.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "ED_object.h"
#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "object_intern.h"

/******************************** API ****************************/

GpencilModifierData *ED_object_gpencil_modifier_add(
    ReportList *reports, Main *bmain, Scene *UNUSED(scene), Object *ob, const char *name, int type)
{
  GpencilModifierData *new_md = NULL;
  const GpencilModifierTypeInfo *mti = BKE_gpencil_modifierType_getInfo(type);

  if (ob->type != OB_GPENCIL) {
    BKE_reportf(reports, RPT_WARNING, "Modifiers cannot be added to object '%s'", ob->id.name + 2);
    return NULL;
  }

  if (mti->flags & eGpencilModifierTypeFlag_Single) {
    if (BKE_gpencil_modifiers_findByType(ob, type)) {
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

  bGPdata *gpd = ob->data;
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  DEG_relations_tag_update(bmain);

  return new_md;
}

/* Return true if the object has a modifier of type 'type' other than
 * the modifier pointed to be 'exclude', otherwise returns false. */
static bool UNUSED_FUNCTION(gpencil_object_has_modifier)(const Object *ob,
                                                         const GpencilModifierData *exclude,
                                                         GpencilModifierType type)
{
  GpencilModifierData *md;

  for (md = ob->greasepencil_modifiers.first; md; md = md->next) {
    if ((md != exclude) && (md->type == type)) {
      return true;
    }
  }

  return false;
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
    return 0;
  }

  DEG_relations_tag_update(bmain);

  BLI_remlink(&ob->greasepencil_modifiers, md);
  BKE_gpencil_modifier_free(md);
  BKE_object_free_derived_caches(ob);

  return 1;
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
    return 0;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  DEG_relations_tag_update(bmain);

  return 1;
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

int ED_object_gpencil_modifier_move_up(ReportList *UNUSED(reports),
                                       Object *ob,
                                       GpencilModifierData *md)
{
  if (md->prev) {
    BLI_remlink(&ob->greasepencil_modifiers, md);
    BLI_insertlinkbefore(&ob->greasepencil_modifiers, md->prev, md);
  }

  return 1;
}

int ED_object_gpencil_modifier_move_down(ReportList *UNUSED(reports),
                                         Object *ob,
                                         GpencilModifierData *md)
{
  if (md->next) {
    BLI_remlink(&ob->greasepencil_modifiers, md);
    BLI_insertlinkafter(&ob->greasepencil_modifiers, md->next, md);
  }

  return 1;
}

static int gpencil_modifier_apply_obdata(
    ReportList *reports, Main *bmain, Depsgraph *depsgraph, Object *ob, GpencilModifierData *md)
{
  const GpencilModifierTypeInfo *mti = BKE_gpencil_modifierType_getInfo(md->type);

  if (mti->isDisabled && mti->isDisabled(md, 0)) {
    BKE_report(reports, RPT_ERROR, "Modifier is disabled, skipping apply");
    return 0;
  }

  if (ob->type == OB_GPENCIL) {
    if (ELEM(NULL, ob, ob->data)) {
      return 0;
    }
    else if (mti->bakeModifier == NULL) {
      BKE_report(reports, RPT_ERROR, "Not implemented");
      return 0;
    }
    mti->bakeModifier(bmain, depsgraph, md, ob);
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }
  else {
    BKE_report(reports, RPT_ERROR, "Cannot apply modifier for this object type");
    return 0;
  }

  return 1;
}

int ED_object_gpencil_modifier_apply(Main *bmain,
                                     ReportList *reports,
                                     Depsgraph *depsgraph,
                                     Object *ob,
                                     GpencilModifierData *md,
                                     int UNUSED(mode))
{

  if (ob->type == OB_GPENCIL) {
    if (ob->mode != OB_MODE_OBJECT) {
      BKE_report(reports, RPT_ERROR, "Modifiers cannot be applied in paint, sculpt or edit mode");
      return 0;
    }

    if (((ID *)ob->data)->us > 1) {
      BKE_report(reports, RPT_ERROR, "Modifiers cannot be applied to multi-user data");
      return 0;
    }
  }
  else if (((ID *)ob->data)->us > 1) {
    BKE_report(reports, RPT_ERROR, "Modifiers cannot be applied to multi-user data");
    return 0;
  }

  if (md != ob->greasepencil_modifiers.first) {
    BKE_report(reports, RPT_INFO, "Applied modifier was not first, result may not be as expected");
  }

  if (!gpencil_modifier_apply_obdata(reports, bmain, depsgraph, ob, md)) {
    return 0;
  }

  BLI_remlink(&ob->greasepencil_modifiers, md);
  BKE_gpencil_modifier_free(md);

  return 1;
}

int ED_object_gpencil_modifier_copy(ReportList *reports, Object *ob, GpencilModifierData *md)
{
  GpencilModifierData *nmd;
  const GpencilModifierTypeInfo *mti = BKE_gpencil_modifierType_getInfo(md->type);
  GpencilModifierType type = md->type;

  if (mti->flags & eGpencilModifierTypeFlag_Single) {
    if (BKE_gpencil_modifiers_findByType(ob, type)) {
      BKE_report(reports, RPT_WARNING, "Only one modifier of this type is allowed");
      return 0;
    }
  }

  nmd = BKE_gpencil_modifier_new(md->type);
  BKE_gpencil_modifier_copyData(md, nmd);
  BLI_insertlinkafter(&ob->greasepencil_modifiers, md, nmd);
  BKE_gpencil_modifier_unique_name(&ob->greasepencil_modifiers, nmd);

  return 1;
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
      mti = BKE_gpencil_modifierType_getInfo(md_item->value);

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
  ot->name = "Add Grease Pencil Modifier";
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
                      rna_enum_object_modifier_type_items,
                      eGpencilModifierType_Thick,
                      "Type",
                      "");
  RNA_def_enum_funcs(prop, gpencil_modifier_add_itemf);
  ot->prop = prop;
}

/********** generic functions for operators using mod names and data context *********************/

static int gpencil_edit_modifier_poll_generic(bContext *C, StructRNA *rna_type, int obtype_flag)
{
  PointerRNA ptr = CTX_data_pointer_get_type(C, "modifier", rna_type);
  Object *ob = (ptr.id.data) ? ptr.id.data : ED_object_active_context(C);

  if (!ob || ID_IS_LINKED(ob)) {
    return 0;
  }
  if (obtype_flag && ((1 << ob->type) & obtype_flag) == 0) {
    return 0;
  }
  if (ptr.id.data && ID_IS_LINKED(ptr.id.data)) {
    return 0;
  }

  if (ID_IS_OVERRIDE_LIBRARY(ob)) {
    CTX_wm_operator_poll_msg_set(C, "Cannot edit modifiers coming from library override");
    return (((GpencilModifierData *)ptr.data)->flag &
            eGpencilModifierFlag_OverrideLibrary_Local) != 0;
  }

  return 1;
}

static bool gpencil_edit_modifier_poll(bContext *C)
{
  return gpencil_edit_modifier_poll_generic(C, &RNA_GpencilModifier, 0);
}

static void gpencil_edit_modifier_properties(wmOperatorType *ot)
{
  PropertyRNA *prop = RNA_def_string(
      ot->srna, "modifier", NULL, MAX_NAME, "Modifier", "Name of the modifier to edit");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

static int gpencil_edit_modifier_invoke_properties(bContext *C, wmOperator *op)
{
  GpencilModifierData *md;

  if (RNA_struct_property_is_set(op->ptr, "modifier")) {
    return true;
  }
  else {
    PointerRNA ptr = CTX_data_pointer_get_type(C, "modifier", &RNA_GpencilModifier);
    if (ptr.data) {
      md = ptr.data;
      RNA_string_set(op->ptr, "modifier", md->name);
      return true;
    }
  }

  return false;
}

static GpencilModifierData *gpencil_edit_modifier_property_get(wmOperator *op,
                                                               Object *ob,
                                                               int type)
{
  char modifier_name[MAX_NAME];
  GpencilModifierData *md;
  RNA_string_get(op->ptr, "modifier", modifier_name);

  md = BKE_gpencil_modifiers_findByName(ob, modifier_name);

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

  if (!md || !ED_object_gpencil_modifier_remove(op->reports, bmain, ob, md)) {
    return OPERATOR_CANCELLED;
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static int gpencil_modifier_remove_invoke(bContext *C,
                                          wmOperator *op,
                                          const wmEvent *UNUSED(event))
{
  if (gpencil_edit_modifier_invoke_properties(C, op)) {
    return gpencil_modifier_remove_exec(C, op);
  }
  else {
    return OPERATOR_CANCELLED;
  }
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

static int gpencil_modifier_move_up_invoke(bContext *C,
                                           wmOperator *op,
                                           const wmEvent *UNUSED(event))
{
  if (gpencil_edit_modifier_invoke_properties(C, op)) {
    return gpencil_modifier_move_up_exec(C, op);
  }
  else {
    return OPERATOR_CANCELLED;
  }
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

static int gpencil_modifier_move_down_invoke(bContext *C,
                                             wmOperator *op,
                                             const wmEvent *UNUSED(event))
{
  if (gpencil_edit_modifier_invoke_properties(C, op)) {
    return gpencil_modifier_move_down_exec(C, op);
  }
  else {
    return OPERATOR_CANCELLED;
  }
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

/************************ apply modifier operator *********************/

static int gpencil_modifier_apply_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *ob = ED_object_active_context(C);
  GpencilModifierData *md = gpencil_edit_modifier_property_get(op, ob, 0);
  int apply_as = RNA_enum_get(op->ptr, "apply_as");

  if (!md || !ED_object_gpencil_modifier_apply(bmain, op->reports, depsgraph, ob, md, apply_as)) {
    return OPERATOR_CANCELLED;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static int gpencil_modifier_apply_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  if (gpencil_edit_modifier_invoke_properties(C, op)) {
    return gpencil_modifier_apply_exec(C, op);
  }
  else {
    return OPERATOR_CANCELLED;
  }
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
               "Apply as",
               "How to apply the modifier to the geometry");
  gpencil_edit_modifier_properties(ot);
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

static int gpencil_modifier_copy_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  if (gpencil_edit_modifier_invoke_properties(C, op)) {
    return gpencil_modifier_copy_exec(C, op);
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

void OBJECT_OT_gpencil_modifier_copy(wmOperatorType *ot)
{
  ot->name = "Copy Modifier";
  ot->description = "Duplicate modifier at the same position in the stack";
  ot->idname = "OBJECT_OT_gpencil_modifier_copy";

  ot->invoke = gpencil_modifier_copy_invoke;
  ot->exec = gpencil_modifier_copy_exec;
  ot->poll = gpencil_edit_modifier_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  gpencil_edit_modifier_properties(ot);
}
