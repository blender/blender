/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>

#include "BLI_function_ref.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "DNA_ID.h"

#include "BKE_idprop.hh"

namespace blender {

struct Main;

namespace bke {

/**
 * The main data type of a project variable.
 *
 * We intentionally re-use the values from `eIDPropertyType` to make translation
 * between the two easy.
 */
enum class ProjectVariableType : char {
  STRING = eIDPropertyType::IDP_STRING,
  INT = eIDPropertyType::IDP_INT,
  FLOAT = eIDPropertyType::IDP_FLOAT,
};

/**
 * The subtype of a string project variable, indicating how the data should be
 * interpreted.
 *
 * We intentionally re-use the values from `PropertySubType` to make translation
 * between the two easy.
 */
enum class ProjectVariableStringSubtype {
  /* These should be kept in sync with their counterparts in PropertySubType
   * from RNA_types.hh. We don't include that header here because some places
   * that include *this* header don't have access to it. */
  NONE = 0,     /* PropertySubType::PROP_NONE */
  FILEPATH = 1, /* PropertySubType::PROP_FILEPATH */
};

/**
 * A variable in a project.
 *
 * A project variable is a named value (like a string or integer)
 * that is shared across all blend files in a project.
 */
class ProjectVariable : IDProperty {
 public:
  ProjectVariable(StringRef name, ProjectVariableType type);

  ~ProjectVariable();
  ProjectVariable(const ProjectVariable &other) = delete;
  ProjectVariable &operator=(const ProjectVariable &other) = delete;
  ProjectVariable(ProjectVariable &&other) noexcept;
  ProjectVariable &operator=(ProjectVariable &&other) noexcept;

  /**
   * Get the underlying ID property of the variable.
   *
   * NOTE: typically you shouldn't use this, especially outside of this source
   * file.
   *
   * We inherit privately from `IDProperty` because we're only using a specific
   * subset of what it does, and we don't want people accidentally putting it
   * in invalid states for this type. This method bypasses those protections,
   * and should only be used in narrow specific cases.
   */
  IDProperty &prop()
  {
    return *this;
  }
  const IDProperty &prop() const
  {
    return *this;
  }

  /**
   * Get the variable's name.
   *
   * Note that renaming a variable requires access to project data (to ensure
   * name uniqueness), and therefore variable renaming is done via
   * `rename_variable()` on `Project` rather than by a method on the variable
   * itself.
   *
   * \see Project::rename_variable()
   */
  StringRefNull name_get() const;

  /**
   * Get the variable's type.
   *
   * Note that the variable's type is determined at construction time,
   * and cannot be changed after.
   *
   * \see Project::new_variable()
   */
  ProjectVariableType type_get() const;

  ProjectVariableStringSubtype string_subtype_get() const;
  void string_subtype_set(ProjectVariableStringSubtype type);

  StringRefNull value_string_get() const;
  int value_int_get() const;
  float value_float_get() const;

  void value_set(StringRefNull value);
  void value_set(int value);
  void value_set(float value);

  StringRefNull description_get() const;
  void description_set(StringRef description);
};

/**
 * A Blender project.
 *
 * There is at most one active project at a time in Blender.
 */
class BlenderProject {
  /** The project name. Should never be empty. */
  std::string name_;

  /**
   * The project root path. Should never be empty.
   *
   * This should generally be an absolute path to a directory that exists, is
   * accessible, and contains a ".blender_project" directory with the project's
   * config in it. This is not, however, guaranteed because via Python a project
   * can be initialized with an arbitrary path, or the file-system could have
   * been modified since the project was loaded, etc.
   */
  std::string root_path_;

 public:
  Vector<std::unique_ptr<ProjectVariable>> variables;
  int active_variable_index = 0;
  /* The index of the selected project asset library in the UI. */
  int active_asset_library_index;

  /**
   * Whether the project has unsaved changes.
   *
   * Default initializes to `true` because a freshly constructed
   * `BlenderProject` is unsaved by definition.
   */
  bool is_dirty = true;

  /**
   * Set the project's name.
   *
   * Also marks the project as dirty.
   *
   * The passed `name` should never be empty (which is invalid).
   */
  void set_name(StringRef name);

  /**
   * Set the project's root path.
   *
   * Also marks the project as dirty.
   *
   * The passed `root_path` should never be empty (which is invalid).
   */
  void set_root_path(StringRef root_path);

  StringRefNull get_name() const;
  StringRefNull get_root_path() const;

  /**
   * Get the array index of the given variable.
   *
   * \return If the variable is found, the index of the variable.  If not found, -1.
   */
  int find_variable_index(ProjectVariable *var);

  /**
   * Create a new variable of the given name and type.
   *
   * If `name` does not adhere to naming requirements, it is automatically
   * altered to meet them by substituting invalid characters. `name` will also
   * be modified if needed to make it unique among the current variables.
   */
  ProjectVariable *new_variable(StringRef name, ProjectVariableType type);

  /**
   * Remove the given variable.
   *
   * \returns The index that the removed variable had, or -1 if the variable
   * wasn't found.
   */
  int remove_variable(ProjectVariable *var);

  /**
   * Move the variable at from_index to to_index.
   *
   * The other variables are shifted appropriately around this.
   *
   * Both from_index and to_index must be valid indices in the list.
   */
  void move_variable(int from_index, int to_index);

  /**
   * Rename the variable at the given index.
   *
   * If `name` does not adhere to naming requirements, it is automatically
   * altered to meet them by substituting invalid characters. `name` will also
   * be modified if needed to make it unique among the current variables.
   */
  void rename_variable(int variable_index, StringRef name);
};

/**
 * Run the given lambda with the global project mutex locked for reading.
 *
 * NOTE: you should avoid using this function directly, except in RNA code where
 * the project pointer is already directly provided. Prefer using
 * `BKE_blender_project_read_callback()`, which fetches the appropriate project for a
 * given `Main`.
 *
 * \see BKE_blender_project_read_callback()
 */
void with_blender_project_read_lock(FunctionRef<void()> lambda);

/**
 * Run the given lambda with the global project mutex locked for writing.
 *
 * NOTE: you should avoid using this function directly, except in RNA code where
 * the project pointer is already directly provided. Prefer using
 * `BKE_blender_project_write_callback()`, which fetches the appropriate project for
 * a given `Main`.
 *
 * \see BKE_blender_project_read_callback()
 */
void with_blender_project_write_lock(FunctionRef<void()> lambda);

/**
 * Return whether the given string is a valid project variable identifier or not.
 *
 * For the moment we are very restrictive: only alphanumeric characters and underscores are
 * allowed, and the first character must not be a digit. This is very similar to the identifier
 * rules in some programming languages.
 *
 * In the future we should likely expand this to allow more of unicode. But better to start
 * restrictive and open up later than start super open and discover we need to make a breaking
 * change by making it more restrictive.
 */
bool is_valid_project_variable_name(StringRef name);

/**
 * Turn the given string into a valid project variable name.
 *
 * This is accomplished via simple substitution of non-allowed characters.
 *
 * The resulting string will pass `is_valid_project_variable_name()` above.
 *
 * \see is_valid_project_variable_name()
 *
 * \returns The new valid variable name.
 */
std::string ensure_is_valid_project_variable_name(StringRef name);

}  // namespace bke

/**
 * Fetch the current active Blender Project, if any.
 *
 * WARNING: this fetches the project without any synchronization for
 * multi-threading, so it is your responsibility to ensure thread safety. Prefer
 * using `BKE_blender_project_read_callback()` and `BKE_blender_project_write_callback()`,
 * which handle thread synchronization for you.
 *
 * \param bmain: The `Main` to return the active project for. At the moment,
 * there is just one global project. However, some temporary `Main`s should be
 * treated as not ever being in a project, in which case this will return
 * nullptr.
 *
 * \returns Either the current active project, or nullptr if there is no active
 * project or if the passed bmain is considered projectless.
 *
 * \see BKE_blender_project_read_callback()
 *
 * \see BKE_blender_project_write_callback()
 */
bke::BlenderProject *BKE_blender_project_get(const Main *bmain);

/**
 * Run the given lambda with read-only access to the active Blender Project, if
 * any.
 *
 * This follows the same project-fetching semantics as
 * `BKE_blender_project_get()`, but ensures thread safety by holding a shared
 * mutex lock while running the lambda with access to the fetched project.
 *
 * The lambda takes a single `const BlenderProject *` parameter, and may return
 * a value of any type (including `void` if none). The returned value (if any)
 * is passed through and returned by this function.
 *
 * NOTE: the lambda is run even if there is no project, in which case the lambda
 * receives a nullptr.
 *
 * \see BKE_blender_project_get()
 *
 * \see BKE_blender_project_write_callback()
 */
template<std::invocable<const bke::BlenderProject *> Fn>
inline auto BKE_blender_project_read_callback(const Main *bmain, Fn lambda)
{
  using T = std::invoke_result_t<Fn, const bke::BlenderProject *>;
  if constexpr (std::is_void_v<T>) {
    bke::with_blender_project_read_lock([&] {
      const bke::BlenderProject *project = BKE_blender_project_get(bmain);
      lambda(project);
    });
  }
  else {
    std::optional<T> result;
    bke::with_blender_project_read_lock([&] {
      const bke::BlenderProject *project = BKE_blender_project_get(bmain);
      result = lambda(project);
    });
    BLI_assert(result.has_value());
    return std::move(*result);
  }
}

/**
 * Run the given lambda with write access to the active Blender Project, if any.
 *
 * Same as `BKE_blender_project_read_callback()`, except that it takes an
 * exclusive mutex lock to provide write access to the project, and the lambda
 * in turn takes a non-const `BlenderProject *` parameter.
 *
 * If you only need to read from the project, use `BKE_blender_project_read_callback()`
 * instead of this to reduce thread contention.
 *
 * \see BKE_blender_project_get()
 *
 * \see BKE_blender_project_read_callback()
 */
template<std::invocable<bke::BlenderProject *> Fn>
inline auto BKE_blender_project_write_callback(const Main *bmain, Fn lambda)
{
  using T = std::invoke_result_t<Fn, bke::BlenderProject *>;
  if constexpr (std::is_void_v<T>) {
    bke::with_blender_project_write_lock([&] {
      bke::BlenderProject *project = BKE_blender_project_get(bmain);
      lambda(project);
    });
  }
  else {
    std::optional<T> result;
    bke::with_blender_project_write_lock([&] {
      bke::BlenderProject *project = BKE_blender_project_get(bmain);
      result = lambda(project);
    });
    BLI_assert(result.has_value());
    return std::move(*result);
  }
}

/**
 * Initialize a new active Blender Project.
 *
 * If either `name` or `root_path` are empty (which is invalid), the current
 * project (if any) will remain as-is and false is returned.  Otherwise the
 * existing project (if any) is cleared, the project is initialized with the
 * given values, and true is returned.
 *
 * This handles thread synchronization internally.
 */
bool BKE_blender_project_init(blender::StringRef name, blender::StringRef root_path);

/**
 * Clears and unloads the current active project, if any.
 *
 * This handles thread synchronization internally.
 */
void BKE_blender_project_clear();

}  // namespace blender
