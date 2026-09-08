/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include "DNA_userdef_types.h"

#include "ED_userpref.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

#include "BKE_blender_project.hh"
#include "BKE_path_templates.hh"

#include "BLI_string_ref.hh"
#include "BLI_string_utils.hh"
#include "BLI_uuid.hh"

#include "BLT_translation.hh"

#include "WM_api.hh"

namespace blender {

const EnumPropertyItem rna_enum_project_variable_type_items[] = {
    {int(bke::ProjectVariableType::STRING), "STRING", 0, "String", "A string variable"},
    {int(bke::ProjectVariableType::INT), "INTEGER", 0, "Integer", "An integer variable"},
    {int(bke::ProjectVariableType::FLOAT), "FLOAT", 0, "Float", "A floating point variable"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_project_variable_string_subtype_items[] = {
    {int(bke::ProjectVariableStringSubtype::NONE), "NONE", 0, "None", "A standard string"},
    {int(bke::ProjectVariableStringSubtype::FILEPATH),
     "FILEPATH",
     0,
     "Filepath",
     "A string interpreted as a filepath. Will be used as-is (unescaped) when substituted into "
     "part of a filepath"},
    {0, nullptr, 0, nullptr, nullptr},
};

}  // namespace blender

#ifdef RNA_RUNTIME

namespace blender {

using namespace bke;

static void project_mark_dirty(bke::BlenderProject *project)
{
  BLI_assert(project != nullptr);
  bke::with_blender_project_write_lock([&] { project->is_dirty = true; });
}

/* For properties that AREN'T saved to disk as part of the project data. */
static void rna_BlenderProject_ui_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA * /*ptr*/)
{
  /* Force full redraw of all windows. */
  WM_main_add_notifier(NC_WINDOW, nullptr);
}

/* For properties that ARE saved to disk as part of the project data. */
static void rna_BlenderProject_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  bke::BlenderProject *project = static_cast<bke::BlenderProject *>(ptr->data);
  project_mark_dirty(project);

  /* Force full redraw of all windows. */
  WM_main_add_notifier(NC_WINDOW, nullptr);
}

static void rna_ProjectVariable_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  BlenderProject *project = ptr->parent().data_as<BlenderProject>();
  BLI_assert(project != nullptr);

  project_mark_dirty(project);

  /* Force full redraw of all windows. */
  WM_main_add_notifier(NC_WINDOW, nullptr);
}

/* --------------------------------------------------------- */

static StructRNA *rna_ProjectVariable_refine(PointerRNA *ptr)
{
  ProjectVariable *var = ptr->data_as<ProjectVariable>();

  switch (var->type_get()) {
    case ProjectVariableType::STRING:
      return RNA_ProjectVariableString;
    case ProjectVariableType::INT:
      return RNA_ProjectVariableInteger;
    case ProjectVariableType::FLOAT:
      return RNA_ProjectVariableFloat;
    default:
      return RNA_UnknownType;
  }
}

static int rna_ProjectVariable_type_get(PointerRNA *ptr)
{
  int var_type;
  with_blender_project_read_lock([&] {
    const ProjectVariable *var = ptr->data_as<ProjectVariable>();
    var_type = int(var->type_get());
  });
  return var_type;
}

static void rna_ProjectVariable_name_get(PointerRNA *ptr, char *value)
{
  with_blender_project_read_lock([&] {
    ProjectVariable *var = ptr->data_as<ProjectVariable>();
    strcpy(value, var->name_get().c_str());
  });
}

static int rna_ProjectVariable_name_length(PointerRNA *ptr)
{
  int name_length;
  with_blender_project_read_lock([&] {
    const ProjectVariable *var = ptr->data_as<ProjectVariable>();
    name_length = var->name_get().size();
  });
  return name_length;
}

static void rna_ProjectVariable_name_set(PointerRNA *ptr, const char *value)
{
  with_blender_project_write_lock([&] {
    BlenderProject *project = ptr->parent().data_as<BlenderProject>();
    BLI_assert(project != nullptr);

    ProjectVariable *var = ptr->data_as<ProjectVariable>();

    project->rename_variable(project->find_variable_index(var), value);
  });
}

static void rna_ProjectVariable_description_get(PointerRNA *ptr, char *value)
{
  with_blender_project_read_lock([&] {
    const ProjectVariable *var = ptr->data_as<ProjectVariable>();
    strcpy(value, var->description_get().c_str());
  });
}

static int rna_ProjectVariable_description_length(PointerRNA *ptr)
{
  int description_length;
  with_blender_project_read_lock([&] {
    const ProjectVariable *var = ptr->data_as<ProjectVariable>();
    description_length = var->description_get().size();
  });
  return description_length;
}

static void rna_ProjectVariable_description_set(PointerRNA *ptr, const char *value)
{
  with_blender_project_write_lock([&] {
    ProjectVariable *var = ptr->data_as<ProjectVariable>();
    var->description_set(value);
  });
}

static int rna_ProjectVariableInteger_value_get(PointerRNA *ptr)
{
  int value_int;
  with_blender_project_read_lock([&] {
    const ProjectVariable *var = ptr->data_as<ProjectVariable>();
    value_int = var->value_int_get();
  });
  return value_int;
}

static void rna_ProjectVariableInteger_value_set(PointerRNA *ptr, int value)
{
  with_blender_project_write_lock([&] {
    ProjectVariable *var = ptr->data_as<ProjectVariable>();
    var->value_set(value);
  });
}

static float rna_ProjectVariableFloat_value_get(PointerRNA *ptr)
{
  float value_float;
  with_blender_project_read_lock([&] {
    const ProjectVariable *var = ptr->data_as<ProjectVariable>();
    value_float = var->value_float_get();
  });
  return value_float;
}

static void rna_ProjectVariableFloat_value_set(PointerRNA *ptr, float value)
{
  with_blender_project_write_lock([&] {
    ProjectVariable *var = ptr->data_as<ProjectVariable>();
    var->value_set(value);
  });
}

static void rna_ProjectVariableString_value_get(PointerRNA *ptr, char *value)
{
  with_blender_project_read_lock([&] {
    const ProjectVariable *var = ptr->data_as<ProjectVariable>();
    strcpy(value, var->value_string_get().c_str());
  });
}

static int rna_ProjectVariableString_value_length(PointerRNA *ptr)
{
  int string_length;
  with_blender_project_read_lock([&] {
    const ProjectVariable *var = ptr->data_as<ProjectVariable>();
    string_length = var->value_string_get().size();
  });
  return string_length;
}

static void rna_ProjectVariableString_value_set(PointerRNA *ptr, const char *value)
{
  with_blender_project_write_lock([&] {
    ProjectVariable *var = ptr->data_as<ProjectVariable>();
    var->value_set(StringRefNull(value));
  });
}

static int rna_ProjectVariableString_subtype_get(PointerRNA *ptr)
{
  int var_type;
  with_blender_project_read_lock([&] {
    const ProjectVariable *var = ptr->data_as<ProjectVariable>();
    var_type = int(var->string_subtype_get());
  });
  return var_type;
}

static void rna_ProjectVariableString_subtype_set(PointerRNA *ptr, int value)
{
  with_blender_project_write_lock([&] {
    ProjectVariable *var = ptr->data_as<ProjectVariable>();
    var->string_subtype_set(ProjectVariableStringSubtype(value));
  });
}

/* --------------------------------------------------------- */

static void rna_BlenderProject_name_get(PointerRNA *ptr, char *value)
{
  bke::with_blender_project_read_lock([&] {
    const bke::BlenderProject *project = static_cast<bke::BlenderProject *>(ptr->data);
    strcpy(value, project->get_name().c_str());
  });
}

static int rna_BlenderProject_name_length(PointerRNA *ptr)
{
  int name_length;
  bke::with_blender_project_read_lock([&] {
    const bke::BlenderProject *project = static_cast<bke::BlenderProject *>(ptr->data);
    name_length = project->get_name().size();
  });
  return name_length;
}

static void rna_BlenderProject_name_set(PointerRNA *ptr, const char *value)
{
  bke::with_blender_project_write_lock([&] {
    bke::BlenderProject *project = static_cast<bke::BlenderProject *>(ptr->data);

    StringRef name = StringRef(value);

    if (name.is_empty()) {
      /* Leave the name as-is when passed an empty (which is invalid) name. */
      return;
    }

    project->set_name(name);
  });
}

static void rna_BlenderProject_root_path_get(PointerRNA *ptr, char *value)
{
  bke::with_blender_project_read_lock([&] {
    const bke::BlenderProject *project = static_cast<bke::BlenderProject *>(ptr->data);
    strcpy(value, project->get_root_path().c_str());
  });
}

static int rna_BlenderProject_root_path_length(PointerRNA *ptr)
{
  int root_path_length;
  bke::with_blender_project_read_lock([&] {
    const bke::BlenderProject *project = static_cast<bke::BlenderProject *>(ptr->data);
    root_path_length = project->get_root_path().size();
  });
  return root_path_length;
}

static int rna_BlenderProject_active_variable_index_get(PointerRNA *ptr)
{
  int active_variable_index;
  bke::with_blender_project_read_lock([&] {
    const bke::BlenderProject *project = ptr->data_as<BlenderProject>();
    active_variable_index = project->active_variable_index;
  });
  return active_variable_index;
}

static void rna_BlenderProject_active_variable_index_set(PointerRNA *ptr, int value)
{
  bke::with_blender_project_write_lock([&] {
    bke::BlenderProject *project = ptr->data_as<BlenderProject>();
    project->active_variable_index = value;
  });
}

static void rna_BlenderProject_active_variable_index_range(
    PointerRNA *ptr, int *min, int *max, int * /*softmin*/, int * /*softmax*/)
{
  bke::with_blender_project_read_lock([&] {
    const bke::BlenderProject *project = ptr->data_as<BlenderProject>();
    *min = 0;
    *max = project->variables.size() - 1;
  });
}

static void rna_iterator_BlenderProject_variables_begin(CollectionPropertyIterator *iter,
                                                        PointerRNA *ptr)
{
  /* Note: we use a read lock here despite `rna_iterator_array_begin()` taking
   * non-const pointers from the project, because in reality this is a read
   * operation that doesn't modify any project data. */
  bke::with_blender_project_read_lock([&] {
    bke::BlenderProject *project = ptr->data_as<BlenderProject>();
    rna_iterator_array_begin(iter,
                             ptr,
                             (void *)project->variables.begin(),
                             sizeof(std::unique_ptr<IDProperty, idprop::IDPropertyDeleter>),
                             project->variables.size(),
                             0,
                             nullptr);
  });
}

static int rna_iterator_BlenderProject_variables_length(PointerRNA *ptr)
{
  int variables_length;
  bke::with_blender_project_read_lock([&] {
    const bke::BlenderProject *project = ptr->data_as<bke::BlenderProject>();
    variables_length = project->variables.size();
  });
  return variables_length;
}

static PointerRNA rna_iterator_BlenderProject_variables_get(CollectionPropertyIterator *iter)
{
  PointerRNA variable;
  bke::with_blender_project_read_lock([&] {
    BLI_assert(iter->valid);

    ArrayIterator *internal = &iter->internal.array;

    std::unique_ptr<IDProperty, idprop::IDPropertyDeleter> *var_ptr_ptr =
        reinterpret_cast<std::unique_ptr<IDProperty, idprop::IDPropertyDeleter> *>(internal->ptr);

    IDProperty *var_ptr = var_ptr_ptr->get();

    variable = RNA_pointer_create_with_parent(iter->parent, RNA_ProjectVariable, var_ptr);
  });
  return variable;
}

static PointerRNA rna_ProjectVariables_new(BlenderProject *project,
                                           ReportList *reports,
                                           const char *name,
                                           int type)
{
  if (!is_valid_project_variable_name(name)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Invalid variable name '%s': name must not be empty, must not start with a digit, "
                "and must contain only alphanumeric characters and underscores.",
                name);
    return {};
  }

  PointerRNA variable;
  bke::with_blender_project_write_lock([&] {
    ProjectVariable *new_var = project->new_variable(name, ProjectVariableType(type));

    project->active_variable_index = project->variables.size() - 1;

    variable = RNA_pointer_create_with_parent(
        RNA_pointer_create_discrete(nullptr, RNA_BlenderProject, project),
        RNA_ProjectVariable,
        new_var);
  });
  project_mark_dirty(project);
  WM_main_add_notifier(NC_WINDOW, nullptr);
  return variable;
}

void rna_ProjectVariables_remove(BlenderProject *project,
                                 ReportList *reports,
                                 PointerRNA *variable_ptr)
{
  BLI_assert(ELEM(variable_ptr->type,
                  RNA_ProjectVariableString,
                  RNA_ProjectVariableInteger,
                  RNA_ProjectVariableFloat));

  bke::with_blender_project_write_lock([&] {
    ProjectVariable *var = variable_ptr->data_as<ProjectVariable>();
    if (project->remove_variable(var) == -1) {
      BKE_reportf(reports, RPT_ERROR, "Variable not found in project variables.");
      return;
    }
  });
  project_mark_dirty(project);
  WM_main_add_notifier(NC_WINDOW, nullptr);
}

void rna_ProjectVariables_move(BlenderProject *project,
                               ReportList *reports,
                               int from_index,
                               int to_index)
{
  bke::with_blender_project_write_lock([&] {
    if (from_index >= project->variables.size()) {
      BKE_reportf(reports, RPT_ERROR, "From index is out of bounds of the variable list.");
      return;
    }

    if (to_index >= project->variables.size()) {
      BKE_reportf(reports, RPT_ERROR, "To index is out of bounds of the variable list.");
      return;
    }

    project->move_variable(from_index, to_index);
  });
  project_mark_dirty(project);
  WM_main_add_notifier(NC_WINDOW, nullptr);
}

/* --------------------------------------------------------- */

static bool rna_BlenderProject_is_dirty_get(PointerRNA *ptr)
{
  bool is_dirty;
  bke::with_blender_project_read_lock([&] {
    bke::BlenderProject *project = static_cast<bke::BlenderProject *>(ptr->data);
    is_dirty = project->is_dirty;
  });
  return is_dirty;
}

static void rna_BlenderProject_is_dirty_set(PointerRNA *ptr, bool value)
{
  bke::with_blender_project_write_lock([&] {
    bke::BlenderProject *project = static_cast<bke::BlenderProject *>(ptr->data);
    project->is_dirty = value;
  });
}

static bool skip_assetlist_item(CollectionPropertyIterator * /*iter*/, void *data)
{
  bUserAssetLibrary *asset_library = static_cast<bUserAssetLibrary *>(data);
  return !(asset_library->flag & ASSET_LIBRARY_PROJECT_DEFINED);
}

static void rna_BlenderProject_asset_library_list_begin(CollectionPropertyIterator *iter,
                                                        PointerRNA *ptr)
{
  rna_iterator_listbase_begin(iter, ptr, &U.asset_libraries, skip_assetlist_item);
}

static int rna_BlenderProject_active_asset_library_get(PointerRNA *ptr)
{
  int active_index;
  bke::with_blender_project_read_lock([&] {
    const bke::BlenderProject *project = static_cast<bke::BlenderProject *>(ptr->data);
    active_index = project->active_asset_library_index;
  });
  return active_index;
}

static void rna_BlenderProject_active_asset_library_set(PointerRNA *ptr, int value)
{
  bke::with_blender_project_write_lock([&] {
    bke::BlenderProject *project = static_cast<bke::BlenderProject *>(ptr->data);
    project->active_asset_library_index = value;
  });
}

static bUserAssetLibrary *rna_BlenderProject_asset_library_new(const bContext *C,
                                                               const char *name,
                                                               const char *directory,
                                                               const char *uuid_str)
{
  bUserAssetLibrary *new_library;
  Main *bmain = CTX_data_main(C);

  BKE_blender_project_write_callback(bmain, [&](bke::BlenderProject *project) {
    std::optional<UUID> uuid = {};
    bool invalid_uuid = false;
    if (uuid_str) {
      try {
        uuid = UUID(uuid_str);
      }
      catch (const std::runtime_error &e) {
        invalid_uuid = true;
      }
    }

    new_library = ED_userpref_asset_library_new(C,
                                                name ? name : "",
                                                directory ? directory : "",
                                                bUserAssetLibraryAddType::Local,
                                                true,
                                                uuid,
                                                {});
    if (invalid_uuid) {
      /* User passed an invalid UUID, disable the library and set the invalid_uuid string so we can
       * notify the user about this in the UI.
       */
      new_library->flag |= ASSET_LIBRARY_DISABLED;
      new_library->invalid_uuid = BLI_strdup(uuid_str);
    }

    int project_asset_index = -1;
    for (bUserAssetLibrary &library : U.asset_libraries) {
      if (library.flag & ASSET_LIBRARY_PROJECT_DEFINED) {
        project_asset_index++;
      }
      if (&library == new_library) {
        break;
      }
    }

    project->active_asset_library_index = project_asset_index;
    project->is_dirty = true;
  });
  /* Force full redraw of all windows. (No notifier to redraw just the project asset windows yet)
   */
  WM_main_add_notifier(NC_WINDOW, nullptr);
  return new_library;
}

static void rna_BlenderProject_asset_library_remove(bContext *C,
                                                    ReportList *reports,
                                                    PointerRNA *ptr)
{
  bUserAssetLibrary *library = static_cast<bUserAssetLibrary *>(ptr->data);
  Main *bmain = CTX_data_main(C);

  BKE_blender_project_write_callback(bmain, [&](bke::BlenderProject *project) {
    if (BLI_findindex(&U.asset_libraries, library) == -1) {
      BKE_report(reports, RPT_ERROR, "Asset Library not found");
      return;
    }

    ED_userpref_asset_library_remove(C, library);

    int count_remaining = 0;
    for (bUserAssetLibrary &library : U.asset_libraries) {
      if (library.flag & ASSET_LIBRARY_PROJECT_DEFINED) {
        count_remaining++;
      }
    }
    CLAMP(project->active_asset_library_index, 0, count_remaining - 1);

    ptr->invalidate();
    project->is_dirty = true;
  });
  /* Force full redraw of all windows.(No notifier to redraw just the project asset windows yet) */
  WM_main_add_notifier(NC_WINDOW, nullptr);
}

}  // namespace blender

#else

namespace blender {

static void rna_def_project_variable_string(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ProjectVariableString", "ProjectVariable");
  RNA_def_struct_ui_text(srna, "String Project Variable", "A project variable of type string");

  prop = RNA_def_property(srna, "value", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Value", "The variable's string/path value");
  RNA_def_property_string_funcs(prop,
                                "rna_ProjectVariableString_value_get",
                                "rna_ProjectVariableString_value_length",
                                "rna_ProjectVariableString_value_set");
  RNA_def_property_update(prop, 0, "rna_ProjectVariable_update");

  prop = RNA_def_property(srna, "subtype", PROP_ENUM, PROP_NONE);
  RNA_def_property_ui_text(prop, "Subtype", "The string variable's subtype");
  RNA_def_property_enum_items(prop, rna_enum_project_variable_string_subtype_items);
  RNA_def_property_enum_funcs(prop,
                              "rna_ProjectVariableString_subtype_get",
                              "rna_ProjectVariableString_subtype_set",
                              nullptr);
}

static void rna_def_project_variable_integer(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ProjectVariableInteger", "ProjectVariable");
  RNA_def_struct_ui_text(srna, "Integer Project Variable", "A project variable of type integer");

  prop = RNA_def_property(srna, "value", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Value", "The variable's integer value");
  RNA_def_property_int_funcs(prop,
                             "rna_ProjectVariableInteger_value_get",
                             "rna_ProjectVariableInteger_value_set",
                             nullptr);
  RNA_def_property_update(prop, 0, "rna_ProjectVariable_update");
}

static void rna_def_project_variable_float(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ProjectVariableFloat", "ProjectVariable");
  RNA_def_struct_ui_text(srna, "Float Project Variable", "A project variable of type float");

  prop = RNA_def_property(srna, "value", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Value", "The variable's float value");
  RNA_def_property_float_funcs(
      prop, "rna_ProjectVariableFloat_value_get", "rna_ProjectVariableFloat_value_set", nullptr);
  RNA_def_property_update(prop, 0, "rna_ProjectVariable_update");
}

static void rna_def_project_variable(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ProjectVariable", nullptr);
  RNA_def_struct_ui_text(srna, "Blender Project Variable", "");
  RNA_def_struct_refine_func(srna, "rna_ProjectVariable_refine");

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_ui_text(prop, "Type", "The variable's data type");
  RNA_def_property_enum_items(prop, rna_enum_project_variable_type_items);
  RNA_def_property_enum_funcs(prop, "rna_ProjectVariable_type_get", nullptr, nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_ui_text(prop,
                           "Name",
                           "The variable's name. Must not start with a digit, and must contain "
                           "only alphanumeric characters and underscores");
  RNA_def_property_string_funcs(prop,
                                "rna_ProjectVariable_name_get",
                                "rna_ProjectVariable_name_length",
                                "rna_ProjectVariable_name_set");
  RNA_def_property_update(prop, 0, "rna_ProjectVariable_update");

  prop = RNA_def_property(srna, "description", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Description", "Description of the variable (e.g. purpose, semantics, etc.)");
  RNA_def_property_string_funcs(prop,
                                "rna_ProjectVariable_description_get",
                                "rna_ProjectVariable_description_length",
                                "rna_ProjectVariable_description_set");
  RNA_def_property_update(prop, 0, "rna_ProjectVariable_update");

  /* Define ProjectVariable subtypes. */
  rna_def_project_variable_string(brna);
  rna_def_project_variable_integer(brna);
  rna_def_project_variable_float(brna);
}

static void rna_def_ProjectVariables(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "ProjectVariables");
  srna = RNA_def_struct(brna, "ProjectVariables", nullptr);
  RNA_def_struct_sdna(srna, "BlenderProject");
  RNA_def_struct_ui_text(srna, "Project Variables", "Collection of project variables");

  /* BlenderProject.variables.new(...) */
  func = RNA_def_function(srna, "new", "rna_ProjectVariables_new");
  RNA_def_function_ui_description(func, "Add a new variable to the project");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "name", "Variable", 0, "Name", "Name of the new variable");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_enum(func,
                      "type",
                      rna_enum_project_variable_type_items,
                      int(eIDPropertyType::IDP_STRING),
                      "Variable Type",
                      "The data type of the variable");
  parm = RNA_def_pointer(
      func, "variable", "ProjectVariable", "", "Newly created project variable");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR);
  RNA_def_function_return(func, parm);

  /* BlenderProject.variables.remove(variable) */
  func = RNA_def_function(srna, "remove", "rna_ProjectVariables_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove a variable from the project");
  parm = RNA_def_pointer(
      func, "variable", "ProjectVariable", "Variable", "The variable to remove");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED | PARM_RNAPTR);

  /* BlenderProject.variables.move(from_index, to_index) */
  func = RNA_def_function(srna, "move", "rna_ProjectVariables_move");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(
      func, "Move a variable from one position to another in the list of variables");
  parm = RNA_def_int(func,
                     "from_index",
                     0,
                     0,
                     INT_MAX,
                     "From Index",
                     "The index of the variable to move",
                     0,
                     INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "to_index",
                     0,
                     0,
                     INT_MAX,
                     "To Index",
                     "The index to move the variable to",
                     0,
                     INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

static void rna_def_project_asset_library(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "ProjectAssetLibrary", "UserAssetLibrary");
  RNA_def_struct_sdna(srna, "bUserAssetLibrary");
  RNA_def_struct_ui_text(srna,
                         "Project Asset Library",
                         "Settings to define a reusable library for Asset Browsers to use");
}

static void rna_def_project_asset_library_collection(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "ProjectAssetLibraryCollection");
  srna = RNA_def_struct(brna, "ProjectAssetLibraryCollection", nullptr);
  RNA_def_struct_ui_text(srna, "Project Asset Libraries", "Collection of project asset libraries");

  func = RNA_def_function(srna, "new", "rna_BlenderProject_asset_library_new");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(func, "Add a new Project Asset Library");
  RNA_def_string(func, "name", nullptr, sizeof(bUserAssetLibrary::name), "Name", "");
  RNA_def_string(func, "directory", nullptr, sizeof(bUserAssetLibrary::dirpath), "Directory", "");
  RNA_def_string(func, "uuid", nullptr, sizeof(bUserAssetLibrary::uuid), "UUID", "");
  /* return type */
  parm = RNA_def_pointer(func, "library", "ProjectAssetLibrary", "", "Newly added asset library");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_BlenderProject_asset_library_remove");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove a Project Asset Library");
  parm = RNA_def_pointer(func, "library", "ProjectAssetLibrary", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
}

static void rna_def_blender_project(BlenderRNA *brna)
{
  StructRNA *srna = RNA_def_struct(brna, "BlenderProject", nullptr);
  RNA_def_struct_ui_text(srna, "Blender Project", "");

  PropertyRNA *prop;
  prop = RNA_def_property(srna, "is_dirty", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_BlenderProject_is_dirty_get", "rna_BlenderProject_is_dirty_set");
  RNA_def_property_ui_text(prop, "Dirty", "Whether the project has unsaved changes");
  RNA_def_property_update(prop, 0, "rna_BlenderProject_ui_update");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_BlenderProject_name_get",
                                "rna_BlenderProject_name_length",
                                "rna_BlenderProject_name_set");
  RNA_def_property_ui_text(prop, "Name", "The project's name");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, 0, "rna_BlenderProject_update");

  prop = RNA_def_property(srna, "root_path", PROP_STRING, PROP_DIRPATH);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_string_funcs(
      prop, "rna_BlenderProject_root_path_get", "rna_BlenderProject_root_path_length", nullptr);
  RNA_def_property_ui_text(prop, "Root Folder", "The path to the root folder of the project");

  prop = RNA_def_property(srna, "active_variable_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_funcs(prop,
                             "rna_BlenderProject_active_variable_index_get",
                             "rna_BlenderProject_active_variable_index_set",
                             "rna_BlenderProject_active_variable_index_range");
  RNA_def_property_ui_text(
      prop, "Active Project Variable", "Index of the currently active variable in the UI");

  /* Collection properties. */
  prop = RNA_def_property(srna, "variables", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "ProjectVariable");
  RNA_def_property_collection_funcs(prop,
                                    "rna_iterator_BlenderProject_variables_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_BlenderProject_variables_get",
                                    "rna_iterator_BlenderProject_variables_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(prop, "Project Variables", "The variables in this project");
  rna_def_ProjectVariables(brna, prop);

  prop = RNA_def_property(srna, "asset_libraries", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_BlenderProject_asset_library_list_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_iterator_listbase_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "ProjectAssetLibrary");
  RNA_def_property_ui_text(prop, "Project Asset Libraries", "");

  rna_def_project_asset_library_collection(brna, prop);
  rna_def_project_asset_library(brna);

  prop = RNA_def_property(srna, "active_asset_library", PROP_INT, PROP_NONE);
  RNA_def_property_int_funcs(prop,
                             "rna_BlenderProject_active_asset_library_get",
                             "rna_BlenderProject_active_asset_library_set",
                             nullptr);
  RNA_def_property_ui_text(prop,
                           "Active Asset Library",
                           "Index of the asset library being edited in the Project Setup UI");
}

void RNA_def_blender_project(BlenderRNA *brna)
{
  rna_def_project_variable(brna);
  rna_def_blender_project(brna);
}

}  // namespace blender

#endif
