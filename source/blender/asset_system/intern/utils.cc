/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "utils.hh"

namespace blender::asset_system::utils {

std::string normalize_directory_path(StringRef directory)
{
  if (directory.is_empty()) {
    return "";
  }

  char dir_normalized[PATH_MAX];
  BLI_strncpy(dir_normalized,
              directory.data(),
              /* + 1 for null terminator. */
              std::min(directory.size() + 1, int64_t(sizeof(dir_normalized))));
  BLI_path_normalize_dir(nullptr, dir_normalized, sizeof(dir_normalized));
  return std::string(dir_normalized);
}

/**
 * Normalize the given `path` (remove 'parent directory' and double-slashes element etc., and
 * convert to native path separators).
 *
 * If `max_len` is not default `std::string::npos` value, only the first part of the given string
 * up to `max_len` is processed, the rest remains unchanged. Needed to avoid modifying ID name part
 * of linked data paths. */
std::string normalize_path(std::string path, size_t max_len)
{
  max_len = (max_len == std::string::npos) ? size_t(path.size()) :
                                             std::min(max_len, size_t(path.size()));
  char *buf = BLI_strdupn(path.c_str(), max_len);
  BLI_path_slash_native(buf);
  BLI_path_normalize(nullptr, buf);

  std::string normalized_path = buf;
  MEM_freeN(buf);

  if (max_len != size_t(path.size())) {
    normalized_path = normalized_path + path.substr(int64_t(max_len));
  }

  return normalized_path;
}

}  // namespace blender::asset_system::utils
