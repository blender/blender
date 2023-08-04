/* SPDX-FileCopyrightText: 2009 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spuserpref
 */

#include <cstring>

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_listbase.h"
#ifdef WIN32
#  include "BLI_winstuff.h"
#endif
#include "BLI_path_util.h"

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_preferences.h"

#include "BKE_report.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_types.h"

#include "UI_interface.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_userpref.h"

#include "MEM_guardedalloc.h"

/* -------------------------------------------------------------------- */
/** \name Reset Default Theme Operator
 * \{ */

static int preferences_reset_default_theme_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  UI_theme_init_default();
  UI_style_init_default();
  WM_reinit_gizmomap_all(bmain);
  WM_event_add_notifier(C, NC_WINDOW, nullptr);
  U.runtime.is_dirty = true;
  return OPERATOR_FINISHED;
}

static void PREFERENCES_OT_reset_default_theme(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reset to Default Theme";
  ot->idname = "PREFERENCES_OT_reset_default_theme";
  ot->description = "Reset to the default theme colors";

  /* callbacks */
  ot->exec = preferences_reset_default_theme_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Auto-Execution Path Operator
 * \{ */

static int preferences_autoexec_add_exec(bContext * /*C*/, wmOperator * /*op*/)
{
  bPathCompare *path_cmp = static_cast<bPathCompare *>(
      MEM_callocN(sizeof(bPathCompare), "bPathCompare"));
  BLI_addtail(&U.autoexec_paths, path_cmp);
  U.runtime.is_dirty = true;
  return OPERATOR_FINISHED;
}

static void PREFERENCES_OT_autoexec_path_add(wmOperatorType *ot)
{
  ot->name = "Add Auto-Execution Path";
  ot->idname = "PREFERENCES_OT_autoexec_path_add";
  ot->description = "Add path to exclude from auto-execution";

  ot->exec = preferences_autoexec_add_exec;

  ot->flag = OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remove Auto-Execution Path Operator
 * \{ */

static int preferences_autoexec_remove_exec(bContext * /*C*/, wmOperator *op)
{
  const int index = RNA_int_get(op->ptr, "index");
  bPathCompare *path_cmp = static_cast<bPathCompare *>(BLI_findlink(&U.autoexec_paths, index));
  if (path_cmp) {
    BLI_freelinkN(&U.autoexec_paths, path_cmp);
    U.runtime.is_dirty = true;
  }
  return OPERATOR_FINISHED;
}

static void PREFERENCES_OT_autoexec_path_remove(wmOperatorType *ot)
{
  ot->name = "Remove Auto-Execution Path";
  ot->idname = "PREFERENCES_OT_autoexec_path_remove";
  ot->description = "Remove path to exclude from auto-execution";

  ot->exec = preferences_autoexec_remove_exec;

  ot->flag = OPTYPE_INTERNAL;

  RNA_def_int(ot->srna, "index", 0, 0, INT_MAX, "Index", "", 0, 1000);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Asset Library Operator
 * \{ */

static int preferences_asset_library_add_exec(bContext * /*C*/, wmOperator *op)
{
  char *path = RNA_string_get_alloc(op->ptr, "directory", nullptr, 0, nullptr);
  char dirname[FILE_MAXFILE];

  BLI_path_slash_rstrip(path);
  BLI_path_split_file_part(path, dirname, sizeof(dirname));

  /* nullptr is a valid directory path here. A library without path will be created then. */
  const bUserAssetLibrary *new_library = BKE_preferences_asset_library_add(&U, dirname, path);
  /* Activate new library in the UI for further setup. */
  U.active_asset_library = BLI_findindex(&U.asset_libraries, new_library);
  U.runtime.is_dirty = true;

  /* There's no dedicated notifier for the Preferences. */
  WM_main_add_notifier(NC_WINDOW, nullptr);

  MEM_freeN(path);
  return OPERATOR_FINISHED;
}

static int preferences_asset_library_add_invoke(bContext *C,
                                                wmOperator *op,
                                                const wmEvent * /*event*/)
{
  if (!RNA_struct_property_is_set(op->ptr, "directory")) {
    WM_event_add_fileselect(C, op);
    return OPERATOR_RUNNING_MODAL;
  }

  return preferences_asset_library_add_exec(C, op);
}

static void PREFERENCES_OT_asset_library_add(wmOperatorType *ot)
{
  ot->name = "Add Asset Library";
  ot->idname = "PREFERENCES_OT_asset_library_add";
  ot->description = "Add a directory to be used by the Asset Browser as source of assets";

  ot->exec = preferences_asset_library_add_exec;
  ot->invoke = preferences_asset_library_add_invoke;

  ot->flag = OPTYPE_INTERNAL;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER,
                                 FILE_SPECIAL,
                                 FILE_OPENFILE,
                                 WM_FILESEL_DIRECTORY,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remove Asset Library Operator
 * \{ */

static bool preferences_asset_library_remove_poll(bContext *C)
{
  if (BLI_listbase_is_empty(&U.asset_libraries)) {
    CTX_wm_operator_poll_msg_set(C, "There is no asset library to remove");
    return false;
  }
  return true;
}

static int preferences_asset_library_remove_exec(bContext * /*C*/, wmOperator *op)
{
  const int index = RNA_int_get(op->ptr, "index");
  bUserAssetLibrary *library = static_cast<bUserAssetLibrary *>(
      BLI_findlink(&U.asset_libraries, index));
  if (!library) {
    return OPERATOR_CANCELLED;
  }

  BKE_preferences_asset_library_remove(&U, library);
  const int count_remaining = BLI_listbase_count(&U.asset_libraries);
  /* Update active library index to be in range. */
  CLAMP(U.active_asset_library, 0, count_remaining - 1);
  U.runtime.is_dirty = true;

  /* Trigger refresh for the Asset Browser. */
  WM_main_add_notifier(NC_SPACE | ND_SPACE_ASSET_PARAMS, nullptr);

  return OPERATOR_FINISHED;
}

static void PREFERENCES_OT_asset_library_remove(wmOperatorType *ot)
{
  ot->name = "Remove Asset Library";
  ot->idname = "PREFERENCES_OT_asset_library_remove";
  ot->description =
      "Remove a path to a .blend file, so the Asset Browser will not attempt to show it anymore";

  ot->exec = preferences_asset_library_remove_exec;
  ot->poll = preferences_asset_library_remove_poll;

  ot->flag = OPTYPE_INTERNAL;

  RNA_def_int(ot->srna, "index", 0, 0, INT_MAX, "Index", "", 0, 1000);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Associate File Type Operator (Windows only)
 * \{ */

static bool associate_blend_poll(bContext *C)
{
#ifdef WIN32
  if (BLI_windows_is_store_install()) {
    CTX_wm_operator_poll_msg_set(C, "Not available for Microsoft Store installations");
    return false;
  }
  return true;
#else
  CTX_wm_operator_poll_msg_set(C, "Windows-only operator");
  return false;
#endif
}

static int associate_blend_exec(bContext * /*C*/, wmOperator *op)
{
#ifdef WIN32
  if (BLI_windows_is_store_install()) {
    BKE_report(
        op->reports, RPT_ERROR, "Registration not possible from Microsoft Store installations");
    return OPERATOR_CANCELLED;
  }

  const bool all_users = (U.uiflag & USER_REGISTER_ALL_USERS);

  WM_cursor_wait(true);

  if (all_users && BLI_windows_execute_self("--register-allusers", true, true, true)) {
    BKE_report(op->reports, RPT_INFO, "File association registered");
    WM_cursor_wait(false);
    return OPERATOR_FINISHED;
  }
  else if (!all_users && BLI_windows_register_blend_extension(false)) {
    BKE_report(op->reports, RPT_INFO, "File association registered");
    WM_cursor_wait(false);
    return OPERATOR_FINISHED;
  }
  else {
    BKE_report(op->reports, RPT_ERROR, "Unable to register file association");
    WM_cursor_wait(false);
    MessageBox(0, "Unable to register file association", "Blender", MB_OK | MB_ICONERROR);
    return OPERATOR_CANCELLED;
  }
#else
  UNUSED_VARS(op);
  BLI_assert_unreachable();
  return OPERATOR_CANCELLED;
#endif
}

static void PREFERENCES_OT_associate_blend(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Register File Association";
  ot->description = "Use this installation for .blend files and to display thumbnails";
  ot->idname = "PREFERENCES_OT_associate_blend";

  /* api callbacks */
  ot->exec = associate_blend_exec;
  ot->poll = associate_blend_poll;
}

static int unassociate_blend_exec(bContext * /*C*/, wmOperator *op)
{
#ifdef WIN32
  if (BLI_windows_is_store_install()) {
    BKE_report(
        op->reports, RPT_ERROR, "Unregistration not possible from Microsoft Store installations");
    return OPERATOR_CANCELLED;
  }

  const bool all_users = (U.uiflag & USER_REGISTER_ALL_USERS);

  WM_cursor_wait(true);

  if (all_users && BLI_windows_execute_self("--unregister-allusers", true, true, true)) {
    BKE_report(op->reports, RPT_INFO, "File association unregistered");
    WM_cursor_wait(false);
    return OPERATOR_FINISHED;
  }
  else if (!all_users && BLI_windows_unregister_blend_extension(false)) {
    BKE_report(op->reports, RPT_INFO, "File association unregistered");
    WM_cursor_wait(false);
    return OPERATOR_FINISHED;
  }
  else {
    BKE_report(op->reports, RPT_ERROR, "Unable to unregister file association");
    WM_cursor_wait(false);
    MessageBox(0, "Unable to unregister file association", "Blender", MB_OK | MB_ICONERROR);
    return OPERATOR_CANCELLED;
  }
#else
  UNUSED_VARS(op);
  BLI_assert_unreachable();
  return OPERATOR_CANCELLED;
#endif
}

static void PREFERENCES_OT_unassociate_blend(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove File Association";
  ot->description = "Remove this installation's associations with .blend files";
  ot->idname = "PREFERENCES_OT_unassociate_blend";

  /* api callbacks */
  ot->exec = unassociate_blend_exec;
  ot->poll = associate_blend_poll;
}

/** \} */

void ED_operatortypes_userpref()
{
  WM_operatortype_append(PREFERENCES_OT_reset_default_theme);

  WM_operatortype_append(PREFERENCES_OT_autoexec_path_add);
  WM_operatortype_append(PREFERENCES_OT_autoexec_path_remove);

  WM_operatortype_append(PREFERENCES_OT_asset_library_add);
  WM_operatortype_append(PREFERENCES_OT_asset_library_remove);

  WM_operatortype_append(PREFERENCES_OT_associate_blend);
  WM_operatortype_append(PREFERENCES_OT_unassociate_blend);
}
