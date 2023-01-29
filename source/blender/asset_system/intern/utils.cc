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

}  // namespace blender::asset_system::utils
