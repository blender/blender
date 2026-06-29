/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include <concepts>
#include <optional>

#include "BLI_function_ref.hh"
#include "BLI_string_ref.hh"

namespace blender {

struct Main;

namespace bke {

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
