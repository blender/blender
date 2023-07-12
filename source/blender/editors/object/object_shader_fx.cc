/* SPDX-FileCopyrightText: 2018 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_shader_fx_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_shader_fx.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"
#include "RNA_prototypes.h"

#include "ED_object.h"
#include "ED_screen.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#include "object_intern.h"

/* -------------------------------------------------------------------- */
/** \name Public API
 * \{ */

ShaderFxData *ED_object_shaderfx_add(
    ReportList *reports, Main *bmain, Scene * /*scene*/, Object *ob, const char *name, int type)
{
  ShaderFxData *new_fx = nullptr;
  const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(ShaderFxType(type));

  if (ob->type != OB_GPENCIL_LEGACY) {
    BKE_reportf(reports, RPT_WARNING, "Effect cannot be added to object '%s'", ob->id.name + 2);
    return nullptr;
  }

  if (fxi->flags & eShaderFxTypeFlag_Single) {
    if (BKE_shaderfx_findby_type(ob, ShaderFxType(type))) {
      BKE_report(reports, RPT_WARNING, "Only one Effect of this type is allowed");
      return nullptr;
    }
  }

  /* get new effect data to add */
  new_fx = BKE_shaderfx_new(type);

  BLI_addtail(&ob->shader_fx, new_fx);

  if (name) {
    STRNCPY_UTF8(new_fx->name, name);
  }

  /* make sure effect data has unique name */
  BKE_shaderfx_unique_name(&ob->shader_fx, new_fx);

  bGPdata *gpd = static_cast<bGPdata *>(ob->data);
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  DEG_relations_tag_update(bmain);

  return new_fx;
}

/* Return true if the object has a effect of type 'type' other than
 * the shaderfx pointed to be 'exclude', otherwise returns false. */
static bool UNUSED_FUNCTION(object_has_shaderfx)(const Object *ob,
                                                 const ShaderFxData *exclude,
                                                 ShaderFxType type)
{
  ShaderFxData *fx;

  for (fx = static_cast<ShaderFxData *>(ob->shader_fx.first); fx; fx = fx->next) {
    if ((fx != exclude) && (fx->type == type)) {
      return true;
    }
  }

  return false;
}

static bool object_shaderfx_remove(Main *bmain,
                                   Object *ob,
                                   ShaderFxData *fx,
                                   bool * /*r_sort_depsgraph*/)
{
  /* It seems on rapid delete it is possible to
   * get called twice on same effect, so make
   * sure it is in list. */
  if (BLI_findindex(&ob->shader_fx, fx) == -1) {
    return 0;
  }

  DEG_relations_tag_update(bmain);

  BLI_remlink(&ob->shader_fx, fx);
  BKE_shaderfx_free(fx);
  BKE_object_free_derived_caches(ob);

  return 1;
}

bool ED_object_shaderfx_remove(ReportList *reports, Main *bmain, Object *ob, ShaderFxData *fx)
{
  bool sort_depsgraph = false;
  bool ok;

  ok = object_shaderfx_remove(bmain, ob, fx, &sort_depsgraph);

  if (!ok) {
    BKE_reportf(reports, RPT_ERROR, "Effect '%s' not in object '%s'", fx->name, ob->id.name);
    return 0;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  DEG_relations_tag_update(bmain);

  return 1;
}

void ED_object_shaderfx_clear(Main *bmain, Object *ob)
{
  ShaderFxData *fx = static_cast<ShaderFxData *>(ob->shader_fx.first);
  bool sort_depsgraph = false;

  if (!fx) {
    return;
  }

  while (fx) {
    ShaderFxData *next_fx;

    next_fx = fx->next;

    object_shaderfx_remove(bmain, ob, fx, &sort_depsgraph);

    fx = next_fx;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  DEG_relations_tag_update(bmain);
}

int ED_object_shaderfx_move_up(ReportList * /*reports*/, Object *ob, ShaderFxData *fx)
{
  if (fx->prev) {
    BLI_remlink(&ob->shader_fx, fx);
    BLI_insertlinkbefore(&ob->shader_fx, fx->prev, fx);
  }

  return 1;
}

int ED_object_shaderfx_move_down(ReportList * /*reports*/, Object *ob, ShaderFxData *fx)
{
  if (fx->next) {
    BLI_remlink(&ob->shader_fx, fx);
    BLI_insertlinkafter(&ob->shader_fx, fx->next, fx);
  }

  return 1;
}

bool ED_object_shaderfx_move_to_index(ReportList *reports,
                                      Object *ob,
                                      ShaderFxData *fx,
                                      const int index)
{
  BLI_assert(fx != nullptr);
  BLI_assert(index >= 0);
  if (index >= BLI_listbase_count(&ob->shader_fx)) {
    BKE_report(reports, RPT_WARNING, "Cannot move effect beyond the end of the stack");
    return false;
  }

  int fx_index = BLI_findindex(&ob->shader_fx, fx);
  BLI_assert(fx_index != -1);
  if (fx_index < index) {
    /* Move shaderfx down in list. */
    for (; fx_index < index; fx_index++) {
      if (!ED_object_shaderfx_move_down(reports, ob, fx)) {
        break;
      }
    }
  }
  else {
    /* Move shaderfx up in list. */
    for (; fx_index > index; fx_index--) {
      if (!ED_object_shaderfx_move_up(reports, ob, fx)) {
        break;
      }
    }
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_OBJECT | ND_SHADERFX, ob);

  return true;
}

void ED_object_shaderfx_link(Object *dst, Object *src)
{
  BLI_freelistN(&dst->shader_fx);
  BKE_shaderfx_copy(&dst->shader_fx, &src->shader_fx);

  DEG_id_tag_update(&dst->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_OBJECT | ND_SHADERFX, dst);
}

void ED_object_shaderfx_copy(Object *dst, ShaderFxData *fx)
{
  ShaderFxData *nfx = BKE_shaderfx_new(fx->type);
  STRNCPY(nfx->name, fx->name);
  BKE_shaderfx_copydata(fx, nfx);
  BLI_addtail(&dst->shader_fx, nfx);

  DEG_id_tag_update(&dst->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_OBJECT | ND_SHADERFX, dst);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic Poll Callback Helpers
 * \{ */

static bool edit_shaderfx_poll_generic(bContext *C,
                                       StructRNA *rna_type,
                                       int obtype_flag,
                                       const bool is_liboverride_allowed)
{
  PointerRNA ptr = CTX_data_pointer_get_type(C, "shaderfx", rna_type);
  Object *ob = (ptr.owner_id) ? (Object *)ptr.owner_id : ED_object_active_context(C);
  ShaderFxData *fx = static_cast<ShaderFxData *>(ptr.data); /* May be nullptr. */

  if (!ED_operator_object_active_editable_ex(C, ob)) {
    return false;
  }

  /* NOTE: Temporary 'forbid all' for overrides, until we implement support to add shaderfx to
   * overrides. */
  if (ID_IS_OVERRIDE_LIBRARY(ob)) {
    CTX_wm_operator_poll_msg_set(C, "Cannot edit shaderfxs in a library override");
    return false;
  }

  if (obtype_flag != 0 && ((1 << ob->type) & obtype_flag) == 0) {
    CTX_wm_operator_poll_msg_set(C, "Object type is not supported");
    return false;
  }
  if (ptr.owner_id != nullptr && !BKE_id_is_editable(CTX_data_main(C), ptr.owner_id)) {
    CTX_wm_operator_poll_msg_set(C, "Cannot edit library or override data");
    return false;
  }
  if (!is_liboverride_allowed && BKE_shaderfx_is_nonlocal_in_liboverride(ob, fx)) {
    CTX_wm_operator_poll_msg_set(
        C, "Cannot edit shaderfxs coming from linked data in a library override");
    return false;
  }

  return true;
}

static bool edit_shaderfx_poll(bContext *C)
{
  return edit_shaderfx_poll_generic(C, &RNA_ShaderFx, 0, false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Effect Operator
 * \{ */

static int shaderfx_add_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = ED_object_active_context(C);
  int type = RNA_enum_get(op->ptr, "type");

  if (!ED_object_shaderfx_add(op->reports, bmain, scene, ob, nullptr, type)) {
    return OPERATOR_CANCELLED;
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_SHADERFX, ob);

  return OPERATOR_FINISHED;
}

static const EnumPropertyItem *shaderfx_add_itemf(bContext *C,
                                                  PointerRNA * /*ptr*/,
                                                  PropertyRNA * /*prop*/,
                                                  bool *r_free)
{
  Object *ob = ED_object_active_context(C);
  EnumPropertyItem *item = nullptr;
  const EnumPropertyItem *fx_item, *group_item = nullptr;
  const ShaderFxTypeInfo *mti;
  int totitem = 0, a;

  if (!ob) {
    return rna_enum_object_shaderfx_type_items;
  }

  for (a = 0; rna_enum_object_shaderfx_type_items[a].identifier; a++) {
    fx_item = &rna_enum_object_shaderfx_type_items[a];
    if (fx_item->identifier[0]) {
      mti = BKE_shaderfx_get_info(ShaderFxType(fx_item->value));

      if (mti->flags & eShaderFxTypeFlag_NoUserAdd) {
        continue;
      }
    }
    else {
      group_item = fx_item;
      fx_item = nullptr;

      continue;
    }

    if (group_item) {
      RNA_enum_item_add(&item, &totitem, group_item);
      group_item = nullptr;
    }

    RNA_enum_item_add(&item, &totitem, fx_item);
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

void OBJECT_OT_shaderfx_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Effect";
  ot->description = "Add a visual effect to the active object";
  ot->idname = "OBJECT_OT_shaderfx_add";

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = shaderfx_add_exec;
  ot->poll = edit_shaderfx_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(
      ot->srna, "type", rna_enum_object_shaderfx_type_items, eShaderFxType_Blur, "Type", "");
  RNA_def_enum_funcs(ot->prop, shaderfx_add_itemf);

  /* Abused, for "Light"... */
  RNA_def_property_translation_context(ot->prop, BLT_I18NCONTEXT_ID_ID);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic Functions for Operators Using Names and Data Context
 * \{ */

static void edit_shaderfx_properties(wmOperatorType *ot)
{
  PropertyRNA *prop = RNA_def_string(
      ot->srna, "shaderfx", nullptr, MAX_NAME, "Shader", "Name of the shaderfx to edit");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

static void edit_shaderfx_report_property(wmOperatorType *ot)
{
  PropertyRNA *prop = RNA_def_boolean(
      ot->srna, "report", false, "Report", "Create a notification after the operation");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/**
 * \param event: If this isn't nullptr, the operator will also look for panels underneath
 * the cursor with custom-data set to a modifier.
 * \param r_retval: This should be used if #event is used in order to return
 * #OPERATOR_PASS_THROUGH to check other operators with the same key set.
 */
static bool edit_shaderfx_invoke_properties(bContext *C,
                                            wmOperator *op,
                                            const wmEvent *event,
                                            int *r_retval)
{
  if (RNA_struct_property_is_set(op->ptr, "shaderfx")) {
    return true;
  }

  PointerRNA ctx_ptr = CTX_data_pointer_get_type(C, "shaderfx", &RNA_ShaderFx);
  if (ctx_ptr.data != nullptr) {
    ShaderFxData *fx = static_cast<ShaderFxData *>(ctx_ptr.data);
    RNA_string_set(op->ptr, "shaderfx", fx->name);
    return true;
  }

  /* Check the custom data of panels under the mouse for an effect. */
  if (event != nullptr) {
    PointerRNA *panel_ptr = UI_region_panel_custom_data_under_cursor(C, event);

    if (!(panel_ptr == nullptr || RNA_pointer_is_null(panel_ptr))) {
      if (RNA_struct_is_a(panel_ptr->type, &RNA_ShaderFx)) {
        ShaderFxData *fx = static_cast<ShaderFxData *>(panel_ptr->data);
        RNA_string_set(op->ptr, "shaderfx", fx->name);
        return true;
      }

      BLI_assert(r_retval != nullptr); /* We need the return value in this case. */
      if (r_retval != nullptr) {
        *r_retval = (OPERATOR_PASS_THROUGH | OPERATOR_CANCELLED);
      }
      return false;
    }
  }

  if (r_retval != nullptr) {
    *r_retval = OPERATOR_CANCELLED;
  }
  return false;
}

static ShaderFxData *edit_shaderfx_property_get(wmOperator *op, Object *ob, int type)
{
  char shaderfx_name[MAX_NAME];
  ShaderFxData *fx;
  RNA_string_get(op->ptr, "shaderfx", shaderfx_name);

  fx = BKE_shaderfx_findby_name(ob, shaderfx_name);

  if (fx && type != 0 && fx->type != type) {
    fx = nullptr;
  }

  return fx;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remove ShaderFX Operator
 * \{ */

static int shaderfx_remove_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = ED_object_active_context(C);
  ShaderFxData *fx = edit_shaderfx_property_get(op, ob, 0);
  if (!fx) {
    return OPERATOR_CANCELLED;
  }

  /* Store name temporarily for report. */
  char name[MAX_NAME];
  STRNCPY(name, fx->name);

  if (!ED_object_shaderfx_remove(op->reports, bmain, ob, fx)) {
    return OPERATOR_CANCELLED;
  }

  if (RNA_boolean_get(op->ptr, "report")) {
    BKE_reportf(op->reports, RPT_INFO, "Removed effect: %s", name);
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_SHADERFX, ob);

  return OPERATOR_FINISHED;
}

static int shaderfx_remove_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  int retval;
  if (edit_shaderfx_invoke_properties(C, op, event, &retval)) {
    return shaderfx_remove_exec(C, op);
  }
  return retval;
}

void OBJECT_OT_shaderfx_remove(wmOperatorType *ot)
{
  ot->name = "Remove Grease Pencil Effect";
  ot->description = "Remove a effect from the active grease pencil object";
  ot->idname = "OBJECT_OT_shaderfx_remove";

  ot->invoke = shaderfx_remove_invoke;
  ot->exec = shaderfx_remove_exec;
  ot->poll = edit_shaderfx_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_shaderfx_properties(ot);
  edit_shaderfx_report_property(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Move up ShaderFX Operator
 * \{ */

static int shaderfx_move_up_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_active_context(C);
  ShaderFxData *fx = edit_shaderfx_property_get(op, ob, 0);

  if (!fx || !ED_object_shaderfx_move_up(op->reports, ob, fx)) {
    return OPERATOR_CANCELLED;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_SHADERFX, ob);

  return OPERATOR_FINISHED;
}

static int shaderfx_move_up_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  int retval;
  if (edit_shaderfx_invoke_properties(C, op, event, &retval)) {
    return shaderfx_move_up_exec(C, op);
  }
  return retval;
}

void OBJECT_OT_shaderfx_move_up(wmOperatorType *ot)
{
  ot->name = "Move Up Effect";
  ot->description = "Move effect up in the stack";
  ot->idname = "OBJECT_OT_shaderfx_move_up";

  ot->invoke = shaderfx_move_up_invoke;
  ot->exec = shaderfx_move_up_exec;
  ot->poll = edit_shaderfx_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_shaderfx_properties(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Move Down ShaderFX Operator
 * \{ */

static int shaderfx_move_down_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_active_context(C);
  ShaderFxData *fx = edit_shaderfx_property_get(op, ob, 0);

  if (!fx || !ED_object_shaderfx_move_down(op->reports, ob, fx)) {
    return OPERATOR_CANCELLED;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_SHADERFX, ob);

  return OPERATOR_FINISHED;
}

static int shaderfx_move_down_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  int retval;
  if (edit_shaderfx_invoke_properties(C, op, event, &retval)) {
    return shaderfx_move_down_exec(C, op);
  }
  return retval;
}

void OBJECT_OT_shaderfx_move_down(wmOperatorType *ot)
{
  ot->name = "Move Down Effect";
  ot->description = "Move effect down in the stack";
  ot->idname = "OBJECT_OT_shaderfx_move_down";

  ot->invoke = shaderfx_move_down_invoke;
  ot->exec = shaderfx_move_down_exec;
  ot->poll = edit_shaderfx_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_shaderfx_properties(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Move ShaderFX to Index Operator
 * \{ */

static int shaderfx_move_to_index_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_active_context(C);
  ShaderFxData *fx = edit_shaderfx_property_get(op, ob, 0);
  int index = RNA_int_get(op->ptr, "index");

  if (!fx || !ED_object_shaderfx_move_to_index(op->reports, ob, fx, index)) {
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

static int shaderfx_move_to_index_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  int retval;
  if (edit_shaderfx_invoke_properties(C, op, event, &retval)) {
    return shaderfx_move_to_index_exec(C, op);
  }
  return retval;
}

void OBJECT_OT_shaderfx_move_to_index(wmOperatorType *ot)
{
  ot->name = "Move Effect to Index";
  ot->idname = "OBJECT_OT_shaderfx_move_to_index";
  ot->description =
      "Change the effect's position in the list so it evaluates after the set number of "
      "others";

  ot->invoke = shaderfx_move_to_index_invoke;
  ot->exec = shaderfx_move_to_index_exec;
  ot->poll = edit_shaderfx_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_shaderfx_properties(ot);
  RNA_def_int(
      ot->srna, "index", 0, 0, INT_MAX, "Index", "The index to move the effect to", 0, INT_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Copy Shader Operator
 * \{ */

static int shaderfx_copy_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_active_context(C);
  ShaderFxData *fx = edit_shaderfx_property_get(op, ob, 0);
  if (!fx) {
    return OPERATOR_CANCELLED;
  }
  ShaderFxData *nfx = BKE_shaderfx_new(fx->type);
  if (!nfx) {
    return OPERATOR_CANCELLED;
  }

  STRNCPY(nfx->name, fx->name);
  /* Make sure effect data has unique name. */
  BKE_shaderfx_unique_name(&ob->shader_fx, nfx);

  BKE_shaderfx_copydata(fx, nfx);
  BLI_insertlinkafter(&ob->shader_fx, fx, nfx);

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_OBJECT | ND_SHADERFX, ob);

  return OPERATOR_FINISHED;
}

static int shaderfx_copy_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  int retval;
  if (edit_shaderfx_invoke_properties(C, op, event, &retval)) {
    return shaderfx_copy_exec(C, op);
  }
  return retval;
}

void OBJECT_OT_shaderfx_copy(wmOperatorType *ot)
{
  ot->name = "Copy Effect";
  ot->description = "Duplicate effect at the same position in the stack";
  ot->idname = "OBJECT_OT_shaderfx_copy";

  ot->invoke = shaderfx_copy_invoke;
  ot->exec = shaderfx_copy_exec;
  ot->poll = edit_shaderfx_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_shaderfx_properties(ot);
}

/** \} */
