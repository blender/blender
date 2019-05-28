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
 */

/** \file
 * \ingroup wm
 *
 * Operator Registry.
 */

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

#include "DNA_ID.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BLT_translation.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"

#include "BKE_context.h"
#include "BKE_idprop.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "wm.h"
#include "wm_event_system.h"

#define UNDOCUMENTED_OPERATOR_TIP N_("(undocumented operator)")

static void wm_operatortype_free_macro(wmOperatorType *ot);

/* -------------------------------------------------------------------- */
/** \name Operator Type Registry
 * \{ */

static GHash *global_ops_hash = NULL;
/** Counter for operator-properties that should not be tagged with #OP_PROP_TAG_ADVANCED. */
static int ot_prop_basic_count = -1;

wmOperatorType *WM_operatortype_find(const char *idname, bool quiet)
{
  if (idname[0]) {
    wmOperatorType *ot;

    /* needed to support python style names without the _OT_ syntax */
    char idname_bl[OP_MAX_TYPENAME];
    WM_operator_bl_idname(idname_bl, idname);

    ot = BLI_ghash_lookup(global_ops_hash, idname_bl);
    if (ot) {
      return ot;
    }

    if (!quiet) {
      CLOG_INFO(
          WM_LOG_OPERATORS, 0, "search for unknown operator '%s', '%s'\n", idname_bl, idname);
    }
  }
  else {
    if (!quiet) {
      CLOG_INFO(WM_LOG_OPERATORS, 0, "search for empty operator");
    }
  }

  return NULL;
}

/* caller must free */
void WM_operatortype_iter(GHashIterator *ghi)
{
  BLI_ghashIterator_init(ghi, global_ops_hash);
}

/** \name Operator Type Append
 * \{ */

static wmOperatorType *wm_operatortype_append__begin(void)
{
  wmOperatorType *ot = MEM_callocN(sizeof(wmOperatorType), "operatortype");

  BLI_assert(ot_prop_basic_count == -1);

  ot->srna = RNA_def_struct_ptr(&BLENDER_RNA, "", &RNA_OperatorProperties);
  RNA_def_struct_property_tags(ot->srna, rna_enum_operator_property_tags);
  /* Set the default i18n context now, so that opfunc can redefine it if needed! */
  RNA_def_struct_translation_context(ot->srna, BLT_I18NCONTEXT_OPERATOR_DEFAULT);
  ot->translation_context = BLT_I18NCONTEXT_OPERATOR_DEFAULT;

  return ot;
}
static void wm_operatortype_append__end(wmOperatorType *ot)
{
  if (ot->name == NULL) {
    CLOG_ERROR(WM_LOG_OPERATORS, "Operator '%s' has no name property", ot->idname);
  }
  BLI_assert((ot->description == NULL) || (ot->description[0]));

  /* Allow calling _begin without _end in operatortype creation. */
  WM_operatortype_props_advanced_end(ot);

  /* XXX All ops should have a description but for now allow them not to. */
  RNA_def_struct_ui_text(
      ot->srna, ot->name, ot->description ? ot->description : UNDOCUMENTED_OPERATOR_TIP);
  RNA_def_struct_identifier(&BLENDER_RNA, ot->srna, ot->idname);

  BLI_ghash_insert(global_ops_hash, (void *)ot->idname, ot);
}

/* all ops in 1 list (for time being... needs evaluation later) */
void WM_operatortype_append(void (*opfunc)(wmOperatorType *))
{
  wmOperatorType *ot = wm_operatortype_append__begin();
  opfunc(ot);
  wm_operatortype_append__end(ot);
}

void WM_operatortype_append_ptr(void (*opfunc)(wmOperatorType *, void *), void *userdata)
{
  wmOperatorType *ot = wm_operatortype_append__begin();
  opfunc(ot, userdata);
  wm_operatortype_append__end(ot);
}

/** \} */

/* called on initialize WM_exit() */
void WM_operatortype_remove_ptr(wmOperatorType *ot)
{
  BLI_assert(ot == WM_operatortype_find(ot->idname, false));

  RNA_struct_free(&BLENDER_RNA, ot->srna);

  if (ot->last_properties) {
    IDP_FreeProperty(ot->last_properties);
  }

  if (ot->macro.first) {
    wm_operatortype_free_macro(ot);
  }

  BLI_ghash_remove(global_ops_hash, ot->idname, NULL, NULL);

  WM_keyconfig_update_operatortype();

  MEM_freeN(ot);
}

bool WM_operatortype_remove(const char *idname)
{
  wmOperatorType *ot = WM_operatortype_find(idname, 0);

  if (ot == NULL) {
    return false;
  }

  WM_operatortype_remove_ptr(ot);

  return true;
}

/* called on initialize WM_init() */
void wm_operatortype_init(void)
{
  /* reserve size is set based on blender default setup */
  global_ops_hash = BLI_ghash_str_new_ex("wm_operatortype_init gh", 2048);
}

static void operatortype_ghash_free_cb(wmOperatorType *ot)
{
  if (ot->last_properties) {
    IDP_FreeProperty(ot->last_properties);
  }

  if (ot->macro.first) {
    wm_operatortype_free_macro(ot);
  }

  if (ot->ext.srna) {
    /* python operator, allocs own string */
    MEM_freeN((void *)ot->idname);
  }

  MEM_freeN(ot);
}

void wm_operatortype_free(void)
{
  BLI_ghash_free(global_ops_hash, NULL, (GHashValFreeFP)operatortype_ghash_free_cb);
  global_ops_hash = NULL;
}

/**
 * Tag all operator-properties of \a ot defined after calling this, until
 * the next #WM_operatortype_props_advanced_end call (if available), with
 * #OP_PROP_TAG_ADVANCED. Previously defined ones properties not touched.
 *
 * Calling this multiple times without a call to #WM_operatortype_props_advanced_end,
 * all calls after the first one are ignored. Meaning all propereties defined after the
 * first call are tagged as advanced.
 *
 * This doesn't do the actual tagging, #WM_operatortype_props_advanced_end does which is
 * called for all operators during registration (see #wm_operatortype_append__end).
 */
void WM_operatortype_props_advanced_begin(wmOperatorType *ot)
{
  if (ot_prop_basic_count ==
      -1) { /* Don't do anything if _begin was called before, but not _end  */
    ot_prop_basic_count = RNA_struct_count_properties(ot->srna);
  }
}

/**
 * Tags all operator-properties of \a ot defined since the first
 * #WM_operatortype_props_advanced_begin call,
 * or the last #WM_operatortype_props_advanced_end call, with #OP_PROP_TAG_ADVANCED.
 *
 * \note This is called for all operators during registration (see #wm_operatortype_append__end).
 * So it does not need to be explicitly called in operator-type definition.
 */
void WM_operatortype_props_advanced_end(wmOperatorType *ot)
{
  PointerRNA struct_ptr;
  int counter = 0;

  if (ot_prop_basic_count == -1) {
    /* WM_operatortype_props_advanced_begin was not called. Don't do anything. */
    return;
  }

  RNA_pointer_create(NULL, ot->srna, NULL, &struct_ptr);

  RNA_STRUCT_BEGIN (&struct_ptr, prop) {
    counter++;
    if (counter > ot_prop_basic_count) {
      WM_operatortype_prop_tag(prop, OP_PROP_TAG_ADVANCED);
    }
  }
  RNA_STRUCT_END;

  ot_prop_basic_count = -1;
}

/**
 * Remove memory of all previously executed tools.
 */
void WM_operatortype_last_properties_clear_all(void)
{
  GHashIterator iter;

  for (WM_operatortype_iter(&iter); (!BLI_ghashIterator_done(&iter));
       (BLI_ghashIterator_step(&iter))) {
    wmOperatorType *ot = BLI_ghashIterator_getValue(&iter);

    if (ot->last_properties) {
      IDP_FreeProperty(ot->last_properties);
      ot->last_properties = NULL;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator Macro Type
 * \{ */

typedef struct {
  int retval;
} MacroData;

static void wm_macro_start(wmOperator *op)
{
  if (op->customdata == NULL) {
    op->customdata = MEM_callocN(sizeof(MacroData), "MacroData");
  }
}

static int wm_macro_end(wmOperator *op, int retval)
{
  if (retval & OPERATOR_CANCELLED) {
    MacroData *md = op->customdata;

    if (md->retval & OPERATOR_FINISHED) {
      retval |= OPERATOR_FINISHED;
      retval &= ~OPERATOR_CANCELLED;
    }
  }

  /* if modal is ending, free custom data */
  if (retval & (OPERATOR_FINISHED | OPERATOR_CANCELLED)) {
    if (op->customdata) {
      MEM_freeN(op->customdata);
      op->customdata = NULL;
    }
  }

  return retval;
}

/* macro exec only runs exec calls */
static int wm_macro_exec(bContext *C, wmOperator *op)
{
  wmOperator *opm;
  int retval = OPERATOR_FINISHED;

  wm_macro_start(op);

  for (opm = op->macro.first; opm; opm = opm->next) {

    if (opm->type->exec) {
      retval = opm->type->exec(C, opm);
      OPERATOR_RETVAL_CHECK(retval);

      if (retval & OPERATOR_FINISHED) {
        MacroData *md = op->customdata;
        md->retval = OPERATOR_FINISHED; /* keep in mind that at least one operator finished */
      }
      else {
        break; /* operator didn't finish, end macro */
      }
    }
    else {
      CLOG_WARN(WM_LOG_OPERATORS, "'%s' cant exec macro", opm->type->idname);
    }
  }

  return wm_macro_end(op, retval);
}

static int wm_macro_invoke_internal(bContext *C,
                                    wmOperator *op,
                                    const wmEvent *event,
                                    wmOperator *opm)
{
  int retval = OPERATOR_FINISHED;

  /* start from operator received as argument */
  for (; opm; opm = opm->next) {
    if (opm->type->invoke) {
      retval = opm->type->invoke(C, opm, event);
    }
    else if (opm->type->exec) {
      retval = opm->type->exec(C, opm);
    }

    OPERATOR_RETVAL_CHECK(retval);

    BLI_movelisttolist(&op->reports->list, &opm->reports->list);

    if (retval & OPERATOR_FINISHED) {
      MacroData *md = op->customdata;
      md->retval = OPERATOR_FINISHED; /* keep in mind that at least one operator finished */
    }
    else {
      break; /* operator didn't finish, end macro */
    }
  }

  return wm_macro_end(op, retval);
}

static int wm_macro_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  wm_macro_start(op);
  return wm_macro_invoke_internal(C, op, event, op->macro.first);
}

static int wm_macro_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmOperator *opm = op->opm;
  int retval = OPERATOR_FINISHED;

  if (opm == NULL) {
    CLOG_ERROR(WM_LOG_OPERATORS, "macro error, calling NULL modal()");
  }
  else {
    retval = opm->type->modal(C, opm, event);
    OPERATOR_RETVAL_CHECK(retval);

    /* if we're halfway through using a tool and cancel it, clear the options [#37149] */
    if (retval & OPERATOR_CANCELLED) {
      WM_operator_properties_clear(opm->ptr);
    }

    /* if this one is done but it's not the last operator in the macro */
    if ((retval & OPERATOR_FINISHED) && opm->next) {
      MacroData *md = op->customdata;

      md->retval = OPERATOR_FINISHED; /* keep in mind that at least one operator finished */

      retval = wm_macro_invoke_internal(C, op, event, opm->next);

      /* if new operator is modal and also added its own handler */
      if (retval & OPERATOR_RUNNING_MODAL && op->opm != opm) {
        wmWindow *win = CTX_wm_window(C);
        wmEventHandler_Op *handler;

        handler = BLI_findptr(&win->modalhandlers, op, offsetof(wmEventHandler_Op, op));
        if (handler) {
          BLI_remlink(&win->modalhandlers, handler);
          wm_event_free_handler(&handler->head);
        }

        /* if operator is blocking, grab cursor
         * This may end up grabbing twice, but we don't care.
         * */
        if (op->opm->type->flag & OPTYPE_BLOCKING) {
          int bounds[4] = {-1, -1, -1, -1};
          int wrap = WM_CURSOR_WRAP_NONE;

          if ((op->opm->flag & OP_IS_MODAL_GRAB_CURSOR) ||
              (op->opm->type->flag & OPTYPE_GRAB_CURSOR_XY)) {
            wrap = WM_CURSOR_WRAP_XY;
          }
          else if (op->opm->type->flag & OPTYPE_GRAB_CURSOR_X) {
            wrap = WM_CURSOR_WRAP_X;
          }
          else if (op->opm->type->flag & OPTYPE_GRAB_CURSOR_Y) {
            wrap = WM_CURSOR_WRAP_Y;
          }

          if (wrap) {
            ARegion *ar = CTX_wm_region(C);
            if (ar) {
              bounds[0] = ar->winrct.xmin;
              bounds[1] = ar->winrct.ymax;
              bounds[2] = ar->winrct.xmax;
              bounds[3] = ar->winrct.ymin;
            }
          }

          WM_cursor_grab_enable(win, wrap, false, bounds);
        }
      }
    }
  }

  return wm_macro_end(op, retval);
}

static void wm_macro_cancel(bContext *C, wmOperator *op)
{
  /* call cancel on the current modal operator, if any */
  if (op->opm && op->opm->type->cancel) {
    op->opm->type->cancel(C, op->opm);
  }

  wm_macro_end(op, OPERATOR_CANCELLED);
}

/* Names have to be static for now */
wmOperatorType *WM_operatortype_append_macro(const char *idname,
                                             const char *name,
                                             const char *description,
                                             int flag)
{
  wmOperatorType *ot;
  const char *i18n_context;

  if (WM_operatortype_find(idname, true)) {
    CLOG_ERROR(WM_LOG_OPERATORS, "operator %s exists, cannot create macro", idname);
    return NULL;
  }

  ot = MEM_callocN(sizeof(wmOperatorType), "operatortype");
  ot->srna = RNA_def_struct_ptr(&BLENDER_RNA, "", &RNA_OperatorProperties);

  ot->idname = idname;
  ot->name = name;
  ot->description = description;
  ot->flag = OPTYPE_MACRO | flag;

  ot->exec = wm_macro_exec;
  ot->invoke = wm_macro_invoke;
  ot->modal = wm_macro_modal;
  ot->cancel = wm_macro_cancel;
  ot->poll = NULL;

  if (!ot->description) {
    /* XXX All ops should have a description but for now allow them not to. */
    ot->description = UNDOCUMENTED_OPERATOR_TIP;
  }

  RNA_def_struct_ui_text(ot->srna, ot->name, ot->description);
  RNA_def_struct_identifier(&BLENDER_RNA, ot->srna, ot->idname);
  /* Use i18n context from ext.srna if possible (py operators). */
  i18n_context = ot->ext.srna ? RNA_struct_translation_context(ot->ext.srna) :
                                BLT_I18NCONTEXT_OPERATOR_DEFAULT;
  RNA_def_struct_translation_context(ot->srna, i18n_context);
  ot->translation_context = i18n_context;

  BLI_ghash_insert(global_ops_hash, (void *)ot->idname, ot);

  return ot;
}

void WM_operatortype_append_macro_ptr(void (*opfunc)(wmOperatorType *, void *), void *userdata)
{
  wmOperatorType *ot;

  ot = MEM_callocN(sizeof(wmOperatorType), "operatortype");
  ot->srna = RNA_def_struct_ptr(&BLENDER_RNA, "", &RNA_OperatorProperties);

  ot->flag = OPTYPE_MACRO;
  ot->exec = wm_macro_exec;
  ot->invoke = wm_macro_invoke;
  ot->modal = wm_macro_modal;
  ot->cancel = wm_macro_cancel;
  ot->poll = NULL;

  if (!ot->description) {
    ot->description = UNDOCUMENTED_OPERATOR_TIP;
  }

  /* Set the default i18n context now, so that opfunc can redefine it if needed! */
  RNA_def_struct_translation_context(ot->srna, BLT_I18NCONTEXT_OPERATOR_DEFAULT);
  ot->translation_context = BLT_I18NCONTEXT_OPERATOR_DEFAULT;
  opfunc(ot, userdata);

  RNA_def_struct_ui_text(ot->srna, ot->name, ot->description);
  RNA_def_struct_identifier(&BLENDER_RNA, ot->srna, ot->idname);

  BLI_ghash_insert(global_ops_hash, (void *)ot->idname, ot);
}

wmOperatorTypeMacro *WM_operatortype_macro_define(wmOperatorType *ot, const char *idname)
{
  wmOperatorTypeMacro *otmacro = MEM_callocN(sizeof(wmOperatorTypeMacro), "wmOperatorTypeMacro");

  BLI_strncpy(otmacro->idname, idname, OP_MAX_TYPENAME);

  /* do this on first use, since operatordefinitions might have been not done yet */
  WM_operator_properties_alloc(&(otmacro->ptr), &(otmacro->properties), idname);
  WM_operator_properties_sanitize(otmacro->ptr, 1);

  BLI_addtail(&ot->macro, otmacro);

  {
    /* operator should always be found but in the event its not. don't segfault */
    wmOperatorType *otsub = WM_operatortype_find(idname, 0);
    if (otsub) {
      RNA_def_pointer_runtime(
          ot->srna, otsub->idname, otsub->srna, otsub->name, otsub->description);
    }
  }

  return otmacro;
}

static void wm_operatortype_free_macro(wmOperatorType *ot)
{
  wmOperatorTypeMacro *otmacro;

  for (otmacro = ot->macro.first; otmacro; otmacro = otmacro->next) {
    if (otmacro->ptr) {
      WM_operator_properties_free(otmacro->ptr);
      MEM_freeN(otmacro->ptr);
    }
  }
  BLI_freelistN(&ot->macro);
}

/** \} */
