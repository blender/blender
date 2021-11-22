/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup bke
 */

#include "BKE_asset_catalog_path.hh"

#include "BLI_path_util.h"

namespace blender::bke {

const char AssetCatalogPath::SEPARATOR = '/';

AssetCatalogPath::AssetCatalogPath(const std::string &path) : path_(path)
{
}

AssetCatalogPath::AssetCatalogPath(StringRef path) : path_(path)
{
}

AssetCatalogPath::AssetCatalogPath(const char *path) : path_(path)
{
}

AssetCatalogPath::AssetCatalogPath(AssetCatalogPath &&other_path) noexcept
    : path_(std::move(other_path.path_))
{
}

uint64_t AssetCatalogPath::hash() const
{
  std::hash<std::string> hasher{};
  return hasher(this->path_);
}

uint64_t AssetCatalogPath::length() const
{
  return this->path_.length();
}

const char *AssetCatalogPath::c_str() const
{
  return this->path_.c_str();
}

const std::string &AssetCatalogPath::str() const
{
  return this->path_;
}

StringRefNull AssetCatalogPath::name() const
{
  const size_t last_sep_index = this->path_.rfind(SEPARATOR);
  if (last_sep_index == std::string::npos) {
    return StringRefNull(this->path_);
  }

  return StringRefNull(this->path_.c_str() + last_sep_index + 1);
}

/* In-class operators, because of the implicit `AssetCatalogPath(StringRef)` constructor.
 * Otherwise `string == string` could cast both sides to `AssetCatalogPath`. */
bool AssetCatalogPath::operator==(const AssetCatalogPath &other_path) const
{
  return this->path_ == other_path.path_;
}

bool AssetCatalogPath::operator!=(const AssetCatalogPath &other_path) const
{
  return !(*this == other_path);
}

bool AssetCatalogPath::operator<(const AssetCatalogPath &other_path) const
{
  return this->path_ < other_path.path_;
}

AssetCatalogPath AssetCatalogPath::operator/(const AssetCatalogPath &path_to_append) const
{
  /* `"" / "path"` or `"path" / ""` should just result in `"path"` */
  if (!*this) {
    return path_to_append;
  }
  if (!path_to_append) {
    return *this;
  }

  std::stringstream new_path;
  new_path << this->path_ << SEPARATOR << path_to_append.path_;
  return AssetCatalogPath(new_path.str());
}

AssetCatalogPath::operator bool() const
{
  return !this->path_.empty();
}

std::ostream &operator<<(std::ostream &stream, const AssetCatalogPath &path_to_append)
{
  stream << path_to_append.path_;
  return stream;
}

AssetCatalogPath AssetCatalogPath::cleanup() const
{
  std::stringstream clean_components;
  bool first_component_seen = false;

  this->iterate_components([&clean_components, &first_component_seen](StringRef component_name,
                                                                      bool /*is_last_component*/) {
    const std::string clean_component = cleanup_component(component_name);

    if (clean_component.empty()) {
      /* These are caused by leading, trailing, or double slashes. */
      return;
    }

    /* If a previous path component has been streamed already, we need a path separator. This
     * cannot use the `is_last_component` boolean, because the last component might be skipped due
     * to the condition above. */
    if (first_component_seen) {
      clean_components << SEPARATOR;
    }
    first_component_seen = true;

    clean_components << clean_component;
  });

  return AssetCatalogPath(clean_components.str());
}

std::string AssetCatalogPath::cleanup_component(StringRef component)
{
  std::string cleaned = component.trim();
  /* Replace colons with something else, as those are used in the CDF file as delimiter. */
  std::replace(cleaned.begin(), cleaned.end(), ':', '-');
  return cleaned;
}

bool AssetCatalogPath::is_contained_in(const AssetCatalogPath &other_path) const
{
  if (!other_path) {
    /* The empty path contains all other paths. */
    return true;
  }

  if (this->path_ == other_path.path_) {
    /* Weak is-in relation: equal paths contain each other. */
    return true;
  }

  /* To be a child path of 'other_path', our path must be at least a separator and another
   * character longer. */
  if (this->length() < other_path.length() + 2) {
    return false;
  }

  /* Create StringRef to be able to use .startswith(). */
  const StringRef this_path(this->path_);
  const bool prefix_ok = this_path.startswith(other_path.path_);
  const char next_char = this_path[other_path.length()];
  return prefix_ok && next_char == SEPARATOR;
}

AssetCatalogPath AssetCatalogPath::parent() const
{
  if (!*this) {
    return AssetCatalogPath("");
  }
  std::string::size_type last_sep_index = this->path_.rfind(SEPARATOR);
  if (last_sep_index == std::string::npos) {
    return AssetCatalogPath("");
  }
  return AssetCatalogPath(this->path_.substr(0, last_sep_index));
}

void AssetCatalogPath::iterate_components(ComponentIteratorFn callback) const
{
  const char *next_slash_ptr;

  for (const char *path_component = this->path_.data(); path_component && path_component[0];
       /* Jump to one after the next slash if there is any. */
       path_component = next_slash_ptr ? next_slash_ptr + 1 : nullptr) {
    /* Note that this also treats backslashes as component separators, which
     * helps in cleaning up backslash-separated paths. */
    next_slash_ptr = BLI_path_slash_find(path_component);

    const bool is_last_component = next_slash_ptr == nullptr;
    /* Note that this won't be null terminated. */
    const StringRef component_name = is_last_component ?
                                         path_component :
                                         StringRef(path_component,
                                                   next_slash_ptr - path_component);

    callback(component_name, is_last_component);
  }
}

AssetCatalogPath AssetCatalogPath::rebase(const AssetCatalogPath &from_path,
                                          const AssetCatalogPath &to_path) const
{
  if (!from_path) {
    if (!to_path) {
      return AssetCatalogPath("");
    }
    return to_path / *this;
  }

  if (!this->is_contained_in(from_path)) {
    return AssetCatalogPath("");
  }

  if (*this == from_path) {
    /* Early return, because otherwise the length+1 below is going to cause problems. */
    return to_path;
  }

  /* When from_path = "test", we need to skip "test/" to get the rest of the path, hence the +1. */
  const StringRef suffix = StringRef(this->path_).substr(from_path.length() + 1);
  const AssetCatalogPath path_suffix(suffix);
  return to_path / path_suffix;
}

}  // namespace blender::bke
