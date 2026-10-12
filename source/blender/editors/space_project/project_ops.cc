/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spuserpref
 */

#include "BKE_blender_project.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"

#include "BLI_fileops.hh"
#include "BLI_path_utils.hh"

#include "BLT_translation.hh"

#include "RNA_access.hh"

#include "RNA_define.hh"
#include "WM_api.hh"
#include "WM_types.hh"

#include "project_intern.hh"
#include "wm_files.hh"

namespace blender {

/** Save changes to the active project before replacing or closing it, if enabled. */
static void project_auto_save(Main *bmain)
{
  if (!(U.pref_flag & USER_PREF_FLAG_PROJECT_SAVE)) {
    return;
  }
  const bool is_dirty = BKE_blender_project_read_callback(
      bmain, [](const bke::BlenderProject *project) { return project && project->is_dirty; });
  if (is_dirty) {
    BKE_blender_project_config_file_write(bmain);
  }
}

static wmOperatorStatus project_new_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);

  char root_path[FILE_MAX];
  RNA_string_get(op->ptr, "directory", root_path);
  BLI_path_abs(root_path, BKE_main_blendfile_path(bmain));
  if (!BLI_path_is_abs_from_cwd(root_path)) {
    BKE_report(op->reports, RPT_ERROR, "Invalid project root folder");
    return OPERATOR_CANCELLED;
  }
  BLI_path_normalize(root_path);
  BLI_path_slash_rstrip(root_path);

  /* Propose the folder name as project name. */
  const char *name = BLI_path_basename(root_path);
  if (name[0] == '\0') {
    name = DATA_("Untitled Project");
  }

  /* Nesting inside another project is allowed, but not replacing it. */
  if (BLI_is_dir((std::string(root_path) + SEP_STR + ".blender_project").c_str())) {
    BKE_report(op->reports, RPT_ERROR, "Folder already contains a project");
    return OPERATOR_CANCELLED;
  }

  if (!BLI_dir_create_recursive(root_path)) {
    BKE_reportf(op->reports, RPT_ERROR, "Failed to create folder \"%s\"", root_path);
    return OPERATOR_CANCELLED;
  }

  project_auto_save(bmain);

  BKE_blender_project_init(name, root_path);
  if (!BKE_blender_project_config_file_write(bmain)) {
    BKE_blender_project_clear();
    BKE_reportf(op->reports, RPT_ERROR, "Failed to write project config in \"%s\"", root_path);
    return OPERATOR_CANCELLED;
  }
  WM_main_add_notifier(NC_WINDOW, nullptr);

  if (!G.background) {
    BKE_blender_project_recent_update(bmain);
  }

  /* Show the project settings when used from the user interface. */
  if (op->flag & OP_IS_INVOKE) {
    WM_operator_name_call(
        C, "SCREEN_OT_project_settings_show", wm::OpCallContext::ExecDefault, nullptr, nullptr);
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus project_new_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  /* Start in the folder of the current blend file. */
  const char *blendfile_path = BKE_main_blendfile_path(CTX_data_main(C));
  if (RNA_string_length(op->ptr, "directory") == 0 && blendfile_path[0] != '\0') {
    char directory[FILE_MAX];
    BLI_path_split_dir_part(blendfile_path, directory, sizeof(directory));
    RNA_string_set(op->ptr, "directory", directory);
  }

  WM_event_add_fileselect(C, op);
  return OPERATOR_RUNNING_MODAL;
}

void PROJECT_OT_new(wmOperatorType *ot)
{
  ot->name = "New Project";
  ot->idname = "PROJECT_OT_new";
  ot->description = "Create a new project";

  ot->invoke = project_new_invoke;
  ot->exec = project_new_exec;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER,
                                 FILE_BLENDER,
                                 FILE_OPENFILE,
                                 WM_FILESEL_DIRECTORY,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
}

static wmOperatorStatus project_open_exec(bContext *C, wmOperator *op)
{
  char dirpath[FILE_MAX];
  RNA_string_get(op->ptr, "directory", dirpath);
  std::string project_root_path = BKE_is_path_inside_blender_project(dirpath);

  if (project_root_path == "") {
    BKE_reportf(op->reports, RPT_ERROR, "Folder '%s' does not belong to a project", dirpath);
    return OPERATOR_CANCELLED;
  }

  /* Initialize project before blend file, so it is taken into account. */
  project_auto_save(CTX_data_main(C));
  BKE_blender_project_clear();
  const bool success = BKE_blender_project_init_from_root_path(CTX_data_main(C),
                                                               project_root_path);

  WM_operator_name_call(
      C, "WM_OT_read_homefile", wm::OpCallContext::ExecDefault, nullptr, nullptr);

  if (!success) {
    BKE_reportf(
        op->reports, RPT_ERROR, "Failed to load project from path: %s", project_root_path.c_str());
    return OPERATOR_CANCELLED;
  }

  if (!G.background) {
    BKE_blender_project_recent_update(CTX_data_main(C));
  }

  if (op->flag & OP_IS_INVOKE) {
    WM_init_splash(C);
  }

  return OPERATOR_FINISHED;
}

static void project_open_after_dialog_callback(bContext *C, void *user_data)
{
  WM_operator_name_call_with_properties(C,
                                        "PROJECT_OT_open",
                                        wm::OpCallContext::InvokeDefault,
                                        static_cast<IDProperty *>(user_data),
                                        nullptr);
}

static wmOperatorStatus project_open_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  if (!RNA_boolean_get(op->ptr, "skip_save_check")) {
    RNA_boolean_set(op->ptr, "skip_save_check", true);
    if (wm_operator_close_file_dialog_if_needed(C, op, project_open_after_dialog_callback)) {
      return OPERATOR_INTERFACE;
    }
  }

  /* Open directly when the folder is given, for example for recent projects. */
  if (RNA_struct_property_is_set(op->ptr, "directory")) {
    return project_open_exec(C, op);
  }

  WM_event_add_fileselect(C, op);
  return OPERATOR_RUNNING_MODAL;
}

void PROJECT_OT_open(wmOperatorType *ot)
{
  ot->name = "Open Project...";
  ot->idname = "PROJECT_OT_open";
  ot->description = "Open a project, starting a new file in it";

  ot->invoke = project_open_invoke;
  ot->exec = project_open_exec;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER,
                                 FILE_BLENDER,
                                 FILE_OPENFILE,
                                 WM_FILESEL_DIRECTORY,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);

  PropertyRNA *prop = RNA_def_boolean(ot->srna,
                                      "skip_save_check",
                                      false,
                                      "Skip check to prevent unsaved changes from being lost",
                                      "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

static wmOperatorStatus project_close_exec(bContext *C, wmOperator *op)
{
  project_auto_save(CTX_data_main(C));
  BKE_blender_project_clear();
  WM_operator_name_call(
      C, "WM_OT_read_homefile", wm::OpCallContext::ExecDefault, nullptr, nullptr);

  if (op->flag & OP_IS_INVOKE) {
    WM_init_splash(C);
  }

  return OPERATOR_FINISHED;
}

static void project_close_after_dialog_callback(bContext *C, void *user_data)
{
  WM_operator_name_call_with_properties(C,
                                        "PROJECT_OT_close",
                                        wm::OpCallContext::InvokeDefault,
                                        static_cast<IDProperty *>(user_data),
                                        nullptr);
}

static wmOperatorStatus project_close_invoke(bContext *C,
                                             wmOperator *op,
                                             const wmEvent * /*event*/)
{
  if (!RNA_boolean_get(op->ptr, "skip_save_check")) {
    RNA_boolean_set(op->ptr, "skip_save_check", true);
    if (wm_operator_close_file_dialog_if_needed(C, op, project_close_after_dialog_callback)) {
      return OPERATOR_INTERFACE;
    }
  }
  return project_close_exec(C, op);
}

static bool project_close_poll(bContext *C)
{
  Main *bmain = CTX_data_main(C);
  return BKE_blender_project_get(bmain) != nullptr;
}

void PROJECT_OT_close(wmOperatorType *ot)
{
  ot->name = "Close Project";
  ot->idname = "PROJECT_OT_close";
  ot->description = "Close the active project, starting a new file";

  ot->invoke = project_close_invoke;
  ot->exec = project_close_exec;
  ot->poll = project_close_poll;

  PropertyRNA *prop = RNA_def_boolean(ot->srna,
                                      "skip_save_check",
                                      false,
                                      "Skip check to prevent unsaved changes from being lost",
                                      "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  RNA_def_property_flag(prop, PROP_HIDDEN);
}
}  // namespace blender
