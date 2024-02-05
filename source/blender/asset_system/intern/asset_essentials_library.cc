/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include "BLI_path_util.h"

#include "BKE_appdir.hh"

#include "AS_essentials_library.hh"

namespace blender::asset_system {

StringRefNull essentials_directory_path()
{
  static std::string path = []() {
    const std::optional<std::string> datafiles_path = BKE_appdir_folder_id(BLENDER_DATAFILES,
                                                                           "assets");
    return datafiles_path.value_or("");
  }();
  return path;
}

}  // namespace blender::asset_system
