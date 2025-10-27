/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Operator Registry.
 */

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "BLT_translation.hh"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_vector_set.hh"

#include "BKE_context.hh"
#include "BKE_idprop.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#ifdef WITH_PYTHON
#  include "BPY_extern.hh"
#endif

#include "WM_api.hh"
#include "WM_keymap.hh"
#include "WM_types.hh"

#include "wm.hh"
#include "wm_event_system.hh"

#define UNDOCUMENTED_OPERATOR_TIP N_("(undocumented operator)")

static void wm_operatortype_free_macro(wmOperatorType *ot);

/* -------------------------------------------------------------------- */
/** \name Operator Type Registry
 * \{ */

using blender::StringRef;

static auto &get_operators_map()
{
  struct OperatorNameGetter {
    StringRef operator()(const wmOperatorType *value) const
    {
      return StringRef(value->idname);
    }
  };
  static auto map = []() {
    blender::CustomIDVectorSet<wmOperatorType *, OperatorNameGetter> map;
    /* Reserve size is set based on blender default setup. */
    map.reserve(2048);
    return map;
  }();
  return map;
}

blender::Span<wmOperatorType *> WM_operatortypes_registered_get()
{
  return get_operators_map();
}

/** Counter for operator-properties that should not be tagged with #OP_PROP_TAG_ADVANCED. */
static int ot_prop_basic_count = -1;

wmOperatorType *WM_operatortype_find(const char *idname, bool quiet)
{
  if (idname[0]) {
    /* Needed to support python style names without the `_OT_` syntax. */
    char idname_bl[OP_MAX_TYPENAME];
    WM_operator_bl_idname(idname_bl, idname);

    if (wmOperatorType *const *ot = get_operators_map().lookup_key_ptr_as(StringRef(idname_bl))) {
      return *ot;
    }

    if (!quiet) {
      CLOG_INFO(WM_LOG_OPERATORS, "Search for unknown operator '%s', '%s'", idname_bl, idname);
    }
  }
  else {
    if (!quiet) {
      CLOG_INFO(WM_LOG_OPERATORS, "Search for empty operator");
    }
  }

  return nullptr;
}

/* -------------------------------------------------------------------- */
/** \name Operator Type Append
 * \{ */

static wmOperatorType *wm_operatortype_append__begin()
{
  wmOperatorType *ot = MEM_new<wmOperatorType>(__func__);

  BLI_assert(ot_prop_basic_count == -1);

  ot->srna = RNA_def_struct_ptr(&BLENDER_RNA, "", &RNA_OperatorProperties);
  RNA_def_struct_property_tags(ot->srna, rna_enum_operator_property_tag_items);
  /* Set the default i18n context now, so that opfunc can redefine it if needed! */
  RNA_def_struct_translation_context(ot->srna, BLT_I18NCONTEXT_OPERATOR_DEFAULT);
  ot->translation_context = BLT_I18NCONTEXT_OPERATOR_DEFAULT;
  ot->cursor_pending = WM_CURSOR_PICK_AREA;

  return ot;
}
static void wm_operatortype_append__end(wmOperatorType *ot)
{
  if (ot->name == nullptr) {
    CLOG_ERROR(WM_LOG_OPERATORS, "Operator '%s' has no name property", ot->idname);
  }
  BLI_assert((ot->description == nullptr) || (ot->description[0]));

  /* Allow calling _begin without _end in operatortype creation. */
  WM_operatortype_props_advanced_end(ot);

  /* XXX All ops should have a description but for now allow them not to. */
  RNA_def_struct_ui_text(
      ot->srna, ot->name, ot->description ? ot->description : UNDOCUMENTED_OPERATOR_TIP);
  RNA_def_struct_identifier(&BLENDER_RNA, ot->srna, ot->idname);

  BLI_assert(WM_operator_bl_idname_is_valid(ot->idname));
  get_operators_map().add_new(ot);

  /* Needed so any operators registered after startup will have their shortcuts set,
   * in "register" scripts for example, see: #143838.
   *
   * This only has run-time implications when run after startup,
   * it's a no-op when run beforehand, see: #WM_keyconfig_update_on_startup. */
  WM_keyconfig_update_operatortype_tag();
}

/* All ops in 1 list (for time being... needs evaluation later). */

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

void WM_operatortype_remove_ptr(wmOperatorType *ot)
{
  BLI_assert(ot == WM_operatortype_find(ot->idname, false));

#ifdef WITH_PYTHON
  /* The 'unexposed' type (inherited from #RNA_OperatorProperties) created for this operator type's
   * properties may have had a python type representation created. This needs to be dereferenced
   * manually here, as other #bpy_class_free (which is part of the unregistering code for runtime
   * operators) will not be able to handle it. */
  BPY_free_srna_pytype(ot->srna);
#endif

  RNA_struct_free(&BLENDER_RNA, ot->srna);

  if (ot->last_properties) {
    IDP_FreeProperty(ot->last_properties);
  }

  if (ot->macro.first) {
    wm_operatortype_free_macro(ot);
  }

  get_operators_map().remove(ot);

  WM_keyconfig_update_operatortype_tag();

  MEM_delete(ot);
}

bool WM_operatortype_remove(const char *idname)
{
  wmOperatorType *ot = WM_operatortype_find(idname, false);

  if (ot == nullptr) {
    return false;
  }

  WM_operatortype_remove_ptr(ot);

  return true;
}

static void operatortype_ghash_free_cb(wmOperatorType *ot)
{
  if (ot->last_properties) {
    IDP_FreeProperty(ot->last_properties);
  }

  if (ot->macro.first) {
    wm_operatortype_free_macro(ot);
  }

  if (ot->rna_ext.srna) {
    /* A Python operator, allocates its own string. */
    MEM_freeN(ot->idname);
  }

  MEM_delete(ot);
}

void wm_operatortype_free()
{
  for (wmOperatorType *ot : get_operators_map()) {
    operatortype_ghash_free_cb(ot);
  }
  get_operators_map().clear();
}

void WM_operatortype_props_advanced_begin(wmOperatorType *ot)
{
  if (ot_prop_basic_count == -1) {
    /* Don't do anything if _begin was called before, but not _end. */
    ot_prop_basic_count = RNA_struct_count_properties(ot->srna);
  }
}

void WM_operatortype_props_advanced_end(wmOperatorType *ot)
{
  PointerRNA struct_ptr;
  int counter = 0;

  if (ot_prop_basic_count == -1) {
    /* WM_operatortype_props_advanced_begin was not called. Don't do anything. */
    return;
  }

  WM_operator_properties_create_ptr(&struct_ptr, ot);

  RNA_STRUCT_BEGIN (&struct_ptr, prop) {
    counter++;
    if (counter > ot_prop_basic_count) {
      WM_operatortype_prop_tag(prop, OP_PROP_TAG_ADVANCED);
    }
  }
  RNA_STRUCT_END;

  ot_prop_basic_count = -1;
}

void WM_operatortype_last_properties_clear_all()
{
  for (wmOperatorType *ot : get_operators_map()) {
    if (ot->last_properties) {
      IDP_FreeProperty(ot->last_properties);
      ot->last_properties = nullptr;
    }
  }
}

void WM_operatortype_idname_visit_for_search(
    const bContext * /*C*/,
    PointerRNA * /*ptr*/,
    PropertyRNA * /*prop*/,
    const char * /*edit_text*/,
    blender::FunctionRef<void(StringPropertySearchVisitParams)> visit_fn)
{
  for (wmOperatorType *ot : get_operators_map()) {
    char idname_py[OP_MAX_TYPENAME];
    WM_operator_py_idname(idname_py, ot->idname);

    StringPropertySearchVisitParams visit_params{};
    visit_params.text = idname_py;
    visit_params.info = ot->name;
    visit_fn(visit_params);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator Macro Type
 * \{ */

struct MacroData {
  wmOperatorStatus retval;
};

static void wm_macro_start(wmOperator *op)
{
  if (op->customdata == nullptr) {
    op->customdata = MEM_callocN<MacroData>("MacroData");
  }
}

static wmOperatorStatus wm_macro_end(wmOperator *op, wmOperatorStatus retval)
{
  MacroData *md = static_cast<MacroData *>(op->customdata);

  if (retval & (OPERATOR_CANCELLED | OPERATOR_INTERFACE)) {
    if (md && (md->retval & OPERATOR_FINISHED)) {
      retval |= OPERATOR_FINISHED;
      retval &= ~(OPERATOR_CANCELLED | OPERATOR_INTERFACE);
    }
  }

  /* If modal is ending, free custom data. */
  if (retval & (OPERATOR_FINISHED | OPERATOR_CANCELLED)) {
    if (md) {
      MEM_freeN(md);
      op->customdata = nullptr;
    }
  }

  return retval;
}

/* Macro exec only runs exec calls. */
static wmOperatorStatus wm_macro_exec(bContext *C, wmOperator *op)
{
  wmOperatorStatus retval = OPERATOR_FINISHED;
  const int op_inherited_flag = op->flag & (OP_IS_REPEAT | OP_IS_REPEAT_LAST);

  wm_macro_start(op);

  LISTBASE_FOREACH (wmOperator *, opm, &op->macro) {
    if (opm->type->exec == nullptr) {
      CLOG_WARN(WM_LOG_OPERATORS, "'%s' can't exec macro", opm->type->idname);
      continue;
    }

    opm->flag |= op_inherited_flag;
    retval = opm->type->exec(C, opm);
    opm->flag &= ~op_inherited_flag;

    OPERATOR_RETVAL_CHECK(retval);

    if (retval & OPERATOR_FINISHED) {
      MacroData *md = static_cast<MacroData *>(op->customdata);
      md->retval = OPERATOR_FINISHED; /* Keep in mind that at least one operator finished. */
    }
    else {
      break; /* Operator didn't finish, end macro. */
    }
  }

  return wm_macro_end(op, retval);
}

static wmOperatorStatus wm_macro_invoke_internal(bContext *C,
                                                 wmOperator *op,
                                                 const wmEvent *event,
                                                 wmOperator *opm)
{
  wmOperatorStatus retval = OPERATOR_FINISHED;
  const int op_inherited_flag = op->flag & (OP_IS_REPEAT | OP_IS_REPEAT_LAST);

  /* Start from operator received as argument. */
  for (; opm; opm = opm->next) {

    opm->flag |= op_inherited_flag;
    if (opm->type->invoke) {
      retval = opm->type->invoke(C, opm, event);
    }
    else if (opm->type->exec) {
      retval = opm->type->exec(C, opm);
    }
    opm->flag &= ~op_inherited_flag;

    OPERATOR_RETVAL_CHECK(retval);

    BLI_movelisttolist(&op->reports->list, &opm->reports->list);

    if (retval & OPERATOR_FINISHED) {
      MacroData *md = static_cast<MacroData *>(op->customdata);
      md->retval = OPERATOR_FINISHED; /* Keep in mind that at least one operator finished. */
    }
    else {
      break; /* Operator didn't finish, end macro. */
    }
  }

  return wm_macro_end(op, retval);
}

static wmOperatorStatus wm_macro_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  wm_macro_start(op);
  return wm_macro_invoke_internal(C, op, event, static_cast<wmOperator *>(op->macro.first));
}

static wmOperatorStatus wm_macro_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmOperator *opm = op->opm;
  wmOperatorStatus retval = OPERATOR_FINISHED;

  if (opm == nullptr) {
    CLOG_ERROR(WM_LOG_OPERATORS, "macro error, calling nullptr modal()");
  }
  else {
    retval = opm->type->modal(C, opm, event);
    OPERATOR_RETVAL_CHECK(retval);

    /* If we're halfway through using a tool and cancel it, clear the options, see: #37149. */
    if (retval & OPERATOR_CANCELLED) {
      WM_operator_properties_clear(opm->ptr);
    }

    /* If this one is done but it's not the last operator in the macro. */
    if ((retval & OPERATOR_FINISHED) && opm->next) {
      MacroData *md = static_cast<MacroData *>(op->customdata);

      md->retval = OPERATOR_FINISHED; /* Keep in mind that at least one operator finished. */

      retval = wm_macro_invoke_internal(C, op, event, opm->next);

      /* If new operator is modal and also added its own handler. */
      if (retval & OPERATOR_RUNNING_MODAL && op->opm != opm) {
        wmWindow *win = CTX_wm_window(C);
        wmEventHandler_Op *handler;

        handler = static_cast<wmEventHandler_Op *>(
            BLI_findptr(&win->modalhandlers, op, offsetof(wmEventHandler_Op, op)));
        if (handler) {
          BLI_remlink(&win->modalhandlers, handler);
          wm_event_free_handler(&handler->head);
        }

        /* If operator is blocking, grab cursor.
         * This may end up grabbing twice, but we don't care. */
        if (op->opm->type->flag & OPTYPE_BLOCKING) {
          int wrap = WM_CURSOR_WRAP_NONE;
          const rcti *wrap_region = nullptr;

          if ((op->opm->flag & OP_IS_MODAL_GRAB_CURSOR) ||
              (op->opm->type->flag & OPTYPE_GRAB_CURSOR_XY))
          {
            wrap = WM_CURSOR_WRAP_XY;
          }
          else if (op->opm->type->flag & OPTYPE_GRAB_CURSOR_X) {
            wrap = WM_CURSOR_WRAP_X;
          }
          else if (op->opm->type->flag & OPTYPE_GRAB_CURSOR_Y) {
            wrap = WM_CURSOR_WRAP_Y;
          }

          if (wrap) {
            ARegion *region = CTX_wm_region(C);
            if (region) {
              wrap_region = &region->winrct;
            }
          }

          WM_cursor_grab_enable(win, eWM_CursorWrapAxis(wrap), wrap_region, false);
        }
      }
    }
  }

  return wm_macro_end(op, retval);
}

static void wm_macro_cancel(bContext *C, wmOperator *op)
{
  /* Call cancel on the current modal operator, if any. */
  if (op->opm && op->opm->type->cancel) {
    op->opm->type->cancel(C, op->opm);
  }

  wm_macro_end(op, OPERATOR_CANCELLED);
}

wmOperatorType *WM_operatortype_append_macro(const char *idname,
                                             const char *name,
                                             const char *description,
                                             int flag)
{
  wmOperatorType *ot;
  const char *i18n_context;

  if (WM_operatortype_find(idname, true)) {
    CLOG_ERROR(WM_LOG_OPERATORS, "operator %s exists, cannot create macro", idname);
    return nullptr;
  }

  ot = MEM_new<wmOperatorType>(__func__);
  ot->srna = RNA_def_struct_ptr(&BLENDER_RNA, "", &RNA_OperatorProperties);

  ot->idname = idname;
  ot->name = name;
  ot->description = description;
  ot->flag = OPTYPE_MACRO | flag;

  ot->exec = wm_macro_exec;
  ot->invoke = wm_macro_invoke;
  ot->modal = wm_macro_modal;
  ot->cancel = wm_macro_cancel;
  ot->poll = nullptr;

  /* XXX All ops should have a description but for now allow them not to. */
  BLI_assert((ot->description == nullptr) || (ot->description[0]));

  RNA_def_struct_ui_text(
      ot->srna, ot->name, ot->description ? ot->description : UNDOCUMENTED_OPERATOR_TIP);
  RNA_def_struct_identifier(&BLENDER_RNA, ot->srna, ot->idname);
  /* Use i18n context from rna_ext.srna if possible (py operators). */
  i18n_context = ot->rna_ext.srna ? RNA_struct_translation_context(ot->rna_ext.srna) :
                                    BLT_I18NCONTEXT_OPERATOR_DEFAULT;
  RNA_def_struct_translation_context(ot->srna, i18n_context);
  ot->translation_context = i18n_context;

  BLI_assert(WM_operator_bl_idname_is_valid(ot->idname));
  get_operators_map().add_new(ot);

  return ot;
}

void WM_operatortype_append_macro_ptr(void (*opfunc)(wmOperatorType *ot, void *userdata),
                                      void *userdata)
{
  wmOperatorType *ot;

  ot = MEM_new<wmOperatorType>(__func__);
  ot->srna = RNA_def_struct_ptr(&BLENDER_RNA, "", &RNA_OperatorProperties);

  ot->flag = OPTYPE_MACRO;
  ot->exec = wm_macro_exec;
  ot->invoke = wm_macro_invoke;
  ot->modal = wm_macro_modal;
  ot->cancel = wm_macro_cancel;
  ot->poll = nullptr;

  /* XXX All ops should have a description but for now allow them not to. */
  BLI_assert((ot->description == nullptr) || (ot->description[0]));

  /* Set the default i18n context now, so that opfunc can redefine it if needed! */
  RNA_def_struct_translation_context(ot->srna, BLT_I18NCONTEXT_OPERATOR_DEFAULT);
  ot->translation_context = BLT_I18NCONTEXT_OPERATOR_DEFAULT;
  opfunc(ot, userdata);

  RNA_def_struct_ui_text(
      ot->srna, ot->name, ot->description ? ot->description : UNDOCUMENTED_OPERATOR_TIP);
  RNA_def_struct_identifier(&BLENDER_RNA, ot->srna, ot->idname);

  BLI_assert(WM_operator_bl_idname_is_valid(ot->idname));
  get_operators_map().add_new(ot);
}

wmOperatorTypeMacro *WM_operatortype_macro_define(wmOperatorType *ot, const char *idname)
{
  wmOperatorTypeMacro *otmacro = MEM_callocN<wmOperatorTypeMacro>("wmOperatorTypeMacro");

  STRNCPY(otmacro->idname, idname);

  /* Do this on first use, since operator definitions might have been not done yet. */
  WM_operator_properties_alloc(&(otmacro->ptr), &(otmacro->properties), idname);
  WM_operator_properties_sanitize(otmacro->ptr, true);

  BLI_addtail(&ot->macro, otmacro);

  /* Operator should always be found but in the event its not. don't segfault. */
  if (wmOperatorType *otsub = WM_operatortype_find(idname, false)) {
    RNA_def_pointer_runtime(ot->srna, otsub->idname, otsub->srna, otsub->name, otsub->description);
  }

  return otmacro;
}

static void wm_operatortype_free_macro(wmOperatorType *ot)
{
  LISTBASE_FOREACH (wmOperatorTypeMacro *, otmacro, &ot->macro) {
    if (otmacro->ptr) {
      WM_operator_properties_free(otmacro->ptr);
      MEM_delete(otmacro->ptr);
    }
  }
  BLI_freelistN(&ot->macro);
}

std::string WM_operatortype_name(wmOperatorType *ot, PointerRNA *properties)
{
  std::string name;
  if (ot->get_name && properties) {
    name = ot->get_name(ot, properties);
  }

  return name.empty() ? std::string(RNA_struct_ui_name(ot->srna)) : name;
}

std::string WM_operatortype_description(bContext *C, wmOperatorType *ot, PointerRNA *properties)
{
  if (ot->get_description && properties) {
    std::string description = ot->get_description(C, ot, properties);
    if (!description.empty()) {
      return description;
    }
  }

  const char *info = RNA_struct_ui_description(ot->srna);
  if (info && info[0]) {
    return info;
  }
  return "";
}

std::string WM_operatortype_description_or_name(bContext *C,
                                                wmOperatorType *ot,
                                                PointerRNA *properties)
{
  std::string text = WM_operatortype_description(C, ot, properties);
  if (text.empty()) {
    std::string text_orig = WM_operatortype_name(ot, properties);
    if (!text_orig.empty()) {
      return text_orig;
    }
  }
  return text;
}

bool WM_operator_depends_on_cursor(bContext &C, wmOperatorType &ot, PointerRNA *properties)
{
  if (ot.flag & OPTYPE_DEPENDS_ON_CURSOR) {
    return true;
  }
  if (ot.depends_on_cursor) {
    return ot.depends_on_cursor(C, ot, properties);
  }
  return false;
}

/** \} */
