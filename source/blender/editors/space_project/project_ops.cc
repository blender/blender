/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spuserpref
 */

#include <fmt/format.h>

#include "BKE_appdir.hh"
#include "BKE_blender_project.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"

#include "BLI_fileops.hh"
#include "BLI_listbase_iterator.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.hh"

#include "BLT_translation.hh"

#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "ED_userpref.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "UI_interface_c.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "project_intern.hh"
#include "wm_files.hh"

namespace blender {

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

static bool has_vaild_project_poll(bContext *C)
{
  Main *bmain = CTX_data_main(C);
  return BKE_blender_project_get(bmain) != nullptr;
}

static void project_mark_dirty(Main *bmain)
{
  BKE_blender_project_write_callback(bmain, [](bke::BlenderProject *project) {
    if (project) {
      project->is_dirty = true;
    };
  });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name New Project Operator
 * \{ */

static std::string new_project_name_get(PointerRNA *ptr)
{
  return RNA_struct_property_is_set(ptr, "name") ? StringRef(RNA_string_get(ptr, "name")).trim() :
                                                   DATA_("Untitled Project");
}

static std::string new_project_default_parent()
{
  char documents_path[FILE_MAXDIR];
  const char *base_path = BKE_appdir_folder_documents(documents_path) ? documents_path :
                                                                        BLI_dir_home();
  char path[FILE_MAX];
  BLI_path_join(path, sizeof(path), base_path ? base_path : "", "Blender", "Projects");
  return path;
}

/**
 * All parent directories of the current blend file, below the home folder and
 * file system root since you wouldn't want a project directly inside those.
 */
static Vector<std::string> new_project_suggested_directories(const Main *bmain)
{
  Vector<std::string> directories;
  const char *blendfile_path = BKE_main_blendfile_path(bmain);
  if (blendfile_path[0] == '\0') {
    return directories;
  }

  const char *home_path = BLI_dir_home();
  char directory[FILE_MAX];
  BLI_path_split_dir_part(blendfile_path, directory, sizeof(directory));

  while (!(home_path && BLI_path_contains(directory, home_path))) {
    char parent[FILE_MAX];
    STRNCPY(parent, directory);
    if (!BLI_path_parent_dir(parent) || !BLI_path_is_abs_from_cwd(parent)) {
      break;
    }
    BLI_path_slash_rstrip(directory);
    directories.append(directory);
    STRNCPY(directory, parent);
  }
  return directories;
}

/** Is `directory` the folder of the blend file or one of its parents. */
static bool new_project_use_existing_directory(const Main *bmain, PointerRNA *ptr)
{
  return RNA_string_length(ptr, "directory") == 0 &&
         !new_project_suggested_directories(bmain).is_empty();
}

/** Normalized absolute project root folder, or empty if not absolute. */
static std::string new_project_root_path(const Main *bmain, PointerRNA *ptr)
{
  char path[FILE_MAX];
  if (new_project_use_existing_directory(bmain, ptr)) {
    const Vector<std::string> directories = new_project_suggested_directories(bmain);
    const int index = RNA_enum_get(ptr, "existing_directory");
    if (!directories.index_range().contains(index)) {
      return "";
    }
    STRNCPY(path, directories[index].c_str());
  }
  else {
    RNA_string_get(ptr, "directory", path);
    BLI_path_abs(path, BKE_main_blendfile_path(bmain));
  }

  if (!BLI_path_is_abs_from_cwd(path)) {
    return "";
  }
  BLI_path_normalize(path);
  BLI_path_slash_rstrip(path);
  return path;
}

/** Root of a project containing the folder. */
static std::string new_project_find_root(const StringRefNull directory)
{
  char path[FILE_MAX];
  STRNCPY(path, directory.c_str());
  while (!BLI_is_dir(path)) {
    if (!BLI_path_parent_dir(path)) {
      return "";
    }
  }
  return BKE_is_path_inside_blender_project(path);
}

static void new_project_name_from_directory(PointerRNA *ptr, const StringRefNull directory)
{
  /* Propose the folder name as project name. */
  char path[FILE_MAX];
  STRNCPY(path, directory.c_str());
  BLI_path_slash_rstrip(path);
  const char *name = BLI_path_basename(path);
  if (name && name[0]) {
    RNA_string_set(ptr, "name", name);
  }
}

/** Update the directory from the project name, if the previous named matched. */
static void new_project_update_directory_from_name_set(PointerRNA *ptr)
{
  char directory[FILE_MAX];
  RNA_string_get(ptr, "directory", directory);
  BLI_path_slash_rstrip(directory);
  RNA_boolean_set(ptr,
                  "update_directory_from_name",
                  BLI_path_basename(directory) == new_project_name_get(ptr));
}

static void new_project_name_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  /* Rename the directory along with the project. */
  const std::string name = new_project_name_get(ptr);
  if (!RNA_boolean_get(ptr, "update_directory_from_name")) {
    return;
  }
  if (name.empty() || name.find_first_of("/\\") != StringRef::not_found) {
    /* Can't use as directory name. */
    return;
  }

  char directory[FILE_MAX];
  RNA_string_get(ptr, "directory", directory);
  BLI_path_slash_rstrip(directory);
  BLI_path_parent_dir(directory);
  BLI_path_append(directory, sizeof(directory), name.c_str());
  RNA_string_set(ptr, "directory", directory);
}

static void new_project_directory_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  new_project_update_directory_from_name_set(ptr);
}

static void new_project_existing_directory_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  const Vector<std::string> directories = new_project_suggested_directories(bmain);
  const int index = RNA_enum_get(ptr, "existing_directory");
  if (directories.index_range().contains(index)) {
    new_project_name_from_directory(ptr, directories[index]);
  }
}

static const EnumPropertyItem *new_project_existing_directory_itemf(bContext *C,
                                                                    PointerRNA * /*ptr*/,
                                                                    PropertyRNA * /*prop*/,
                                                                    bool *r_free)
{
  /* Item strings must remain valid after returning, keep them until they change. */
  static Vector<std::string> directories;
  Vector<std::string> new_directories = new_project_suggested_directories(C ? CTX_data_main(C) :
                                                                              G_MAIN);
  if (new_directories != directories) {
    directories = std::move(new_directories);
  }

  EnumPropertyItem *items = nullptr;
  int totitem = 0;
  for (const int i : directories.index_range()) {
    EnumPropertyItem item = {};
    item.value = i;
    item.identifier = directories[i].c_str();
    item.name = directories[i].c_str();
    RNA_enum_item_add(&items, &totitem, &item);
  }
  RNA_enum_item_end(&items, &totitem);
  *r_free = true;
  return items;
}

static bool project_new_poll(bContext *C)
{
  Main *bmain = CTX_data_main(C);
  if (BKE_blender_project_get(bmain) != nullptr) {
    CTX_wm_operator_poll_msg_set(C, IFACE_("Close the current project before creating a new one"));
    return false;
  }

  return true;
}

static wmOperatorStatus project_new_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);

  const std::string root_path = new_project_root_path(bmain, op->ptr);
  if (root_path.empty()) {
    BKE_report(op->reports, RPT_ERROR, "Invalid project root folder");
    return OPERATOR_CANCELLED;
  }

  if (!RNA_struct_property_is_set(op->ptr, "name")) {
    new_project_name_from_directory(op->ptr, root_path);
  }

  const std::string name = new_project_name_get(op->ptr);
  if (name.empty()) {
    BKE_report(op->reports, RPT_ERROR, "Missing project name");
    return OPERATOR_CANCELLED;
  }

  /* Don't allow nesting inside or replacing a project. Nested does not have
   * well defined semantices so is not supported for now. */
  if (!BKE_is_path_inside_blender_project(root_path.c_str()).empty()) {
    BKE_report(op->reports, RPT_ERROR, "Folder already part of a project");
    return OPERATOR_CANCELLED;
  }

  if (!BLI_dir_create_recursive(root_path.c_str())) {
    BKE_reportf(op->reports, RPT_ERROR, "Failed to create folder \"%s\"", root_path.c_str());
    return OPERATOR_CANCELLED;
  }

  BKE_blender_project_config_autosave(bmain);

  BKE_blender_project_init(name, root_path);
  if (!BKE_blender_project_config_file_write(bmain)) {
    BKE_blender_project_clear();
    BKE_reportf(
        op->reports, RPT_ERROR, "Failed to write project config in \"%s\"", root_path.c_str());
    return OPERATOR_CANCELLED;
  }
  /* Force full redraw of all windows, and refresh their title. */
  WM_main_add_notifier(NC_WINDOW, nullptr);
  WM_main_add_notifier(NC_WM | ND_PROJECT_CHANGED, nullptr);

  /* Show the project settings when used from the user interface. */
  if (op->flag & OP_IS_INVOKE) {
    WM_operator_name_call(
        C, "SCREEN_OT_project_settings_show", wm::OpCallContext::ExecDefault, nullptr, nullptr);
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus project_new_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  Main *bmain = CTX_data_main(C);

  if (RNA_string_length(op->ptr, "directory") == 0) {
    if (BKE_main_blendfile_path(bmain)[0] != '\0') {
      /* Saved file, use an existing parent directory. */
      Vector<std::string> suggested_directories = new_project_suggested_directories(bmain);
      if (suggested_directories.is_empty()) {
        BKE_report(op->reports,
                   RPT_ERROR,
                   "Can not create project for a blend file in the home or root folder");
        return OPERATOR_CANCELLED;
      }

      RNA_enum_set(op->ptr, "existing_directory", 0);
      new_project_name_from_directory(op->ptr, suggested_directories[0]);
    }
    else {
      /* Not saved, suggest a default project name and path. */
      const std::string projects_dir = new_project_default_parent();
      const std::string base_name = new_project_name_get(op->ptr);
      std::string name = base_name;
      char directory[FILE_MAX];
      BLI_path_join(directory, sizeof(directory), projects_dir.c_str(), name.c_str());
      /* Make the name unique. */
      for (int number = 2; BLI_exists(directory); number++) {
        name = fmt::format("{} {}", base_name, number);
        BLI_path_join(directory, sizeof(directory), projects_dir.c_str(), name.c_str());
      }

      RNA_string_set(op->ptr, "name", name.c_str());
      RNA_string_set(op->ptr, "directory", directory);
    }
  }

  new_project_update_directory_from_name_set(op->ptr);

  return WM_operator_props_dialog_popup(
      C,
      op,
      600,
      std::nullopt,
      IFACE_("Create Project"),
      false,
      RPT_("A Blender Project is a directory that provides a shared environment for related "
           ".blend files, textures, and other files, with project-specific settings such as "
           "Asset Libraries, custom variables, and other configuration options.\n"
           "\n"
           "This is a structured way to organize files and settings across complex pipelines "
           "and team projects."));
}

/** Information to help notice when the wrong folder is used by accident. */
static void new_project_draw_folder_info(ui::Layout &layout, const std::string &root_path)
{
  const std::string parent_root = new_project_find_root(root_path);
  if (!parent_root.empty()) {
    if (BLI_path_cmp_normalized(parent_root.c_str(), root_path.c_str()) == 0) {
      layout.label(IFACE_("Folder already contains a project."), ICON_STATUS_WARNING);
      return;
    }
    layout.label(IFACE_("Folder is inside another project."), ICON_STATUS_WARNING);
  }
}

static void project_new_ui(bContext *C, wmOperator *op)
{
  ui::Layout &layout = *op->layout;
  const Main *bmain = CTX_data_main(C);
  PointerRNA *ptr = op->ptr;

  /* Narrower label column than usual, to leave more space for long paths. */
  constexpr float label_factor = 0.25f;
  auto labeled_row = [&](const StringRef text) -> ui::Layout & {
    ui::Layout &split = layout.split(label_factor, false);
    ui::Layout &col = split.column(false);
    col.alignment_set(ui::LayoutAlign::Right);
    col.label(text, ICON_NONE);
    return split.row(true);
  };

  /* Name and path. */
  labeled_row(IFACE_("Name")).prop(ptr, "name", UI_ITEM_NONE, "", ICON_NONE);

  if (new_project_use_existing_directory(bmain, ptr)) {
    ui::Layout &row = labeled_row(IFACE_("Project Root Folder"));
    row.prop(ptr, "existing_directory", UI_ITEM_NONE, "", ICON_NONE);

    ui::Layout &button_row = row.row(true);
    button_row.enabled_set(false);
    button_row.button(
        "",
        ICON_FILEBROWSER,
        [](bContext &) {},
        TIP_("The current file needs to be inside of the project's path.\n"
             "To pick a different folder, move the current file there first, or create a new "
             "project from a new unsaved file."));
  }
  else {
    ui::template_filepath(&labeled_row(IFACE_("Project Root Folder")),
                          C,
                          ptr,
                          "directory",
                          nullptr,
                          nullptr,
                          "",
                          std::nullopt,
                          true);
  }

  /* Path info. */
  const std::string root_path = new_project_root_path(bmain, ptr);
  if (!root_path.empty()) {
    ui::Layout &split = layout.split(label_factor, false);
    split.column(false);
    new_project_draw_folder_info(split.column(false), root_path);
  }
}

void PROJECT_OT_new(wmOperatorType *ot)
{
  ot->name = "New Project";
  ot->idname = "PROJECT_OT_new";
  ot->description = "Create a new project";

  ot->invoke = project_new_invoke;
  ot->exec = project_new_exec;
  ot->poll = project_new_poll;
  ot->ui = project_new_ui;

  PropertyRNA *prop;
  prop = RNA_def_string(ot->srna, "name", "Untitled Project", 0, "Name", "Name of the project");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  RNA_def_property_update_runtime(prop, new_project_name_update);

  prop = RNA_def_string_dir_path(ot->srna,
                                 "directory",
                                 nullptr,
                                 FILE_MAX,
                                 "Project Root Folder",
                                 "Project folder, created if missing");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  RNA_def_property_update_runtime(prop, new_project_directory_update);

  prop = RNA_def_boolean(
      ot->srna, "update_directory_from_name", false, "Update Directory From Name", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  RNA_def_property_flag(prop, PROP_HIDDEN);

  /* Choice of folder when a blend file is open, used if `directory` is not set. */
  prop = RNA_def_enum(ot->srna,
                      "existing_directory",
                      rna_enum_dummy_NULL_items,
                      0,
                      "Project Root Folder",
                      "Folder of the current file or one of its parents, to turn into a project");
  RNA_def_enum_funcs(prop, new_project_existing_directory_itemf);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  RNA_def_property_update_runtime(prop, new_project_existing_directory_update);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Open Project Operator
 * \{ */

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
  BKE_blender_project_config_autosave(CTX_data_main(C));
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

  /* Force full redraw of all windows, and refresh their title. */
  WM_main_add_notifier(NC_WINDOW, nullptr);
  WM_main_add_notifier(NC_WM | ND_PROJECT_CHANGED, nullptr);

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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Close Project Operator
 * \{ */

static wmOperatorStatus project_close_exec(bContext *C, wmOperator *op)
{
  BKE_blender_project_config_autosave(CTX_data_main(C));
  BKE_blender_project_clear();
  WM_operator_name_call(
      C, "WM_OT_read_homefile", wm::OpCallContext::ExecDefault, nullptr, nullptr);

  /* Force full redraw of all windows, and refresh their title. */
  WM_main_add_notifier(NC_WINDOW, nullptr);
  WM_main_add_notifier(NC_WM | ND_PROJECT_CHANGED, nullptr);

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

void PROJECT_OT_close(wmOperatorType *ot)
{
  ot->name = "Close Project";
  ot->idname = "PROJECT_OT_close";
  ot->description = "Close the active project, starting a new file";

  ot->invoke = project_close_invoke;
  ot->exec = project_close_exec;
  ot->poll = has_vaild_project_poll;

  PropertyRNA *prop = RNA_def_boolean(ot->srna,
                                      "skip_save_check",
                                      false,
                                      "Skip check to prevent unsaved changes from being lost",
                                      "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

/** \} */

static wmOperatorStatus project_config_save_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  if (BKE_blender_project_config_file_write(bmain)) {
    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

void PROJECT_OT_config_save(wmOperatorType *ot)
{
  ot->name = "Save Project Config";
  ot->idname = "PROJECT_OT_config_save";
  ot->description = "Save the current project config to disk";

  ot->exec = project_config_save_exec;
  ot->poll = has_vaild_project_poll;
}
/* -------------------------------------------------------------------- */
/** \name Add Asset Library Operator
 * \{ */

static wmOperatorStatus project_asset_library_add_exec(bContext *C, wmOperator *op)
{
  const bUserAssetLibraryAddType library_type = bUserAssetLibraryAddType(
      RNA_enum_get(op->ptr, "type"));

  char name[sizeof(bUserAssetLibrary::name)] = "";
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "name");
  if (RNA_property_is_set(op->ptr, prop)) {
    RNA_property_string_get(op->ptr, prop, name);
  }

  switch (library_type) {
    case bUserAssetLibraryAddType::Local: {
      char *dirpath = RNA_string_get_alloc(op->ptr, "directory", nullptr, 0, nullptr);
      /* Store directories inside the project relative to the project root. */
      const std::string dirpath_str = BKE_blender_project_read_callback(
          CTX_data_main(C), [&](const bke::BlenderProject *project) {
            return project ? BKE_blender_project_path_make_relative(dirpath, false, *project) :
                             std::string(dirpath);
          });
      ED_userpref_asset_library_new(C, name, dirpath_str.c_str(), library_type, true, {}, {});
      MEM_delete(dirpath);
      break;
    }
    case bUserAssetLibraryAddType::Remote: {
      char *remote_url = RNA_string_get_alloc(op->ptr, "remote_url", nullptr, 0, nullptr);
      ED_userpref_asset_library_new(C, name, remote_url, library_type, true, {}, {});
      MEM_delete(remote_url);
      break;
    }
  }

  U.runtime.is_dirty = true;
  Main *bmain = CTX_data_main(C);
  project_mark_dirty(bmain);
  return OPERATOR_FINISHED;
}

static wmOperatorStatus project_asset_library_add_invoke(bContext *C,
                                                         wmOperator *op,
                                                         const wmEvent *event)
{
  const bUserAssetLibraryAddType library_type = bUserAssetLibraryAddType(
      RNA_enum_get(op->ptr, "type"));

  if ((library_type == bUserAssetLibraryAddType::Local) &&
      !RNA_struct_property_is_set(op->ptr, "directory"))
  {
    WM_event_add_fileselect(C, op);
    return OPERATOR_RUNNING_MODAL;
  }

  return WM_operator_props_popup_confirm_ex(
      C, op, event, IFACE_("Add Asset Library"), IFACE_("Create"));
}

static void project_asset_library_add_ui(bContext * /*C*/, wmOperator *op)
{
  blender::ui::Layout *layout = op->layout;
  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);

  PointerRNA *ptr = op->ptr;
  const bUserAssetLibraryAddType library_type = bUserAssetLibraryAddType(
      RNA_enum_get(ptr, "type"));
  switch (library_type) {
    case bUserAssetLibraryAddType::Remote: {
      layout->prop(op->ptr, "remote_url", ui::ITEM_R_IMMEDIATE, std::nullopt, ICON_NONE);
      break;
    }
    case bUserAssetLibraryAddType::Local: {
      layout->prop(op->ptr, "name", ui::ITEM_R_IMMEDIATE, std::nullopt, ICON_NONE);
      break;
    }
  }
}

static const EnumPropertyItem *custom_library_type_itemf(bContext * /*C*/,
                                                         PointerRNA * /*ptr*/,
                                                         PropertyRNA * /*prop*/,
                                                         bool *r_free)
{
  *r_free = false;

  if (USER_EXPERIMENTAL_TEST(&U, use_remote_asset_libraries)) {
    /* Experimental flag is enabled, just return all items. */
    return custom_library_type_items;
  }

  /* Since the Remote item is the first in the list, we can just return a pointer to
   * the 2nd item in the list, when remote asset libraries should be hidden. */
  BLI_assert(custom_library_type_items[0].value == int(bUserAssetLibraryAddType::Remote));
  return custom_library_type_items + 1;
}

void PROJECT_OT_asset_library_add(wmOperatorType *ot)
{
  ot->name = "Add Asset Library";
  ot->idname = "PROJECT_OT_asset_library_add";
  ot->description = "Add a directory to be used by the Project as source of assets";

  ot->exec = project_asset_library_add_exec;
  ot->invoke = project_asset_library_add_invoke;
  ot->ui = project_asset_library_add_ui;
  ot->poll = has_vaild_project_poll;

  ot->flag = OPTYPE_INTERNAL | OPTYPE_REGISTER;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER,
                                 FILE_SPECIAL,
                                 FILE_OPENFILE,
                                 WM_FILESEL_DIRECTORY,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);

  /* Copy the RNA values are copied into the operator to avoid repetition. */
  StructRNA *type_ref = RNA_UserAssetLibrary;

  { /* Name. */
    const char *prop_id = "name";
    const PropertyRNA *prop_ref = RNA_struct_type_find_property(type_ref, prop_id);
    PropertyRNA *prop = RNA_def_string(ot->srna,
                                       prop_id,
                                       nullptr,
                                       sizeof(bUserExtensionRepo::name),
                                       RNA_property_ui_name_raw(prop_ref),
                                       RNA_property_ui_description_raw(prop_ref));
    RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  }

  { /* Remote Path. */
    const char *prop_id = "remote_url";
    const PropertyRNA *prop_ref = RNA_struct_type_find_property(type_ref, prop_id);
    PropertyRNA *prop = RNA_def_string(ot->srna,
                                       prop_id,
                                       nullptr,
                                       sizeof(bUserAssetLibrary::remote_url),
                                       RNA_property_ui_name_raw(prop_ref),
                                       RNA_property_ui_description_raw(prop_ref));
    RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  }

  ot->prop = RNA_def_enum(ot->srna,
                          "type",
                          custom_library_type_items,
                          int(bUserAssetLibraryAddType::Local),
                          "Type",
                          "The kind of asset library to add");
  RNA_def_enum_funcs(ot->prop, custom_library_type_itemf);
  RNA_def_property_flag(ot->prop, PROP_SKIP_SAVE | PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remove Asset Library Operator
 * \{ */

static bool project_asset_library_remove_poll(bContext *C)
{
  int num_assets = 0;
  for (bUserAssetLibrary &library : U.asset_libraries) {
    if (library.flag & ASSET_LIBRARY_PROJECT_DEFINED) {
      num_assets++;
    }
  }
  if (num_assets == 0) {
    CTX_wm_operator_poll_msg_set(C, "There is no asset library to remove");
    return false;
  }
  return true;
}

static wmOperatorStatus project_asset_library_remove_exec(bContext *C, wmOperator *op)
{
  const int index = RNA_int_get(op->ptr, "index");
  int num_assets = 0;
  bUserAssetLibrary *library_ptr = nullptr;
  for (bUserAssetLibrary &library : U.asset_libraries) {
    if (library.flag & ASSET_LIBRARY_PROJECT_DEFINED) {
      if (num_assets == index) {
        library_ptr = &library;
      }
      num_assets++;
    }
  }
  if (!library_ptr) {
    return OPERATOR_CANCELLED;
  }
  if (library_ptr->extension_id[0]) {
    BKE_report(
        op->reports,
        RPT_ERROR,
        "Asset libraries defined by an extension are removed by uninstalling the extension");
    return OPERATOR_CANCELLED;
  }

  ED_userpref_asset_library_remove(C, library_ptr);

  Main *bmain = CTX_data_main(C);
  BKE_blender_project_write_callback(bmain, [num_assets](bke::BlenderProject *project) {
    /* num_assets - 2 because we removed one item (-1) and then adjust from size to last possible
     * index.
     */
    CLAMP(project->active_asset_library_index, 0, num_assets - 2);
  });

  U.runtime.is_dirty = true;
  project_mark_dirty(bmain);

  /* Force full redraw of all windows. */
  WM_event_add_notifier(C, NC_WINDOW, nullptr);

  return OPERATOR_FINISHED;
}

void PROJECT_OT_asset_library_remove(wmOperatorType *ot)
{
  ot->name = "Remove Asset Library";
  ot->idname = "PROJECT_OT_asset_library_remove";
  ot->description =
      "Remove a path to a .blend file, so the Asset Browser will not attempt to show it anymore";

  ot->exec = project_asset_library_remove_exec;
  ot->poll = project_asset_library_remove_poll;

  ot->flag = OPTYPE_INTERNAL;

  RNA_def_int(ot->srna, "index", 0, 0, INT_MAX, "Index", "", 0, 1000);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Project Variable Operator
 * \{ */

enum VariableType {
  STRING = 0,
  FILEPATH = 1,
  INTEGER = 2,
  FLOAT = 3,
};

static constexpr EnumPropertyItem project_variable_type_items[] = {
    {VariableType::STRING, "STRING", 0, "String Variable", ""},
    {VariableType::FILEPATH, "FILEPATH", 0, "Filepath Variable", ""},
    {VariableType::INTEGER, "INTEGER", 0, "Integer Variable", ""},
    {VariableType::FLOAT, "FLOAT", 0, "Float Variable", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static wmOperatorStatus project_variable_add_exec(bContext *C, wmOperator *op)
{
  const VariableType variable_type = VariableType(RNA_enum_get(op->ptr, "variable_type"));
  Main *bmain = CTX_data_main(C);
  BKE_blender_project_write_callback(bmain, [variable_type](bke::BlenderProject *project) {
    std::string variable_name;
    bke::ProjectVariableType proj_var_type;
    switch (variable_type) {
      case VariableType::STRING:
        variable_name = "string_variable";
        proj_var_type = bke::ProjectVariableType::STRING;
        break;
      case VariableType::FILEPATH:
        variable_name = "filepath_variable";
        proj_var_type = bke::ProjectVariableType::STRING;
        break;
      case VariableType::INTEGER:
        variable_name = "integer_variable";
        proj_var_type = bke::ProjectVariableType::INT;
        break;
      case VariableType::FLOAT:
        variable_name = "float_variable";
        proj_var_type = bke::ProjectVariableType::FLOAT;
        break;
      default:
        BLI_assert_unreachable();
        break;
    }
    bke::ProjectVariable *new_var = project->new_variable(variable_name, proj_var_type);
    if (variable_type == VariableType::FILEPATH) {
      new_var->string_subtype_set(bke::ProjectVariableStringSubtype::FILEPATH);
    }
    project->active_variable_index = project->variables.size() - 1;
  });
  /* Force full redraw of all windows. */
  WM_event_add_notifier(C, NC_WINDOW, nullptr);

  return OPERATOR_FINISHED;
}

void PROJECT_OT_variable_add(wmOperatorType *ot)
{
  ot->name = "Add Variable";
  ot->idname = "PROJECT_OT_variable_add";
  ot->description = "Add a new variable to the current project";

  ot->exec = project_variable_add_exec;
  ot->poll = has_vaild_project_poll;

  ot->flag = OPTYPE_INTERNAL;

  ot->prop = RNA_def_enum(ot->srna,
                          "variable_type",
                          project_variable_type_items,
                          int(bke::ProjectVariableType::STRING),
                          "Variable Type",
                          "The kind of variable type to add");
  RNA_def_property_flag(ot->prop, PROP_SKIP_SAVE | PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remove Project Variable Operator
 * \{ */
static bool project_variable_remove_poll(bContext *C)
{
  Main *bmain = CTX_data_main(C);
  return BKE_blender_project_read_callback(bmain, [](const bke::BlenderProject *project) {
    return project != nullptr && project->active_variable_index >= 0 &&
           project->active_variable_index < project->variables.size();
  });
}

static wmOperatorStatus project_variable_remove_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  BKE_blender_project_write_callback(bmain, [](bke::BlenderProject *project) {
    project->remove_variable(project->variables[project->active_variable_index].get());
  });
  /* Force full redraw of all windows. */
  WM_event_add_notifier(C, NC_WINDOW, nullptr);
  return OPERATOR_FINISHED;
}

void PROJECT_OT_variable_remove(wmOperatorType *ot)
{
  ot->name = "Remove Variable";
  ot->idname = "PROJECT_OT_variable_remove";
  ot->description = "Remove the active variable from the current project";

  ot->exec = project_variable_remove_exec;
  ot->poll = project_variable_remove_poll;

  ot->flag = OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Move Project Variable Operator
 * \{ */
enum DirectionType {
  UP = 0,
  DOWN = 1,
};

static constexpr EnumPropertyItem project_variable_move_items[] = {
    {DirectionType::UP, "UP", 0, "Move Up", ""},
    {DirectionType::DOWN, "DOWN", 0, "Move Down", ""},
    {0, nullptr, 0, nullptr, nullptr},
};
static wmOperatorStatus project_variable_move_exec(bContext *C, wmOperator *op)
{
  const DirectionType direction = DirectionType(RNA_enum_get(op->ptr, "direction"));
  Main *bmain = CTX_data_main(C);
  BKE_blender_project_write_callback(bmain, [direction](bke::BlenderProject *project) {
    int index = project->active_variable_index;
    if (direction == UP && index > 0) {
      project->move_variable(index, index - 1);
      project->active_variable_index -= 1;
    }
    else if (direction == DOWN && (index + 1) < project->variables.size()) {
      project->move_variable(index, index + 1);
      project->active_variable_index += 1;
    }
  });
  /* Force full redraw of all windows. */
  WM_event_add_notifier(C, NC_WINDOW, nullptr);
  return OPERATOR_FINISHED;
}

void PROJECT_OT_variable_move(wmOperatorType *ot)
{
  ot->name = "Move Variable";
  ot->idname = "PROJECT_OT_variable_move";
  ot->description = "Move the active variable up or down in the list of variables";

  ot->exec = project_variable_move_exec;
  ot->poll = project_variable_remove_poll;

  ot->flag = OPTYPE_INTERNAL;

  ot->prop = RNA_def_enum(ot->srna,
                          "direction",
                          project_variable_move_items,
                          0,
                          "Move Direction",
                          "Which direction to move the variable");
  RNA_def_property_flag(ot->prop, PROP_SKIP_SAVE | PROP_HIDDEN);
}

/** \} */

}  // namespace blender
