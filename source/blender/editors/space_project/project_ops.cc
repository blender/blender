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
#include "BLI_listbase_iterator.hh"
#include "BLI_path_utils.hh"

#include "BLT_translation.hh"

#include "ED_userpref.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "project_intern.hh"
#include "wm_files.hh"

namespace blender {

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

  BKE_blender_project_config_autosave(bmain);

  BKE_blender_project_init(name, root_path);
  if (!BKE_blender_project_config_file_write(bmain)) {
    BKE_blender_project_clear();
    BKE_reportf(op->reports, RPT_ERROR, "Failed to write project config in \"%s\"", root_path);
    return OPERATOR_CANCELLED;
  }
  /* Force full redraw of all windows, and refresh their title. */
  WM_main_add_notifier(NC_WINDOW, nullptr);
  WM_main_add_notifier(NC_WM | ND_PROJECT_CHANGED, nullptr);

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
