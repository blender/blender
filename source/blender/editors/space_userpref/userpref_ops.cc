/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spuserpref
 */

#include <cstring>
#include <fmt/format.h>

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_listbase.h"
#ifdef WIN32
#  include "BLI_winstuff.h"
#endif
#include "BLI_fileops.h"
#include "BLI_path_util.h"

#include "BKE_callbacks.h"
#include "BKE_context.hh"
#include "BKE_main.hh"
#include "BKE_preferences.h"

#include "BKE_report.h"

#include "BLT_translation.h"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.h"
#include "RNA_types.hh"

#include "UI_interface.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_userpref.hh"

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
/** \name Add Extension Repository Operator
 * \{ */

enum class bUserExtensionRepoAddType {
  Remote = 0,
  Local = 1,
};

static int preferences_extension_repo_add_exec(bContext *C, wmOperator *op)
{
  const bUserExtensionRepoAddType repo_type = bUserExtensionRepoAddType(
      RNA_enum_get(op->ptr, "type"));

  Main *bmain = CTX_data_main(C);
  BKE_callback_exec_null(bmain, BKE_CB_EVT_EXTENSION_REPOS_UPDATE_PRE);

  char name[sizeof(bUserExtensionRepo::name)];
  char custom_directory[FILE_MAX] = "";

  const bool use_custom_directory = RNA_boolean_get(op->ptr, "use_custom_directory");
  RNA_string_get(op->ptr, "name", name);

  if (use_custom_directory) {
    RNA_string_get(op->ptr, "custom_directory", custom_directory);
    BLI_path_slash_rstrip(custom_directory);
  }

  const char *module = custom_directory[0] ? BLI_path_basename(custom_directory) : name;
  bUserExtensionRepo *new_repo = BKE_preferences_extension_repo_add(
      &U, name, module, custom_directory);

  if (use_custom_directory) {
    new_repo->flag |= USER_EXTENSION_REPO_FLAG_USE_CUSTOM_DIRECTORY;
  }

  if (repo_type == bUserExtensionRepoAddType::Remote) {
    RNA_string_get(op->ptr, "remote_path", new_repo->remote_path);
    new_repo->flag |= USER_EXTENSION_REPO_FLAG_USE_REMOTE_PATH;
  }

  /* Activate new repository in the UI for further setup. */
  U.active_extension_repo = BLI_findindex(&U.extension_repos, new_repo);
  U.runtime.is_dirty = true;

  BKE_callback_exec_null(bmain, BKE_CB_EVT_EXTENSION_REPOS_UPDATE_POST);

  /* There's no dedicated notifier for the Preferences. */
  WM_event_add_notifier(C, NC_WINDOW, nullptr);

  return OPERATOR_FINISHED;
}

static int preferences_extension_repo_add_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const bUserExtensionRepoAddType repo_type = bUserExtensionRepoAddType(
      RNA_enum_get(op->ptr, "type"));
  PropertyRNA *prop_name = RNA_struct_find_property(op->ptr, "name");
  if (!RNA_property_is_set(op->ptr, prop_name)) {
    const char *name_default = nullptr;
    if (repo_type == bUserExtensionRepoAddType::Remote) {
      name_default = "Remote Repository";
    }
    else {
      name_default = "User Repository";
    }
    RNA_property_string_set(op->ptr, prop_name, name_default);
  }

  return WM_operator_props_popup_confirm(C, op, event);
}

static bool preferences_extension_repo_add_poll_property(const bContext * /*C*/,
                                                         wmOperator *op,
                                                         const PropertyRNA *prop)
{
  PointerRNA *ptr = op->ptr;

  const char *prop_id = RNA_property_identifier(prop);
  const bUserExtensionRepoAddType repo_type = bUserExtensionRepoAddType(RNA_enum_get(ptr, "type"));

  /* Only show remote_path for remote repositories. */
  if (STREQ(prop_id, "remote_path")) {
    if (repo_type != bUserExtensionRepoAddType::Remote) {
      return false;
    }
  }

  if (STREQ(prop_id, "custom_directory")) {
    if (!RNA_boolean_get(ptr, "use_custom_directory")) {
      return false;
    }
  }

  /* Else, show it! */
  return true;
}

static void PREFERENCES_OT_extension_repo_add(wmOperatorType *ot)
{
  ot->name = "Add Extension Repository";
  ot->idname = "PREFERENCES_OT_extension_repo_add";
  ot->description = "Add a directory to be used as a local extension repository";

  ot->invoke = preferences_extension_repo_add_invoke;
  ot->exec = preferences_extension_repo_add_exec;
  ot->poll_property = preferences_extension_repo_add_poll_property;

  ot->flag = OPTYPE_INTERNAL | OPTYPE_REGISTER;

  static const EnumPropertyItem repo_type_items[] = {
      {int(bUserExtensionRepoAddType::Remote),
       "REMOTE",
       ICON_WORLD,
       "Add Remote Repository",
       "Add a repository referencing an remote repository "
       "with support for listing and updating extensions"},
      {int(bUserExtensionRepoAddType::Local),
       "LOCAL",
       ICON_DISK_DRIVE,
       "Add Local Repository",
       "Add a repository managed manually without referencing an external repository"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* After creating a new repository some settings can't be easily changed
   * (especially the custom directory). To avoid showing a partially initialized repository,
   * set these values upon creation instead of having the user create the repository and change
   * them afterwards.
   *
   * An alternative solution could be implemented by creating an "uninitialized" repository,
   * setting up all it's properties then running an "initialize" operator however this seems
   * unnecessarily confusing as in most cases a user can do this in one step by naming and
   * setting the repositories URL (optionally the custom-directory). */

  /* Copy the RNA values are copied into the operator to avoid repetition. */
  StructRNA *type_ref = &RNA_UserExtensionRepo;

  { /* Name. */
    const char *prop_id = "name";
    PropertyRNA *prop_ref = RNA_struct_type_find_property(type_ref, prop_id);
    PropertyRNA *prop = RNA_def_string(ot->srna,
                                       prop_id,
                                       nullptr,
                                       sizeof(bUserExtensionRepo::name),
                                       RNA_property_ui_name_raw(prop_ref),
                                       RNA_property_ui_description_raw(prop_ref));
    RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  }

  { /* Remote Path. */
    const char *prop_id = "remote_path";
    PropertyRNA *prop_ref = RNA_struct_type_find_property(type_ref, prop_id);
    PropertyRNA *prop = RNA_def_string(ot->srna,
                                       prop_id,
                                       nullptr,
                                       sizeof(bUserExtensionRepo::remote_path),
                                       RNA_property_ui_name_raw(prop_ref),
                                       RNA_property_ui_description_raw(prop_ref));
    RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  }

  { /* Use Custom Directory. */
    const char *prop_id = "use_custom_directory";
    PropertyRNA *prop_ref = RNA_struct_type_find_property(type_ref, prop_id);
    PropertyRNA *prop = RNA_def_boolean(ot->srna,
                                        prop_id,
                                        false,
                                        RNA_property_ui_name_raw(prop_ref),
                                        RNA_property_ui_description_raw(prop_ref));
    RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  }

  { /* Custom Directory. */
    const char *prop_id = "custom_directory";
    PropertyRNA *prop_ref = RNA_struct_type_find_property(type_ref, prop_id);
    PropertyRNA *prop = RNA_def_string_dir_path(ot->srna,
                                                prop_id,
                                                nullptr,
                                                sizeof(bUserExtensionRepo::remote_path),
                                                RNA_property_ui_name_raw(prop_ref),
                                                RNA_property_ui_description_raw(prop_ref));
    RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  }

  ot->prop = RNA_def_enum(
      ot->srna, "type", repo_type_items, 0, "Type", "The kind of repository to add");
  RNA_def_property_flag(ot->prop, PROP_SKIP_SAVE | PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remove Extension Repository Operator
 * \{ */

enum class bUserExtensionRepoRemoveType {
  RepoOnly = 0,
  RepoWithDirectory = 1,
};

static bool preferences_extension_repo_remove_poll(bContext *C)
{
  if (BLI_listbase_is_empty(&U.extension_repos)) {
    CTX_wm_operator_poll_msg_set(C, "There is no extension repository to remove");
    return false;
  }
  return true;
}

static int preferences_extension_repo_remove_invoke(bContext *C,
                                                    wmOperator *op,
                                                    const wmEvent * /*event*/)
{
  const int index = RNA_int_get(op->ptr, "index");
  bUserExtensionRepoRemoveType repo_type = bUserExtensionRepoRemoveType(
      RNA_enum_get(op->ptr, "type"));
  bUserExtensionRepo *repo = static_cast<bUserExtensionRepo *>(
      BLI_findlink(&U.extension_repos, index));

  if (!repo) {
    return OPERATOR_CANCELLED;
  }
  std::string message;
  if (repo_type == bUserExtensionRepoRemoveType::RepoWithDirectory) {
    char dirpath[FILE_MAX];
    BKE_preferences_extension_repo_dirpath_get(repo, dirpath, sizeof(dirpath));

    if (dirpath[0]) {
      message = fmt::format(IFACE_("Remove all files in \"{}\"."), dirpath);
    }
    else {
      message = IFACE_("Remove, local files not found.");
      repo_type = bUserExtensionRepoRemoveType::RepoOnly;
    }
  }
  else {
    message = IFACE_("Remove, keeping local files.");
  }

  const char *confirm_text = (repo_type == bUserExtensionRepoRemoveType::RepoWithDirectory) ?
                                 IFACE_("Remove Repository & Files") :
                                 IFACE_("Remove Repository");

  return WM_operator_confirm_ex(
      C, op, nullptr, message.c_str(), confirm_text, ALERT_ICON_WARNING, true);
}

static int preferences_extension_repo_remove_exec(bContext *C, wmOperator *op)
{
  const int index = RNA_int_get(op->ptr, "index");
  const bUserExtensionRepoRemoveType repo_type = bUserExtensionRepoRemoveType(
      RNA_enum_get(op->ptr, "type"));
  bUserExtensionRepo *repo = static_cast<bUserExtensionRepo *>(
      BLI_findlink(&U.extension_repos, index));
  if (!repo) {
    return OPERATOR_CANCELLED;
  }

  Main *bmain = CTX_data_main(C);
  BKE_callback_exec_null(bmain, BKE_CB_EVT_EXTENSION_REPOS_UPDATE_PRE);

  if (repo_type == bUserExtensionRepoRemoveType::RepoWithDirectory) {
    char dirpath[FILE_MAX];
    BKE_preferences_extension_repo_dirpath_get(repo, dirpath, sizeof(dirpath));
    if (dirpath[0] && BLI_is_dir(dirpath)) {
      if (BLI_delete(dirpath, true, true) != 0) {
        BKE_reportf(op->reports,
                    RPT_ERROR,
                    "Error removing directory: %s",
                    errno ? strerror(errno) : "unknown");
      }
    }
  }

  BKE_preferences_extension_repo_remove(&U, repo);
  const int count_remaining = BLI_listbase_count(&U.extension_repos);
  /* Update active repo index to be in range. */
  CLAMP(U.active_extension_repo, 0, count_remaining - 1);
  U.runtime.is_dirty = true;

  BKE_callback_exec_null(bmain, BKE_CB_EVT_EXTENSION_REPOS_UPDATE_POST);

  /* There's no dedicated notifier for the Preferences. */
  WM_event_add_notifier(C, NC_WINDOW, nullptr);

  return OPERATOR_FINISHED;
}

static void PREFERENCES_OT_extension_repo_remove(wmOperatorType *ot)
{
  ot->name = "Remove Extension Repository";
  ot->idname = "PREFERENCES_OT_extension_repo_remove";
  ot->description = "Remove an extension repository";

  ot->invoke = preferences_extension_repo_remove_invoke;
  ot->exec = preferences_extension_repo_remove_exec;
  ot->poll = preferences_extension_repo_remove_poll;

  ot->flag = OPTYPE_INTERNAL;

  static const EnumPropertyItem repo_type_items[] = {
      {int(bUserExtensionRepoRemoveType::RepoOnly), "REPO_ONLY", 0, "Remove Repository"},
      {int(bUserExtensionRepoRemoveType::RepoWithDirectory),
       "REPO_AND_DIRECTORY",
       0,
       "Remove Repository & Files",
       "Delete all associated local files when removing"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_int(ot->srna, "index", 0, 0, INT_MAX, "Index", "", 0, 1000);

  ot->prop = RNA_def_enum(
      ot->srna, "type", repo_type_items, 0, "Type", "Method for removing the repository");
  RNA_def_property_flag(ot->prop, PROP_SKIP_SAVE | PROP_HIDDEN);
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

  WM_operatortype_append(PREFERENCES_OT_extension_repo_add);
  WM_operatortype_append(PREFERENCES_OT_extension_repo_remove);

  WM_operatortype_append(PREFERENCES_OT_associate_blend);
  WM_operatortype_append(PREFERENCES_OT_unassociate_blend);
}
