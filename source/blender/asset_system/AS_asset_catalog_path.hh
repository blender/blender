/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#pragma once

#include "BLI_function_ref.hh"
#include "BLI_string_ref.hh"
#include "BLI_sys_types.h"

#include <string>

namespace blender::asset_system {

/**
 * Location of an Asset Catalog in the catalog tree, denoted by slash-separated path components.
 *
 * Each path component is a string that is not allowed to have slashes or colons. The latter is to
 * make things easy to save in the colon-delimited Catalog Definition File format.
 *
 * The path of a catalog determines where in the catalog hierarchy the catalog is shown. Examples
 * are "Characters/Ellie/Poses/Hand" or "Kit_bash/City/Skyscrapers". The path looks like a
 * file-system path, with a few differences:
 *
 * - Only slashes are used as path component separators.
 * - All paths are absolute, so there is no need for a leading slash.
 *
 * See https://wiki.blender.org/wiki/Source/Architecture/Asset_System/Catalogs
 *
 * Paths are stored as byte sequences, and assumed to be UTF-8.
 */
class AssetCatalogPath {
  friend std::ostream &operator<<(std::ostream &stream, const AssetCatalogPath &path_to_append);

  /**
   * The path itself, such as "Agents/Secret/327".
   */
  std::string path_ = "";

 public:
  static const char SEPARATOR;

  AssetCatalogPath() = default;
  AssetCatalogPath(StringRef path);
  AssetCatalogPath(std::string path);
  AssetCatalogPath(const char *path);
  AssetCatalogPath(const AssetCatalogPath &other_path) = default;
  AssetCatalogPath(AssetCatalogPath &&other_path) noexcept;
  ~AssetCatalogPath() = default;

  uint64_t hash() const;
  uint64_t length() const; /* Length of the path in bytes. */

  /** C-string representation of the path. */
  const char *c_str() const;
  const std::string &str() const;

  /* The last path component, used as label in the tree view. */
  StringRefNull name() const;

  /* In-class operators, because of the implicit `AssetCatalogPath(StringRef)` constructor.
   * Otherwise `string == string` could cast both sides to `AssetCatalogPath`. */
  bool operator==(const AssetCatalogPath &other_path) const;
  bool operator!=(const AssetCatalogPath &other_path) const;
  bool operator<(const AssetCatalogPath &other_path) const;
  AssetCatalogPath &operator=(const AssetCatalogPath &other_path) = default;
  AssetCatalogPath &operator=(AssetCatalogPath &&other_path) = default;

  /** Concatenate two paths, returning the new path. */
  AssetCatalogPath operator/(const AssetCatalogPath &path_to_append) const;

  /* False when the path is empty, true otherwise. */
  operator bool() const;

  /**
   * Clean up the path. This ensures:
   * - Every path component is stripped of its leading/trailing spaces.
   * - Empty components (caused by double slashes or leading/trailing slashes) are removed.
   * - Invalid characters are replaced with valid ones.
   */
  [[nodiscard]] AssetCatalogPath cleanup() const;

  /**
   * \return true only if the given path is a parent of this catalog's path.
   * When this catalog's path is equal to the given path, return true as well.
   * In other words, this defines a weak subset.
   *
   * True: "some/path/there" is contained in "some/path" and "some".
   * False: "path/there" is not contained in "some/path/there".
   *
   * Note that non-cleaned-up paths (so for example starting or ending with a
   * slash) are not supported, and result in undefined behavior.
   */
  bool is_contained_in(const AssetCatalogPath &other_path) const;

  /**
   * \return the parent path, or an empty path if there is no parent.
   */
  AssetCatalogPath parent() const;

  /**
   * Change the initial part of the path from `from_path` to `to_path`.
   * If this path does not start with `from_path`, return an empty path as result.
   *
   * Example:
   *
   * AssetCatalogPath path("some/path/to/some/catalog");
   * path.rebase("some/path", "new/base") -> "new/base/to/some/catalog"
   */
  AssetCatalogPath rebase(const AssetCatalogPath &from_path,
                          const AssetCatalogPath &to_path) const;

  /** Call the callback function for each path component, in left-to-right order. */
  using ComponentIteratorFn = FunctionRef<void(StringRef component_name, bool is_last_component)>;
  void iterate_components(ComponentIteratorFn callback) const;

 protected:
  /** Strip leading/trailing spaces and replace disallowed characters. */
  static std::string cleanup_component(StringRef component_name);
};

/** Output the path as string. */
std::ostream &operator<<(std::ostream &stream, const AssetCatalogPath &path_to_append);

}  // namespace blender::asset_system
