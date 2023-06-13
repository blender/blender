/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edproject
 */

#include <climits>

#include "BKE_asset_library_custom.h"
#include "BKE_blender_project.h"
#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "BLI_path_util.h"

#include "BLT_translation.h"

#include "DNA_space_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_project.h"
#include "ED_screen.h"

using namespace blender;

static bool has_active_project_poll(bContext *C)
{
  const BlenderProject *active_project = CTX_wm_project();
  CTX_wm_operator_poll_msg_set(C, TIP_("No active project loaded"));
  return active_project != NULL;
}

/* -------------------------------------------------------------------- */
/** \name New project operator
 * \{ */

static bool new_project_poll(bContext *C)
{
  if (!U.experimental.use_blender_projects) {
    CTX_wm_operator_poll_msg_set(C, "Experimental project support is not enabled");
    return false;
  }
  return true;
}

static int new_project_exec(bContext *C, wmOperator *op)
{
  const Main *bmain = CTX_data_main(C);

  if (!RNA_struct_property_is_set(op->ptr, "directory")) {
    BKE_report(op->reports, RPT_ERROR, "No path defined for creating a new project in");
    return OPERATOR_CANCELLED;
  }
  char project_root_dir[FILE_MAXDIR];
  RNA_string_get(op->ptr, "directory", project_root_dir);

  if (!ED_project_new(bmain, project_root_dir, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  PropertyRNA *prop_open_settings = RNA_struct_find_property(op->ptr, "open_settings_after");
  if (RNA_property_is_set(op->ptr, prop_open_settings) &&
      RNA_property_boolean_get(op->ptr, prop_open_settings))
  {
    ED_project_settings_window_show(C, op->reports);
  }

  WM_main_add_notifier(NC_PROJECT, NULL);
  /* Update the window title. */
  WM_main_add_notifier(NC_WM | ND_DATACHANGED, NULL);

  return OPERATOR_FINISHED;
}

static int new_project_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  const Main *bmain = CTX_data_main(C);
  const char *blendfile_path = BKE_main_blendfile_path(bmain);
  if (blendfile_path[0]) {
    /* Open at the .blend file location if any. */
    RNA_string_set(op->ptr, "directory", blendfile_path);
  }

  WM_event_add_fileselect(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static void PROJECT_OT_new(wmOperatorType *ot)
{
  ot->name = "New Project";
  ot->idname = "PROJECT_OT_new";
  ot->description = "Choose a directory to use as the root of a project";

  ot->invoke = new_project_invoke;
  ot->exec = new_project_exec;
  /* omit window poll so this can work in background mode */
  ot->poll = new_project_poll;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER,
                                 FILE_BLENDER,
                                 FILE_OPENFILE,
                                 WM_FILESEL_DIRECTORY,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);

  PropertyRNA *prop;
  prop = RNA_def_boolean(ot->srna,
                         "open_settings_after",
                         false,
                         "",
                         "Open the project settings window after successfully creating a project");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Write Project Settings Operator
 * \{ */

static int save_settings_exec(bContext * /*C*/, wmOperator * /*op*/)
{
  BlenderProject *active_project = CTX_wm_project();

  if (!BKE_project_settings_save(active_project)) {
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

static void PROJECT_OT_save_settings(wmOperatorType *ot)
{
  ot->name = "Save Project Settings";
  ot->idname = "PROJECT_OT_save_settings";
  ot->description = "Make the current changes to the project settings permanent";

  ot->invoke = WM_operator_confirm;
  ot->poll = has_active_project_poll;
  ot->exec = save_settings_exec;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Delete project setup operator
 * \{ */

static int delete_project_setup_exec(bContext *C, wmOperator *op)
{
  if (!BKE_project_delete_settings_directory(CTX_wm_project())) {
    BKE_report(op->reports,
               RPT_ERROR,
               "Failed to delete project settings. Is the project directory read-only?");
    return OPERATOR_CANCELLED;
  }
  BKE_project_active_unset();

  WM_main_add_notifier(NC_PROJECT, NULL);
  /* Update the window title. */
  WM_event_add_notifier_ex(CTX_wm_manager(C), CTX_wm_window(C), NC_WM | ND_DATACHANGED, NULL);

  return OPERATOR_FINISHED;
}

static void PROJECT_OT_delete_setup(wmOperatorType *ot)
{
  ot->name = "Delete Project Setup";
  ot->idname = "PROJECT_OT_delete_setup";
  ot->description =
      "Remove the configuration of the current project with all settings, but keep project files "
      "(such as .blend files) untouched";

  ot->invoke = WM_operator_confirm;
  ot->exec = delete_project_setup_exec;
  /* omit window poll so this can work in background mode */
  ot->poll = has_active_project_poll;
}

/** \} */
/* -------------------------------------------------------------------- */
/** \name Add Custom Asset Library
 * \{ */

static int custom_asset_library_add_exec(bContext * /*C*/, wmOperator *op)
{
  BlenderProject *project = CTX_wm_project();

  char path[FILE_MAXDIR];
  char dirname[FILE_MAXFILE];

  RNA_string_get(op->ptr, "directory", path);

  BLI_path_slash_rstrip(path);
  /* Always keep project paths relative for now. Adds the "//" prefix which usually denotes a path
   * that's relative to the current .blend, for now use it for project relative paths as well. */
  BLI_path_rel(path, BKE_project_root_path_get(project));
  BLI_path_split_file_part(path, dirname, sizeof(dirname));

  ListBase *asset_libraries = BKE_project_custom_asset_libraries_get(project);
  /* NULL is a valid directory path here. A library without path will be created then. */
  BKE_asset_library_custom_add(asset_libraries, dirname, path);
  BKE_project_tag_has_unsaved_changes(project);

  WM_main_add_notifier(NC_ASSET | ND_ASSET_LIBRARY, NULL);
  WM_main_add_notifier(NC_PROJECT, NULL);

  return OPERATOR_FINISHED;
}

static int custom_asset_library_add_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  if (!RNA_struct_property_is_set(op->ptr, "directory")) {
    WM_event_add_fileselect(C, op);
    return OPERATOR_RUNNING_MODAL;
  }

  return custom_asset_library_add_exec(C, op);
}

/* Similar to #PREFERENCES_OT_asset_library_add. */
static void PROJECT_OT_custom_asset_library_add(wmOperatorType *ot)
{
  ot->name = "Add Asset Library";
  ot->idname = "PROJECT_OT_custom_asset_library_add";
  ot->description = "Register a directory to be used by the Asset Browser as source of assets";

  ot->exec = custom_asset_library_add_exec;
  ot->invoke = custom_asset_library_add_invoke;
  ot->poll = has_active_project_poll;

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
/** \name Remove Custom Asset Library
 * \{ */

static int custom_asset_library_remove_exec(bContext * /*C*/, wmOperator *op)
{
  const int index = RNA_int_get(op->ptr, "index");

  BlenderProject *project = CTX_wm_project();
  ListBase *asset_libraries = BKE_project_custom_asset_libraries_get(project);
  CustomAssetLibraryDefinition *library = BKE_asset_library_custom_find_from_index(asset_libraries,
                                                                                   index);
  BKE_asset_library_custom_remove(asset_libraries, library);
  BKE_project_tag_has_unsaved_changes(project);

  WM_main_add_notifier(NC_ASSET | ND_ASSET_LIBRARY, NULL);
  WM_main_add_notifier(NC_PROJECT, NULL);

  return OPERATOR_FINISHED;
}

/* Similar to #PREFERENCES_OT_asset_library_remove. */
static void PROJECT_OT_custom_asset_library_remove(wmOperatorType *ot)
{
  ot->name = "Remove Asset Library";
  ot->idname = "PROJECT_OT_custom_asset_library_remove";
  ot->description =
      "Unregister a path to a .blend file, so the Asset Browser will not attempt to show it "
      "anymore";

  ot->exec = custom_asset_library_remove_exec;
  ot->poll = has_active_project_poll;

  ot->flag = OPTYPE_INTERNAL;

  RNA_def_int(ot->srna, "index", 0, 0, INT_MAX, "Index", "", 0, 1000);
}

/** \} */

void ED_operatortypes_project()
{
  WM_operatortype_append(PROJECT_OT_new);
  WM_operatortype_append(PROJECT_OT_save_settings);
  WM_operatortype_append(PROJECT_OT_delete_setup);
  WM_operatortype_append(PROJECT_OT_custom_asset_library_add);
  WM_operatortype_append(PROJECT_OT_custom_asset_library_remove);
}
