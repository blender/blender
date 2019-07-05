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
 * The Original Code is Copyright (C) 2004 Blender Foundation
 * All rights reserved.
 */

/** \file
 * \ingroup edundo
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "BLI_utildefines.h"
#include "BLI_callbacks.h"
#include "BLI_listbase.h"

#include "BLT_translation.h"

#include "BKE_blender_undo.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_layer.h"
#include "BKE_undo_system.h"
#include "BKE_workspace.h"
#include "BKE_paint.h"

#include "BLO_blend_validate.h"

#include "ED_gpencil.h"
#include "ED_render.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_undo.h"

#include "WM_api.h"
#include "WM_types.h"
#include "WM_toolsystem.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"
#include "UI_resources.h"

/** We only need this locally. */
static CLG_LogRef LOG = {"ed.undo"};

/* -------------------------------------------------------------------- */
/** \name Generic Undo System Access
 *
 * Non-operator undo editor functions.
 * \{ */

void ED_undo_push(bContext *C, const char *str)
{
  CLOG_INFO(&LOG, 1, "name='%s'", str);

  const int steps = U.undosteps;

  if (steps <= 0) {
    return;
  }

  wmWindowManager *wm = CTX_wm_manager(C);

  /* Only apply limit if this is the last undo step. */
  if (wm->undo_stack->step_active && (wm->undo_stack->step_active->next == NULL)) {
    BKE_undosys_stack_limit_steps_and_memory(wm->undo_stack, steps - 1, 0);
  }

  BKE_undosys_step_push(wm->undo_stack, C, str);

  if (U.undomemory != 0) {
    const size_t memory_limit = (size_t)U.undomemory * 1024 * 1024;
    BKE_undosys_stack_limit_steps_and_memory(wm->undo_stack, 0, memory_limit);
  }

  WM_file_tag_modified();
}

/**
 * \note Also check #undo_history_exec in bottom if you change notifiers.
 */
static int ed_undo_step_impl(
    bContext *C, int step, const char *undoname, int undo_index, ReportList *reports)
{
  /* Mutually exclusives, ensure correct input. */
  BLI_assert(((undoname || undo_index != -1) && !step) ||
             (!(undoname || undo_index != -1) && step));
  CLOG_INFO(&LOG, 1, "name='%s', step=%d", undoname, step);
  wmWindowManager *wm = CTX_wm_manager(C);
  Scene *scene = CTX_data_scene(C);
  ScrArea *sa = CTX_wm_area(C);

  /* undo during jobs are running can easily lead to freeing data using by jobs,
   * or they can just lead to freezing job in some other cases */
  WM_jobs_kill_all(wm);

  if (G.debug & G_DEBUG_IO) {
    Main *bmain = CTX_data_main(C);
    if (bmain->lock != NULL) {
      BKE_report(reports, RPT_INFO, "Checking sanity of current .blend file *BEFORE* undo step");
      BLO_main_validate_libraries(bmain, reports);
    }
  }

  /* TODO(campbell): undo_system: use undo system */
  /* grease pencil can be can be used in plenty of spaces, so check it first */
  if (ED_gpencil_session_active()) {
    return ED_undo_gpencil_step(C, step, undoname);
  }
  if (sa && (sa->spacetype == SPACE_VIEW3D)) {
    Object *obact = CTX_data_active_object(C);
    if (obact && (obact->type == OB_GPENCIL)) {
      ED_gpencil_toggle_brush_cursor(C, false, NULL);
    }
  }

  UndoStep *step_data_from_name = NULL;
  int step_for_callback = step;
  if (undoname != NULL) {
    step_data_from_name = BKE_undosys_step_find_by_name(wm->undo_stack, undoname);
    if (step_data_from_name == NULL) {
      return OPERATOR_CANCELLED;
    }

    /* TODO(campbell), could use simple optimization. */
    /* Pointers match on redo. */
    step_for_callback = (BLI_findindex(&wm->undo_stack->steps, step_data_from_name) <
                         BLI_findindex(&wm->undo_stack->steps, wm->undo_stack->step_active)) ?
                            1 :
                            -1;
  }
  else if (undo_index != -1) {
    step_for_callback = (undo_index <
                         BLI_findindex(&wm->undo_stack->steps, wm->undo_stack->step_active)) ?
                            1 :
                            -1;
  }

  /* App-Handlers (pre). */
  {
    /* Note: ignore grease pencil for now. */
    Main *bmain = CTX_data_main(C);
    wm->op_undo_depth++;
    BLI_callback_exec(
        bmain, &scene->id, (step_for_callback > 0) ? BLI_CB_EVT_UNDO_PRE : BLI_CB_EVT_REDO_PRE);
    wm->op_undo_depth--;
  }

  /* Undo System */
  {
    if (undoname) {
      BKE_undosys_step_undo_with_data(wm->undo_stack, C, step_data_from_name);
    }
    else if (undo_index != -1) {
      BKE_undosys_step_undo_from_index(wm->undo_stack, C, undo_index);
    }
    else {
      if (step == 1) {
        BKE_undosys_step_undo(wm->undo_stack, C);
      }
      else {
        BKE_undosys_step_redo(wm->undo_stack, C);
      }
    }

    /* Set special modes for grease pencil */
    if (sa && (sa->spacetype == SPACE_VIEW3D)) {
      Object *obact = CTX_data_active_object(C);
      if (obact && (obact->type == OB_GPENCIL)) {
        /* set cursor */
        if (ELEM(obact->mode,
                 OB_MODE_PAINT_GPENCIL,
                 OB_MODE_SCULPT_GPENCIL,
                 OB_MODE_WEIGHT_GPENCIL)) {
          ED_gpencil_toggle_brush_cursor(C, true, NULL);
        }
        else {
          ED_gpencil_toggle_brush_cursor(C, false, NULL);
        }
        /* set workspace mode */
        Base *basact = CTX_data_active_base(C);
        ED_object_base_activate(C, basact);
      }
    }
  }

  /* App-Handlers (post). */
  {
    Main *bmain = CTX_data_main(C);
    scene = CTX_data_scene(C);
    wm->op_undo_depth++;
    BLI_callback_exec(
        bmain, &scene->id, step_for_callback > 0 ? BLI_CB_EVT_UNDO_POST : BLI_CB_EVT_REDO_POST);
    wm->op_undo_depth--;
  }

  if (G.debug & G_DEBUG_IO) {
    Main *bmain = CTX_data_main(C);
    if (bmain->lock != NULL) {
      BKE_report(reports, RPT_INFO, "Checking sanity of current .blend file *AFTER* undo step");
      BLO_main_validate_libraries(bmain, reports);
    }
  }

  WM_event_add_notifier(C, NC_WINDOW, NULL);
  WM_event_add_notifier(C, NC_WM | ND_UNDO, NULL);

  WM_toolsystem_refresh_active(C);

  Main *bmain = CTX_data_main(C);
  WM_toolsystem_refresh_screen_all(bmain);

  return OPERATOR_FINISHED;
}

static int ed_undo_step_direction(bContext *C, int step, ReportList *reports)
{
  return ed_undo_step_impl(C, step, NULL, -1, reports);
}

static int ed_undo_step_by_name(bContext *C, const char *undo_name, ReportList *reports)
{
  return ed_undo_step_impl(C, 0, undo_name, -1, reports);
}

static int ed_undo_step_by_index(bContext *C, int index, ReportList *reports)
{
  return ed_undo_step_impl(C, 0, NULL, index, reports);
}

void ED_undo_grouped_push(bContext *C, const char *str)
{
  /* do nothing if previous undo task is the same as this one (or from the same undo group) */
  wmWindowManager *wm = CTX_wm_manager(C);
  const UndoStep *us = wm->undo_stack->step_active;
  if (us && STREQ(str, us->name)) {
    BKE_undosys_stack_clear_active(wm->undo_stack);
  }

  /* push as usual */
  ED_undo_push(C, str);
}

void ED_undo_pop(bContext *C)
{
  ed_undo_step_direction(C, 1, NULL);
}
void ED_undo_redo(bContext *C)
{
  ed_undo_step_direction(C, -1, NULL);
}

void ED_undo_push_op(bContext *C, wmOperator *op)
{
  /* in future, get undo string info? */
  ED_undo_push(C, op->type->name);
}

void ED_undo_grouped_push_op(bContext *C, wmOperator *op)
{
  if (op->type->undo_group[0] != '\0') {
    ED_undo_grouped_push(C, op->type->undo_group);
  }
  else {
    ED_undo_grouped_push(C, op->type->name);
  }
}

void ED_undo_pop_op(bContext *C, wmOperator *op)
{
  /* search back a couple of undo's, in case something else added pushes */
  ed_undo_step_by_name(C, op->type->name, op->reports);
}

/* name optionally, function used to check for operator redo panel */
bool ED_undo_is_valid(const bContext *C, const char *undoname)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  return BKE_undosys_stack_has_undo(wm->undo_stack, undoname);
}

bool ED_undo_is_memfile_compatible(const bContext *C)
{
  /* Some modes don't co-exist with memfile undo, disable their use: T60593
   * (this matches 2.7x behavior). */
  ViewLayer *view_layer = CTX_data_view_layer(C);
  if (view_layer != NULL) {
    Object *obact = OBACT(view_layer);
    if (obact != NULL) {
      if (obact->mode & (OB_MODE_SCULPT | OB_MODE_EDIT)) {
        return false;
      }
    }
  }
  return true;
}

/**
 * When a property of ID changes, return false.
 *
 * This is to avoid changes to a property making undo pushes
 * which are ignored by the undo-system.
 * For example, changing a brush property isn't stored by sculpt-mode undo steps.
 * This workaround is needed until the limitation is removed, see: T61948.
 */
bool ED_undo_is_legacy_compatible_for_property(struct bContext *C, ID *id)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  if (view_layer != NULL) {
    Object *obact = OBACT(view_layer);
    if (obact != NULL) {
      if (obact->mode & OB_MODE_ALL_PAINT) {
        /* Don't store property changes when painting
         * (only do undo pushes on brush strokes which each paint operator handles on it's own). */
        CLOG_INFO(&LOG, 1, "skipping undo for paint-mode");
        return false;
      }
      else if (obact->mode & OB_MODE_EDIT) {
        if ((id == NULL) || (obact->data == NULL) ||
            (GS(id->name) != GS(((ID *)obact->data)->name))) {
          /* No undo push on id type mismatch in edit-mode. */
          CLOG_INFO(&LOG, 1, "skipping undo for edit-mode");
          return false;
        }
      }
    }
  }
  return true;
}

/**
 * Ideally we wont access the stack directly,
 * this is needed for modes which handle undo themselves (bypassing #ED_undo_push).
 *
 * Using global isn't great, this just avoids doing inline,
 * causing 'BKE_global.h' & 'BKE_main.h' includes.
 */
UndoStack *ED_undo_stack_get(void)
{
  wmWindowManager *wm = G_MAIN->wm.first;
  return wm->undo_stack;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Undo, Undo Push & Redo Operators
 * \{ */

static int ed_undo_exec(bContext *C, wmOperator *op)
{
  /* "last operator" should disappear, later we can tie this with undo stack nicer */
  WM_operator_stack_clear(CTX_wm_manager(C));
  int ret = ed_undo_step_direction(C, 1, op->reports);
  if (ret & OPERATOR_FINISHED) {
    /* Keep button under the cursor active. */
    WM_event_add_mousemove(C);
  }
  return ret;
}

static int ed_undo_push_exec(bContext *C, wmOperator *op)
{
  if (G.background) {
    /* Exception for background mode, see: T60934.
     * Note: since the undo stack isn't initialized on startup, background mode behavior
     * won't match regular usage, this is just for scripts to do explicit undo pushes. */
    wmWindowManager *wm = CTX_wm_manager(C);
    if (wm->undo_stack == NULL) {
      wm->undo_stack = BKE_undosys_stack_create();
    }
  }
  char str[BKE_UNDO_STR_MAX];
  RNA_string_get(op->ptr, "message", str);
  ED_undo_push(C, str);
  return OPERATOR_FINISHED;
}

static int ed_redo_exec(bContext *C, wmOperator *op)
{
  int ret = ed_undo_step_direction(C, -1, op->reports);
  if (ret & OPERATOR_FINISHED) {
    /* Keep button under the cursor active. */
    WM_event_add_mousemove(C);
  }
  return ret;
}

static int ed_undo_redo_exec(bContext *C, wmOperator *UNUSED(op))
{
  wmOperator *last_op = WM_operator_last_redo(C);
  int ret = ED_undo_operator_repeat(C, last_op);
  ret = ret ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
  if (ret & OPERATOR_FINISHED) {
    /* Keep button under the cursor active. */
    WM_event_add_mousemove(C);
  }
  return ret;
}

/* Disable in background mode, we could support if it's useful, T60934. */

static bool ed_undo_is_init_poll(bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  if (wm->undo_stack == NULL) {
    CTX_wm_operator_poll_msg_set(C, "Undo disabled at startup");
    return false;
  }
  return true;
}

static bool ed_undo_is_init_and_screenactive_poll(bContext *C)
{
  if (ed_undo_is_init_poll(C) == false) {
    return false;
  }
  return ED_operator_screenactive(C);
}

static bool ed_undo_redo_poll(bContext *C)
{
  wmOperator *last_op = WM_operator_last_redo(C);
  return (last_op && ed_undo_is_init_and_screenactive_poll(C) &&
          WM_operator_check_ui_enabled(C, last_op->type->name));
}

void ED_OT_undo(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Undo";
  ot->description = "Undo previous action";
  ot->idname = "ED_OT_undo";

  /* api callbacks */
  ot->exec = ed_undo_exec;
  ot->poll = ed_undo_is_init_and_screenactive_poll;
}

void ED_OT_undo_push(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Undo Push";
  ot->description = "Add an undo state (internal use only)";
  ot->idname = "ED_OT_undo_push";

  /* api callbacks */
  ot->exec = ed_undo_push_exec;
  /* Unlike others undo operators this initializes undo stack. */
  ot->poll = ED_operator_screenactive;

  ot->flag = OPTYPE_INTERNAL;

  RNA_def_string(ot->srna,
                 "message",
                 "Add an undo step *function may be moved*",
                 BKE_UNDO_STR_MAX,
                 "Undo Message",
                 "");
}

void ED_OT_redo(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Redo";
  ot->description = "Redo previous action";
  ot->idname = "ED_OT_redo";

  /* api callbacks */
  ot->exec = ed_redo_exec;
  ot->poll = ed_undo_is_init_and_screenactive_poll;
}

void ED_OT_undo_redo(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Undo and Redo";
  ot->description = "Undo and redo previous action";
  ot->idname = "ED_OT_undo_redo";

  /* api callbacks */
  ot->exec = ed_undo_redo_exec;
  ot->poll = ed_undo_redo_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator Repeat
 * \{ */

/* ui callbacks should call this rather than calling WM_operator_repeat() themselves */
int ED_undo_operator_repeat(bContext *C, wmOperator *op)
{
  int ret = 0;

  if (op) {
    CLOG_INFO(&LOG, 1, "idname='%s'", op->type->idname);
    wmWindowManager *wm = CTX_wm_manager(C);
    struct Scene *scene = CTX_data_scene(C);

    /* keep in sync with logic in view3d_panel_operator_redo() */
    ARegion *ar_orig = CTX_wm_region(C);
    ARegion *ar_win = BKE_area_find_region_active_win(CTX_wm_area(C));

    if (ar_win) {
      CTX_wm_region_set(C, ar_win);
    }

    if ((WM_operator_repeat_check(C, op)) && (WM_operator_poll(C, op->type)) &&
        /* note, undo/redo cant run if there are jobs active,
         * check for screen jobs only so jobs like material/texture/world preview
         * (which copy their data), wont stop redo, see [#29579]],
         *
         * note, - WM_operator_check_ui_enabled() jobs test _must_ stay in sync with this */
        (WM_jobs_test(wm, scene, WM_JOB_TYPE_ANY) == 0)) {
      int retval;

      if (G.debug & G_DEBUG) {
        printf("redo_cb: operator redo %s\n", op->type->name);
      }

      WM_operator_free_all_after(wm, op);

      ED_undo_pop_op(C, op);

      if (op->type->check) {
        if (op->type->check(C, op)) {
          /* check for popup and re-layout buttons */
          ARegion *ar_menu = CTX_wm_menu(C);
          if (ar_menu) {
            ED_region_tag_refresh_ui(ar_menu);
          }
        }
      }

      if (op->type->flag & OPTYPE_USE_EVAL_DATA) {
        /* We need to force refresh of depsgraph after undo step,
         * redoing the operator *may* rely on some valid evaluated data. */
        Main *bmain = CTX_data_main(C);
        scene = CTX_data_scene(C);
        ViewLayer *view_layer = CTX_data_view_layer(C);
        BKE_scene_view_layer_graph_evaluated_ensure(bmain, scene, view_layer);
      }

      retval = WM_operator_repeat(C, op);
      if ((retval & OPERATOR_FINISHED) == 0) {
        if (G.debug & G_DEBUG) {
          printf("redo_cb: operator redo failed: %s, return %d\n", op->type->name, retval);
        }
        ED_undo_redo(C);
      }
      else {
        ret = 1;
      }
    }
    else {
      if (G.debug & G_DEBUG) {
        printf("redo_cb: WM_operator_repeat_check returned false %s\n", op->type->name);
      }
    }

    /* set region back */
    CTX_wm_region_set(C, ar_orig);
  }
  else {
    CLOG_WARN(&LOG, "called with NULL 'op'");
  }

  return ret;
}

void ED_undo_operator_repeat_cb(bContext *C, void *arg_op, void *UNUSED(arg_unused))
{
  ED_undo_operator_repeat(C, (wmOperator *)arg_op);
}

void ED_undo_operator_repeat_cb_evt(bContext *C, void *arg_op, int UNUSED(arg_event))
{
  ED_undo_operator_repeat(C, (wmOperator *)arg_op);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Undo History Operator
 * \{ */

/* create enum based on undo items */
static const EnumPropertyItem *rna_undo_itemf(bContext *C, int *totitem)
{
  EnumPropertyItem item_tmp = {0}, *item = NULL;
  int i = 0;

  wmWindowManager *wm = CTX_wm_manager(C);
  if (wm->undo_stack == NULL) {
    return NULL;
  }

  for (UndoStep *us = wm->undo_stack->steps.first; us; us = us->next, i++) {
    if (us->skip == false) {
      item_tmp.identifier = us->name;
      item_tmp.name = IFACE_(us->name);
      if (us == wm->undo_stack->step_active) {
        item_tmp.icon = ICON_LAYER_ACTIVE;
      }
      else {
        item_tmp.icon = ICON_NONE;
      }
      item_tmp.value = i;
      RNA_enum_item_add(&item, totitem, &item_tmp);
    }
  }
  RNA_enum_item_end(&item, totitem);

  return item;
}

static int undo_history_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  int totitem = 0;

  {
    const EnumPropertyItem *item = rna_undo_itemf(C, &totitem);

    if (totitem > 0) {
      uiPopupMenu *pup = UI_popup_menu_begin(
          C, WM_operatortype_name(op->type, op->ptr), ICON_NONE);
      uiLayout *layout = UI_popup_menu_layout(pup);
      uiLayout *split = uiLayoutSplit(layout, 0.0f, false);
      uiLayout *column = NULL;
      const int col_size = 20 + totitem / 12;
      int i, c;
      bool add_col = true;

      for (c = 0, i = totitem; i--;) {
        if (add_col && !(c % col_size)) {
          column = uiLayoutColumn(split, false);
          add_col = false;
        }
        if (item[i].identifier) {
          uiItemIntO(column, item[i].name, item[i].icon, op->type->idname, "item", item[i].value);
          ++c;
          add_col = true;
        }
      }

      MEM_freeN((void *)item);

      UI_popup_menu_end(C, pup);
    }
  }
  return OPERATOR_CANCELLED;
}

/* note: also check ed_undo_step() in top if you change notifiers */
static int undo_history_exec(bContext *C, wmOperator *op)
{
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "item");
  if (RNA_property_is_set(op->ptr, prop)) {
    int item = RNA_property_int_get(op->ptr, prop);
    WM_operator_stack_clear(CTX_wm_manager(C));
    ed_undo_step_by_index(C, item, op->reports);
    WM_event_add_notifier(C, NC_WINDOW, NULL);
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void ED_OT_undo_history(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Undo History";
  ot->description = "Redo specific action in history";
  ot->idname = "ED_OT_undo_history";

  /* api callbacks */
  ot->invoke = undo_history_invoke;
  ot->exec = undo_history_exec;
  ot->poll = ed_undo_is_init_and_screenactive_poll;

  RNA_def_int(ot->srna, "item", 0, 0, INT_MAX, "Item", "", 0, INT_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Undo Helper Functions
 * \{ */

void ED_undo_object_set_active_or_warn(ViewLayer *view_layer,
                                       Object *ob,
                                       const char *info,
                                       CLG_LogRef *log)
{
  Object *ob_prev = OBACT(view_layer);
  if (ob_prev != ob) {
    Base *base = BKE_view_layer_base_find(view_layer, ob);
    if (base != NULL) {
      view_layer->basact = base;
    }
    else {
      /* Should never fail, may not crash but can give odd behavior. */
      CLOG_WARN(log, "'%s' failed to restore active object: '%s'", info, ob->id.name + 2);
    }
  }
}

void ED_undo_object_editmode_restore_helper(struct bContext *C,
                                            Object **object_array,
                                            uint object_array_len,
                                            uint object_array_stride)
{
  Main *bmain = CTX_data_main(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint bases_len = 0;
  /* Don't request unique data because we wan't to de-select objects when exiting edit-mode
   * for that to be done on all objects we can't skip ones that share data. */
  Base **bases = BKE_view_layer_array_from_bases_in_edit_mode(view_layer, NULL, &bases_len);
  for (uint i = 0; i < bases_len; i++) {
    ((ID *)bases[i]->object->data)->tag |= LIB_TAG_DOIT;
  }
  Scene *scene = CTX_data_scene(C);
  Object **ob_p = object_array;
  for (uint i = 0; i < object_array_len; i++, ob_p = POINTER_OFFSET(ob_p, object_array_stride)) {
    Object *obedit = *ob_p;
    ED_object_editmode_enter_ex(bmain, scene, obedit, EM_NO_CONTEXT);
    ((ID *)obedit->data)->tag &= ~LIB_TAG_DOIT;
  }
  for (uint i = 0; i < bases_len; i++) {
    ID *id = bases[i]->object->data;
    if (id->tag & LIB_TAG_DOIT) {
      ED_object_editmode_exit_ex(bmain, scene, bases[i]->object, EM_FREEDATA);
      /* Ideally we would know the selection state it was before entering edit-mode,
       * for now follow the convention of having them unselected when exiting the mode. */
      ED_object_base_select(bases[i], BA_DESELECT);
    }
  }
  MEM_freeN(bases);
}

/** \} */
