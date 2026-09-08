/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <mutex>
#include <shared_mutex>

#include "BKE_preferences.h"
#include "BLI_listbase.hh"
#include "RNA_types.hh"

#include "BKE_blender_project.hh"
#include "BKE_global.hh"
#include "BKE_main.hh"

#include "BLI_function_ref.hh"
#include "BLI_string.hh"
#include "BLI_string_ref.hh"
#include "BLI_string_utils.hh"

#include "DNA_asset_types.h"
#include "DNA_userdef_types.h"

namespace blender {

/**
 * Get a reference to the global Blender project.
 *
 * As a general rule, the project's mutex should be held while accessing this to
 * prevent data races. The public APIs `BKE_blender_project_read_callback()` and
 * `BKE_blender_project_write_callback()` enforce this (if not abused) and should be
 * used where possible.
 *
 * \see get_project_mutex()
 *
 * \see BKE_blender_project_read_callback()
 *
 * \see BKE_blender_project_write_callback()
 */
static std::optional<bke::BlenderProject> &get_project()
{
  /* Construct on First Use idiom. */
  static std::optional<bke::BlenderProject> project;

  return project;
}

/**
 * Get a reference to the global Blender project's mutex.
 *
 * \see get_project()
 */
static std::shared_mutex &get_project_mutex()
{
  /* Construct on First Use idiom. */
  static std::shared_mutex project_mutex;

  return project_mutex;
}

namespace bke {

ProjectVariable::ProjectVariable(StringRef name, ProjectVariableType type)
{
  /* This variable is a hack to get around IDProperties having no value-based APIs for property
   * creation, only pointer-based. We free this after copying its data to `this`. */
  IDProperty *tmp_property;

  /* Guard against invalid types, in case of enum casts from integers.
   * We could instead do this as the `default` case in the switch below, but
   * then we don't get nice compiler warnings if someone adds a type but
   * forgets to handle it here. */
  BLI_assert(ELEM(
      type, ProjectVariableType::INT, ProjectVariableType::FLOAT, ProjectVariableType::STRING));

  switch (type) {
    case ProjectVariableType::INT: {
      IDPropertyTemplate prop_template{0};
      prop_template.i = 0;
      tmp_property = IDP_New(eIDPropertyType::IDP_INT, &prop_template, name, eIDPropertyFlag(0));
      break;
    }

    case ProjectVariableType::FLOAT: {
      IDPropertyTemplate prop_template{0};
      prop_template.f = 0.0f;
      tmp_property = IDP_New(eIDPropertyType::IDP_FLOAT, &prop_template, name, eIDPropertyFlag(0));
      break;
    }

    case ProjectVariableType::STRING: {
      tmp_property = IDP_NewString("", name, eIDPropertyFlag(0));
      break;
    }
  }

  IDP_ui_data_ensure(tmp_property);
  tmp_property->ui_data->description = BLI_strdup("");

  this->prop() = *tmp_property;

  /* Shallow delete, so the pointers copied to `this` remain valid. */
  MEM_delete_void(static_cast<void *>(tmp_property));
}

ProjectVariable::~ProjectVariable()
{
  IDP_ClearProperty(this);
}

ProjectVariable::ProjectVariable(ProjectVariable &&other) noexcept
{
  this->prop() = other.prop();
  other.prop() = {};
}

ProjectVariable &ProjectVariable::operator=(ProjectVariable &&other) noexcept
{
  if (this != &other) {
    IDP_ClearProperty(this);
    this->prop() = other.prop();
    other.prop() = {};
  }
  return *this;
}

StringRefNull ProjectVariable::name_get() const
{
  return StringRefNull(this->name);
}

ProjectVariableType ProjectVariable::type_get() const
{
  return ProjectVariableType(this->type);
}

ProjectVariableStringSubtype ProjectVariable::string_subtype_get() const
{
  return ProjectVariableStringSubtype(this->ui_data->rna_subtype);
}

void ProjectVariable::string_subtype_set(ProjectVariableStringSubtype type)
{
  this->ui_data->rna_subtype = PropertySubType(type);
}

StringRefNull ProjectVariable::value_string_get() const
{
  return StringRefNull(IDP_string_get(this));
}

int ProjectVariable::value_int_get() const
{
  return IDP_int_get(this);
}

float ProjectVariable::value_float_get() const
{
  return IDP_float_get(this);
}

void ProjectVariable::value_set(StringRefNull value)
{
  IDP_AssignString(this, value.c_str());
}

void ProjectVariable::value_set(int value)
{
  IDP_int_set(this, value);
}

void ProjectVariable::value_set(float value)
{
  IDP_float_set(this, value);
}

StringRefNull ProjectVariable::description_get() const
{
  return StringRefNull(this->ui_data->description);
}

void ProjectVariable::description_set(StringRef description)
{
  MEM_delete(this->ui_data->description);
  this->ui_data->description = MEM_new_array<char>(description.size() + 1,
                                                   "ProjectVariable description");
  description.copy_unsafe(this->ui_data->description);
}

void BlenderProject::set_name(StringRef name)
{
  BLI_assert(!name.is_empty());

  this->name_ = name;

  this->is_dirty = true;
}

void BlenderProject::set_root_path(StringRef root_path)
{
  BLI_assert(!root_path.is_empty());

  this->root_path_ = root_path;

  this->is_dirty = true;
}

StringRefNull BlenderProject::get_name() const
{
  return StringRefNull(this->name_);
}

StringRefNull BlenderProject::get_root_path() const
{
  return StringRefNull(this->root_path_);
}

int BlenderProject::find_variable_index(ProjectVariable *var)
{
  BLI_assert(var != nullptr);
  if (var == nullptr) {
    return -1;
  }

  for (int i : this->variables.index_range()) {
    if (this->variables[i].get() == var) {
      return i;
    }
  }

  return -1;
}

ProjectVariable *BlenderProject::new_variable(StringRef name, ProjectVariableType type)
{
  const std::string new_name = ensure_is_valid_project_variable_name(name);

  this->variables.append(std::make_unique<ProjectVariable>("", type));
  this->rename_variable(this->variables.size() - 1, new_name); /* Also ensures name uniqueness. */

  this->is_dirty = true;

  return this->variables.last().get();
}

int BlenderProject::remove_variable(ProjectVariable *var)
{
  const int index = this->find_variable_index(var);
  if (index == -1) {
    return -1;
  }

  this->variables.remove(index);

  if (this->active_variable_index > index) {
    this->active_variable_index -= 1;
  }
  this->active_variable_index = std::min(this->active_variable_index,
                                         int(this->variables.size() - 1));

  this->is_dirty = true;
  return index;
}

void BlenderProject::move_variable(int from_index, int to_index)
{
  BLI_assert(from_index < this->variables.size());
  BLI_assert(to_index < this->variables.size());

  /* No-op. */
  if (from_index == to_index) {
    return;
  }

  if (from_index < to_index) {
    std::rotate(this->variables.data() + from_index,
                this->variables.data() + from_index + 1,
                this->variables.data() + to_index + 1);
  }
  else if (from_index > to_index) {
    std::rotate(this->variables.data() + to_index,
                this->variables.data() + from_index,
                this->variables.data() + from_index + 1);
  }

  this->is_dirty = true;
}

void BlenderProject::rename_variable(int variable_index, StringRef name)
{
  BLI_assert(variable_index >= 0 && variable_index < this->variables.size());
  if (variable_index < 0 || variable_index >= this->variables.size()) {
    return;
  }

  const std::string new_name = ensure_is_valid_project_variable_name(name);

  auto is_variable_name_used = [&](StringRef name) -> bool {
    for (int i = 0; i < this->variables.size(); i++) {
      if (i == variable_index) {
        /* Skip the variable we're renaming. */
        continue;
      }

      if (this->variables[i]->name_get() == name) {
        return true;
      }
    }

    return false;
  };

  this->variables[variable_index]->prop().name[0] = '\0';
  BLI_uniquename_cb(is_variable_name_used,
                    new_name.c_str(),
                    '_',
                    this->variables[variable_index]->prop().name,
                    sizeof(IDProperty::name));
}

void with_blender_project_read_lock(FunctionRef<void()> lambda)
{
  std::shared_lock<std::shared_mutex> lock(get_project_mutex());
  lambda();
}

void with_blender_project_write_lock(FunctionRef<void()> lambda)
{
  std::unique_lock<std::shared_mutex> lock(get_project_mutex());
  lambda();
}

bool is_valid_project_variable_name(StringRef name)
{
  return name == ensure_is_valid_project_variable_name(name);
}

std::string ensure_is_valid_project_variable_name(StringRef name)
{
  std::string new_name(name);

  /* Shouldn't be empty. */
  if (new_name.size() == 0) {
    new_name.push_back('_');
  }

  /* Shouldn't start with a numerical digit. */
  if (new_name[0] >= '0' && new_name[0] <= '9') {
    new_name[0] = '_';
  }

  /* All characters should be alphanumeric or underscore.
   *
   * Note: this is utf8-safe because all non-ascii characters get substituted,
   * and thus any multi-byte code points just end up as multiple underscores
   * rather than being e.g. only partially substituted. This perhaps isn't a
   * pleasing result in some cases, but the string always ends up being valid
   * utf8.
   */
  for (int i = 0; i < new_name.size(); i++) {
    const char c = new_name[i];
    const bool is_valid_identifier_char = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                          (c >= '0' && c <= '9') || c == '_';
    if (!is_valid_identifier_char) {
      new_name[i] = '_';
    }
  }

  return new_name;
}

}  // namespace bke

bke::BlenderProject *BKE_blender_project_get(const Main *bmain)
{
  if (bmain == nullptr || !bmain->is_part_of_project) {
    return nullptr;
  }

  std::optional<bke::BlenderProject> &project = get_project();
  if (!project.has_value()) {
    return nullptr;
  }

  return &*project;
}

bool BKE_blender_project_init(blender::StringRef name, blender::StringRef root_path)
{
  if (name.is_empty() || root_path.is_empty()) {
    return false;
  }

  BKE_blender_project_clear();

  bke::with_blender_project_write_lock([&] {
    std::optional<bke::BlenderProject> &project = get_project();

    project = blender::bke::BlenderProject();

    project->set_name(name);
    project->set_root_path(root_path);
  });

  return true;
}

void BKE_blender_project_clear()
{
  bke::with_blender_project_write_lock([&] {
    std::optional<bke::BlenderProject> &project = get_project();

    for (auto &user_library : U.asset_libraries.items_mutable()) {
      if ((user_library.flag & ASSET_LIBRARY_PROJECT_DEFINED)) {
        /* Usually we also poke some UI code when removing asset libraries.
         * However this should be fine as the UI should be refreshed when
         * we are in the process of clearing out the project data.
         */
        BKE_preferences_asset_library_remove(&U, &user_library);
      }
    }

    project = std::nullopt;
  });
}

}  // namespace blender
