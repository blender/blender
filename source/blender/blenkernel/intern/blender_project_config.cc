/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <toml.hpp>

#include "CLG_log.h"

#include "BLI_fileops.hh"
#include "BLI_listbase.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.hh"
#include "BLI_string_ref.hh"
#include "BLI_uuid.hh"

#include "BKE_blender_project.hh"
#include "BKE_global.hh"
#include "BKE_preferences.h"

#include "DNA_asset_types.h"
#include "DNA_userdef_types.h"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

namespace blender {

static CLG_LogRef LOG = {"project"};

namespace bke {
constexpr toml::spec toml_version = toml::spec::v(1, 0, 0);
/* The schema version of the Project Config, for managing breaking schema changes.
 *
 * Only increment this when there are breaking schema changes.
 *
 * Project versioning should generally follow the same approach as blend file
 * versioning. In short:
 *
 * - When reasonably possible, change the schema in ways that fully preserve
 *   backwards compatibility and that avoid critical forwards compatibility
 *   breaks.
 * - When that's not possible, plan ahead and only make the needed breaking
 *   changes on major version bumps of Blender (e.g. 5.x -> 6.x).
 * - In the version just before a major version bump, add forwards compatibility
 *   code for the anticipated breaking changes, so that e.g. the last 5.x release
 *   can still open projects from Blender 6.0.
 *
 * For more details, see:
 * https://developer.blender.org/docs/handbook/guidelines/compatibility_handling_for_blend_files/
 *
 * NOTE: when incrementing this version number, ensure that the versioning code
 * in `create_blender_project_from_toml()` is appropriately expanded to auto-upgrade
 * between versions
 */
const int project_schema_version = 1;

static std::string toml_type_name(const toml::value &value)
{
  switch (value.type()) {
    case toml::value_t::empty:
      return "empty";
    case toml::value_t::boolean:
      return "boolean";
    case toml::value_t::integer:
      return "integer";
    case toml::value_t::floating:
      return "float";
    case toml::value_t::string:
      return "string";
    case toml::value_t::offset_datetime:
      return "offset datetime";
    case toml::value_t::local_datetime:
      return "local datetime";
    case toml::value_t::local_date:
      return "local date";
    case toml::value_t::local_time:
      return "local time";
    case toml::value_t::array:
      return "array";
    case toml::value_t::table:
      return "table";
  }

  return "unknown";
}

struct TomlProjectVariable {
  std::string name;
  ProjectVariableType type;
  toml::value value;
  ProjectVariableStringSubtype subtype;
  std::string description;

  void from_toml(const toml::value &v)
  {
    this->name = toml::find<std::string>(v, "name");
    if (!is_valid_project_variable_name(this->name)) {
      toml::value var = v.at("name");
      std::string err = toml::format_error(
          "Invalid project variable name",
          var,
          "Invalid variable name: \"" + this->name +
              "\". Variable names can not be empty, must not start with a digit, and "
              "must contain only alphanumeric characters and underscores.");

      throw toml::type_error(err, var.location());
    }

    auto type = toml::find<std::string>(v, "type");
    if (type == "INTEGER") {
      this->type = ProjectVariableType::INT;
    }
    else if (type == "FLOAT") {
      this->type = ProjectVariableType::FLOAT;
    }
    else if (type == "STRING") {
      this->type = ProjectVariableType::STRING;
    }
    else {
      toml::value var = v.at("type");
      std::string err = toml::format_error(
          "Invalid Project variable type",
          var,
          "invalid value for \'type' expected \"INTEGER\", \"FLOAT\", or \"STRING\", but got \"" +
              type + "\"");

      throw toml::type_error(err, var.location());
    }

    this->value = v.at("value");
    toml::value value = this->value;

    if (this->type == ProjectVariableType::INT && !value.is_integer()) {
      std::string err = toml::format_error(
          "Project Variable type and value mismatch",
          value,
          "Actual and declared type of project variable \"value\" do not match. Got \"" +
              toml_type_name(value) + "\", expected an integer");

      throw toml::type_error(err, value.location());
    }
    if (this->type == ProjectVariableType::FLOAT && !value.is_floating()) {
      std::string err = toml::format_error(
          "Project Variable type and value mismatch",
          value,
          "Actual and declared type of project variable \"value\" do not match. Got \"" +
              toml_type_name(value) + "\", expected a float");

      throw toml::type_error(err, value.location());
    }
    if (this->type == ProjectVariableType::STRING) {
      if (!value.is_string()) {
        std::string err = toml::format_error(
            "Project Variable type and value mismatch",
            value,
            "Actual and declared type of project variable \"value\" do not match. Got \"" +
                toml_type_name(value) + "\", expected a string");

        throw toml::type_error(err, value.location());
      }

      auto subtype = toml::find<std::string>(v, "subtype");

      if (subtype == "NONE") {
        this->subtype = ProjectVariableStringSubtype::NONE;
      }
      else if (subtype == "FILEPATH") {
        this->subtype = ProjectVariableStringSubtype::FILEPATH;
      }
      else {
        toml::value var = v.at("subtype");
        std::string err = toml::format_error("Invalid Project variable subtype",
                                             var,
                                             "invalid value for \'subtype' expected \"NONE\" or "
                                             "\"FILEPATH\", but got \"" +
                                                 subtype + "\"");

        throw toml::type_error(err, var.location());
      }
    }
    this->description = toml::find_or(v, "description", "");
  }
};

static void project_variable_add(BlenderProject *project, TomlProjectVariable project_variable)
{
  ProjectVariable *new_var = project->new_variable(project_variable.name, project_variable.type);
  toml::value value = project_variable.value;

  switch (project_variable.type) {
    case ProjectVariableType::INT:
      new_var->value_set(toml::get<int>(value));
      break;
    case ProjectVariableType::FLOAT:
      new_var->value_set(toml::get<float>(value));
      break;
    case ProjectVariableType::STRING:
      new_var->value_set(toml::get<std::string>(value));
      new_var->string_subtype_set(project_variable.subtype);
      break;
    default:
      BLI_assert_unreachable();
      break;
  }

  if (project_variable.description != "") {
    new_var->description_set(project_variable.description);
  }
}

struct TomlAssetLibrary {
  /* Common variables. */
  std::string name;
  int import_method; /* eAssetImportMethod enum */
  /* Remote library variables. */
  bool is_remote_asset_lib =
      false; /* Only used to keep track if "url" was defined when parsing. */
  std::string url;
  std::string access_token;
  /* Local library variables. */
  std::string path;
  bool use_relative_path;
  std::string uuid;

  void from_toml(const toml::value &v)
  {
    this->name = toml::find<std::string>(v, "name");

    std::string import_method_str = toml::find<std::string>(v, "import_method");
    if (!RNA_enum_value_from_identifier(
            rna_enum_asset_import_method_items, import_method_str.c_str(), &this->import_method))
    {
      toml::value var = v.at("import_method");
      std::string err = toml::format_error("Invalid Project asset library import method",
                                           var,
                                           "invalid value for \'import_method. "
                                           "\"" +
                                               import_method_str +
                                               "\" is not a valid import method");

      throw toml::type_error(err, var.location());
    }

    if (v.contains("url")) {
      this->is_remote_asset_lib = true;
      this->url = toml::find<std::string>(v, "url");

      this->access_token = toml::find_or(v, "access_token", "");
      return;
    }

    this->path = toml::find<std::string>(v, "path");

    this->use_relative_path = toml::find<bool>(v, "use_relative_path");

    this->uuid = toml::find_or(v, "uuid", "");
  }
};

static void project_asset_library_add(TomlAssetLibrary project_asset_library)
{
  if (project_asset_library.is_remote_asset_lib) {
    bUserAssetLibrary *new_library = BKE_preferences_project_remote_asset_library_add(
        &U,
        project_asset_library.name.c_str(),
        project_asset_library.url.c_str(),
        project_asset_library.access_token.c_str());

    new_library->import_method = project_asset_library.import_method;
    return;
  }

  std::optional<UUID> uuid = {};
  bool invalid_uuid = false;
  if (project_asset_library.uuid != "") {
    try {
      uuid = UUID(project_asset_library.uuid);
    }
    catch (const std::runtime_error &e) {
      invalid_uuid = true;
    }
  }

  bUserAssetLibrary *new_library = BKE_preferences_project_asset_library_add(
      &U, project_asset_library.name.c_str(), project_asset_library.path.c_str(), uuid);

  if (invalid_uuid) {
    /* User passed an invalid UUID, disable the library and set the invalid_uuid string so we can
     * notify the user about this in the UI.
     */
    new_library->flag |= ASSET_LIBRARY_DISABLED;
    new_library->invalid_uuid = BLI_strdup(project_asset_library.uuid.c_str());
  }

  if (project_asset_library.use_relative_path) {
    new_library->flag |= ASSET_LIBRARY_RELATIVE_PATH;
  }
  new_library->import_method = project_asset_library.import_method;
}

static void create_blender_project_from_toml(Main *bmain,
                                             std::string toml_path,
                                             std::string project_root_path)
{
  /* This blender::fstream is needed to properly handle utf-8 paths on Windows. */
  blender::fstream config_file(toml_path, std::ios::in | std::ios::binary);
  if (!config_file.is_open()) {
    throw std::system_error(errno, std::generic_category(), "Failed to open file: " + toml_path);
  }

  toml::value config_values = toml::parse(config_file, toml_path, toml_version);
  toml::value schema_var = config_values.at("schema_version");
  int version = schema_var.as_integer();

  if (version > project_schema_version) {
    std::string err = toml::format_error(
        "Too new project schema version!",
        schema_var,
        "expected a value equal or below: " + std::to_string(project_schema_version) + "\"");
    throw toml::type_error(err, schema_var.location());
  }

  if (version < project_schema_version) {
    // Versioning code here
  }

  auto project_name = toml::find<std::string>(config_values, "name");
  if (!BKE_blender_project_init(project_name, project_root_path)) {
    throw std::invalid_argument(
        "Couldn't initialize the project because the project name was empty");
  }
  BlenderProject *project = BKE_blender_project_get(bmain);

  if (config_values.contains("ocio_config")) {
    auto ocio_config_path = config_values.at("ocio_config").as_string();
    project->set_ocio_config_path(ocio_config_path);
  }

  if (config_values.contains("variables")) {
    for (const auto &variable : toml::find<toml::array>(config_values, "variables")) {
      auto project_variable = toml::get<TomlProjectVariable>(variable);
      project_variable_add(project, project_variable);
    }
  }

  if (config_values.contains("asset_libraries")) {
    for (const auto &library : toml::find<toml::array>(config_values, "asset_libraries")) {
      auto project_asset_library = toml::get<TomlAssetLibrary>(library);
      project_asset_library_add(project_asset_library);
    }
  }

  if (config_values.contains("splash_image")) {
    auto splash_image_path = config_values.at("splash_image").as_string();
    project->set_splash_image_path(splash_image_path);
  }

  // TODO: check if we didn't access any of the parsed toml variables
  // We need to build the toml lib with "TOML11_ENABLE_ACCESS_CHECK" to do this though.

  /* Done loading, mark project as non dirty. */
  project->is_dirty = false;
}

static toml::ordered_array project_variables_to_toml(BlenderProject *project)
{
  toml::ordered_array toml_array;
  for (auto &project_variable : project->variables) {
    toml::ordered_table variable_data = {{"name", std::string(project_variable->name_get())}};

    ProjectVariableType type = project_variable->type_get();
    ProjectVariableStringSubtype string_subtype;

    const char *idname;
    if (RNA_enum_id_from_value(rna_enum_project_variable_type_items, int(type), &idname) == 0) {
      /* This lookup should never fail. */
      BLI_assert_unreachable();
    }
    variable_data["type"] = std::string(idname);

    switch (project_variable->type_get()) {
      case ProjectVariableType::INT:
        variable_data["value"] = project_variable->value_int_get();
        break;
      case ProjectVariableType::FLOAT:
        variable_data["value"] = project_variable->value_float_get();
        break;
      case ProjectVariableType::STRING:
        variable_data["value"] = std::string(project_variable->value_string_get());
        string_subtype = project_variable->string_subtype_get();
        if (RNA_enum_id_from_value(
                rna_enum_project_variable_string_subtype_items, int(string_subtype), &idname) == 0)
        {
          /* This lookup should never fail. */
          BLI_assert_unreachable();
        }
        variable_data["subtype"] = std::string(idname);
        break;
      default:
        BLI_assert_unreachable();
        break;
    }

    StringRefNull description = project_variable->description_get();

    if (!description.is_empty()) {
      variable_data["description"] = std::string(description);
    }

    toml_array.push_back(variable_data);
  }
  return toml_array;
}

static toml::ordered_array project_asset_libraries_to_toml()
{
  toml::ordered_array toml_array;
  for (auto &user_library : U.asset_libraries.items_mutable()) {
    if ((user_library.flag & ASSET_LIBRARY_PROJECT_DEFINED)) {
      const char *import_method;
      if (RNA_enum_id_from_value(rna_enum_asset_import_method_items,
                                 int(user_library.import_method),
                                 &import_method) == 0)
      {
        /* This lookup should never fail. */
        BLI_assert_unreachable();
      }
      toml::ordered_table asset_library_data;
      if (user_library.flag & ASSET_LIBRARY_USE_REMOTE_URL) {
        /* remote asset library */
        asset_library_data = {{"name", user_library.name},
                              {"import_method", std::string(import_method)},
                              {"url", user_library.remote_url}};
        if (user_library.flag & ASSET_LIBRARY_USE_AUTH_TOKEN && user_library.auth_token) {
          asset_library_data["access_token"] = std::string(user_library.auth_token);
        }
      }
      else {
        /* Local asset library */
        UUID uuid = UUID(user_library.uuid);
        asset_library_data = {
            {"name", user_library.name},
            {"import_method", std::string(import_method)},
            {"path", user_library.dirpath},
            {"use_relative_path", bool(user_library.flag & ASSET_LIBRARY_RELATIVE_PATH)},
            {"uuid", uuid.str()}};
      }
      toml_array.push_back(asset_library_data);
    }
  }
  return toml_array;
}

static toml::ordered_table create_toml_from_blender_project(BlenderProject *project)
{
  toml::ordered_table toml_table = {{"schema_version", project_schema_version},
                                    {"name", std::string(project->get_name())}};

  StringRefNull ocio_config_path = project->get_ocio_config_path();
  if (!ocio_config_path.is_empty()) {
    toml_table["ocio_config"] = std::string(ocio_config_path);
  }

  toml::ordered_array variables = project_variables_to_toml(project);
  if (!variables.empty()) {
    toml_table["variables"] = variables;
  }

  toml::ordered_array asset_libraries = project_asset_libraries_to_toml();
  if (!asset_libraries.empty()) {
    toml_table["asset_libraries"] = asset_libraries;
  }

  StringRefNull splash_image_path = project->get_splash_image_path();
  if (!splash_image_path.is_empty()) {
    toml_table["splash_image"] = std::string(splash_image_path);
  }

  return toml_table;
}

}  // namespace bke

void BKE_blender_project_config_autosave(Main *bmain)
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

bool BKE_blender_project_config_file_write(Main *bmain)
{
  bke::BlenderProject *project = BKE_blender_project_get(bmain);
  if (project == nullptr) {
    CLOG_ERROR(&LOG, "Couldn't save project config, no active project.");
    return false;
  }
  /* Create the directory for new projects. */
  const std::string config_dir = project->get_root_path() + SEP_STR + BLENDER_PROJECT_CONFIG_DIR;
  if (!BLI_dir_create_recursive(config_dir.c_str())) {
    CLOG_ERROR(&LOG, "Failed to create project config directory '%s'", config_dir.c_str());
    return false;
  }

  std::string toml_path = config_dir + SEP_STR + BLENDER_PROJECT_CONFIG_FILE;

  toml::ordered_table toml_config = create_toml_from_blender_project(project);

  /* This blender::fstream is needed to properly handle utf-8 paths on Windows. */
  blender::fstream config_file(toml_path, std::ios::out | std::ios::binary);
  if (!config_file.is_open()) {
    CLOG_ERROR(&LOG,
               "Failed to open project config file '%s' for writing: %s",
               toml_path.c_str(),
               std::strerror(errno));
    return false;
  }
  config_file << toml::format(toml::ordered_value(toml_config), bke::toml_version);
  config_file.close(); /* Flush to get all failures. */
  if (config_file.fail()) {
    return false;
  }

  project->is_dirty = false;

  if (!G.background) {
    /* Update the recent projects in case the project name changed (or if this is a new project).
     */
    BKE_blender_project_recent_update(bmain);
  }

  return true;
}

bool BKE_blender_project_init_from_root_path(Main *bmain, std::string project_root_path)
{
  if (!BLI_is_dir(project_root_path.c_str())) {
    CLOG_ERROR(&LOG, "Project root path '%s', is not a directory", project_root_path.c_str());
    return false;
  }
  std::string toml_path = project_root_path + SEP_STR + BLENDER_PROJECT_CONFIG_DIR + SEP_STR +
                          BLENDER_PROJECT_CONFIG_FILE;
  if (!BLI_is_file(toml_path.c_str())) {
    CLOG_ERROR(&LOG, "Project has no project.toml file");
    return false;
  }

  try {
    bke::create_blender_project_from_toml(bmain, toml_path, project_root_path);
  }
  catch (const std::exception &e) {
    CLOG_ERROR(&LOG, "Failed loading project from toml: \n%s", e.what());
    /* Delete any project and its data if managed to create one before failing with an error. */
    BKE_blender_project_clear();
    return false;
  }
  CLOG_INFO_NOCHECK(&LOG, "Open project: \"%s\"", project_root_path.c_str());
  return true;
}

}  // namespace blender
