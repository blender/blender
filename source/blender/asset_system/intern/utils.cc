/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
  BLI_path_normalize_dir(dir_normalized, sizeof(dir_normalized));
  return std::string(dir_normalized);
}

std::string normalize_path(StringRefNull path, int64_t max_len)
{
  const int64_t len = (max_len == StringRef::not_found) ? path.size() :
                                                          std::min(max_len, path.size());

  char *buf = BLI_strdupn(path.c_str(), len);
  BLI_path_slash_native(buf);
  BLI_path_normalize(buf);

  std::string normalized_path = buf;
  MEM_freeN(buf);

  if (len != path.size()) {
    normalized_path = normalized_path + path.substr(len);
  }

  return normalized_path;
}

}  // namespace blender::asset_system::utils
