/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <fstream>

#include "BKE_blender_project.h"
#include "BKE_blender_project.hh"

#include "BLI_path_util.h"
#include "BLI_serialize.hh"
#include "BLI_string.h"

#include "DNA_space_types.h"

#include "BLI_fileops.h"
#include "BLI_path_util.h"

namespace serialize = blender::io::serialize;

namespace blender::bke {

static StringRef path_strip_trailing_native_slash(StringRef path);
static bool path_contains_project_settings(StringRef path);

BlenderProject::BlenderProject(std::unique_ptr<ProjectSettings> settings)
    : settings_(std::move(settings))
{
}

BlenderProject *BlenderProject::set_active_from_settings(std::unique_ptr<ProjectSettings> settings)
{
  if (settings) {
    active_ = std::make_unique<BlenderProject>(BlenderProject(std::move(settings)));
  }
  else {
    active_ = nullptr;
  }

  return active_.get();
}

BlenderProject *BlenderProject::get_active()
{
  return active_.get();
}

StringRef BlenderProject::project_root_path_find_from_path(StringRef path)
{
  std::string path_native = path;
  BLI_path_slash_native(path_native.data());

  StringRef cur_path = path;

  while (cur_path.size()) {
    std::string pa = StringRef(path_native.c_str(), cur_path.size());
    if (bke::path_contains_project_settings(pa)) {
      return path.substr(0, cur_path.size());
    }

    /* Walk "up the path" (check the parent next). */
    const int64_t pos_last_slash = cur_path.find_last_of(SEP);
    if (pos_last_slash == StringRef::not_found) {
      break;
    }
    cur_path = cur_path.substr(0, pos_last_slash);
  }

  return "";
}

ProjectSettings &BlenderProject::get_settings() const
{
  BLI_assert(settings_ != nullptr);
  return *settings_;
}

/* ---------------------------------------------------------------------- */

ProjectSettings::ProjectSettings(StringRef project_root_path)
    : project_root_path_(project_root_path)
{
}

bool ProjectSettings::create_settings_directory(StringRef project_root_path)
{
  std::string project_root_path_native = project_root_path;
  BLI_path_slash_native(project_root_path_native.data());

  return BLI_dir_create_recursive(
      std::string(project_root_path_native + SEP + SETTINGS_DIRNAME).c_str());
}

static StringRef path_strip_trailing_native_slash(StringRef path)
{
  const int64_t pos_before_trailing_slash = path.find_last_not_of(SEP);
  return (pos_before_trailing_slash == StringRef::not_found) ?
             path :
             path.substr(0, pos_before_trailing_slash + 1);
}

static bool path_contains_project_settings(StringRef path)
{
  path = path_strip_trailing_native_slash(path);
  return BLI_exists(std::string(path + SEP_STR + ProjectSettings::SETTINGS_DIRNAME).c_str());
}

struct ExtractedSettings {
  std::string project_name;
};

static std::unique_ptr<serialize::Value> read_settings_file(StringRef settings_filepath)
{
  std::ifstream is;
  is.open(settings_filepath);
  if (is.fail()) {
    return nullptr;
  }

  serialize::JsonFormatter formatter;
  /* Will not be a dictionary in case of error (corrupted file). */
  std::unique_ptr<serialize::Value> deserialized_values = formatter.deserialize(is);
  is.close();

  if (deserialized_values->type() != serialize::eValueType::Dictionary) {
    return nullptr;
  }

  return deserialized_values;
}

static std::unique_ptr<ExtractedSettings> extract_settings(
    const serialize::DictionaryValue &dictionary)
{
  using namespace serialize;

  std::unique_ptr extracted_settings = std::make_unique<ExtractedSettings>();

  const DictionaryValue::Lookup attributes = dictionary.create_lookup();
  const DictionaryValue::LookupValue *project_value = attributes.lookup_ptr("project");
  BLI_assert(project_value != nullptr);

  const DictionaryValue *project_dict = (*project_value)->as_dictionary_value();
  const StringValue *project_name_value =
      project_dict->create_lookup().lookup("name")->as_string_value();
  if (project_name_value) {
    extracted_settings->project_name = project_name_value->value();
  }

  return extracted_settings;
}

struct ResolvedPaths {
  std::string settings_filepath;
  std::string project_root_path;
};

/**
 * Returned paths can be assumed to use native slashes.
 */
static ResolvedPaths resolve_paths_from_project_path(StringRef project_path)
{
  std::string project_path_native = project_path;
  BLI_path_slash_native(project_path_native.data());

  ResolvedPaths resolved_paths{};

  const StringRef path_no_trailing_slashes = path_strip_trailing_native_slash(project_path_native);
  if (path_no_trailing_slashes.endswith(ProjectSettings::SETTINGS_DIRNAME)) {
    resolved_paths.project_root_path =
        StringRef(path_no_trailing_slashes).drop_suffix(ProjectSettings::SETTINGS_DIRNAME.size());
  }
  else {
    resolved_paths.project_root_path = std::string(path_no_trailing_slashes) + SEP;
  }
  resolved_paths.settings_filepath = resolved_paths.project_root_path +
                                     ProjectSettings::SETTINGS_DIRNAME + SEP +
                                     ProjectSettings::SETTINGS_FILENAME;

  return resolved_paths;
}

std::unique_ptr<ProjectSettings> ProjectSettings::load_from_disk(StringRef project_path)
{
  ResolvedPaths paths = resolve_paths_from_project_path(project_path);

  if (!BLI_exists(paths.project_root_path.c_str())) {
    return nullptr;
  }
  if (!path_contains_project_settings(paths.project_root_path.c_str())) {
    return nullptr;
  }

  std::unique_ptr<serialize::Value> values = read_settings_file(paths.settings_filepath);
  std::unique_ptr<ExtractedSettings> extracted_settings = nullptr;
  if (values) {
    BLI_assert(values->as_dictionary_value() != nullptr);
    extracted_settings = extract_settings(*values->as_dictionary_value());
  }

  std::unique_ptr loaded_settings = std::make_unique<ProjectSettings>(paths.project_root_path);
  if (extracted_settings) {
    loaded_settings->project_name_ = extracted_settings->project_name;
  }

  return loaded_settings;
}

std::unique_ptr<serialize::DictionaryValue> ProjectSettings::to_dictionary() const
{
  using namespace serialize;

  std::unique_ptr<DictionaryValue> root = std::make_unique<DictionaryValue>();
  DictionaryValue::Items &root_attributes = root->elements();
  std::unique_ptr<DictionaryValue> project_dict = std::make_unique<DictionaryValue>();
  DictionaryValue::Items &project_attributes = project_dict->elements();
  project_attributes.append_as("name", new StringValue(project_name_));
  root_attributes.append_as("project", std::move(project_dict));

  return root;
}

static void write_settings_file(StringRef settings_filepath,
                                std::unique_ptr<serialize::DictionaryValue> dictionary)
{
  using namespace serialize;

  JsonFormatter formatter;

  std::ofstream os;
  os.open(settings_filepath, std::ios::out | std::ios::trunc);
  formatter.serialize(os, *dictionary);
  os.close();
}

bool ProjectSettings::save_to_disk(StringRef project_path)
{
  ResolvedPaths paths = resolve_paths_from_project_path(project_path);

  if (!BLI_exists(paths.project_root_path.c_str())) {
    return false;
  }
  if (!path_contains_project_settings(paths.project_root_path.c_str())) {
    return false;
  }

  std::unique_ptr settings_as_dict = to_dictionary();
  write_settings_file(paths.settings_filepath, std::move(settings_as_dict));

  has_unsaved_changes_ = false;

  return true;
}

StringRefNull ProjectSettings::project_root_path() const
{
  return project_root_path_;
}

void ProjectSettings::project_name(StringRef new_name)
{
  project_name_ = new_name;
  has_unsaved_changes_ = true;
}

StringRefNull ProjectSettings::project_name() const
{
  return project_name_;
}

bool ProjectSettings::has_unsaved_changes() const
{
  return has_unsaved_changes_;
}

}  // namespace blender::bke

/* ---------------------------------------------------------------------- */

using namespace blender;

bool BKE_project_create_settings_directory(const char *project_root_path)
{
  return bke::ProjectSettings::create_settings_directory(project_root_path);
}

BlenderProject *BKE_project_active_get(void)
{
  return reinterpret_cast<BlenderProject *>(bke::BlenderProject::get_active());
}

void BKE_project_active_unset(void)
{
  bke::BlenderProject::set_active_from_settings(nullptr);
}

bool BKE_project_contains_path(const char *path)
{
  const StringRef found_root_path = bke::BlenderProject::project_root_path_find_from_path(path);
  return !found_root_path.is_empty();
}

BlenderProject *BKE_project_active_load_from_path(const char *path)
{
  /* Project should be unset if the path doesn't contain a project root. Unset in the beginning so
   * early exiting behaves correctly. */
  BKE_project_active_unset();

  StringRef project_root = bke::BlenderProject::project_root_path_find_from_path(path);
  if (project_root.is_empty()) {
    return nullptr;
  }

  std::unique_ptr project_settings = bke::ProjectSettings::load_from_disk(project_root);
  if (!project_settings) {
    return nullptr;
  }

  bke::BlenderProject::set_active_from_settings(std::move(project_settings));

  return BKE_project_active_get();
}

bool BKE_project_settings_save(const BlenderProject *project_handle)
{
  const bke::BlenderProject *project = reinterpret_cast<const bke::BlenderProject *>(
      project_handle);
  bke::ProjectSettings &settings = project->get_settings();
  return settings.save_to_disk(settings.project_root_path());
}

const char *BKE_project_root_path_get(const BlenderProject *project_handle)
{
  const bke::BlenderProject *project = reinterpret_cast<const bke::BlenderProject *>(
      project_handle);
  return project->get_settings().project_root_path().c_str();
}

void BKE_project_name_set(const BlenderProject *project_handle, const char *name)
{
  const bke::BlenderProject *project = reinterpret_cast<const bke::BlenderProject *>(
      project_handle);
  project->get_settings().project_name(name);
}

const char *BKE_project_name_get(const BlenderProject *project_handle)
{
  const bke::BlenderProject *project = reinterpret_cast<const bke::BlenderProject *>(
      project_handle);
  return project->get_settings().project_name().c_str();
}

bool BKE_project_has_unsaved_changes(const BlenderProject *project_handle)
{
  const bke::BlenderProject *project = reinterpret_cast<const bke::BlenderProject *>(
      project_handle);
  const bke::ProjectSettings &settings = project->get_settings();
  return settings.has_unsaved_changes();
}
