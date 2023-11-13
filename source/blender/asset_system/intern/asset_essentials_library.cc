/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include "BLI_path_util.h"

#include "BKE_appdir.h"

#include "AS_essentials_library.hh"

namespace blender::asset_system {

StringRefNull essentials_directory_path()
{
  static std::string path = []() {
    const char *datafiles_path = BKE_appdir_folder_id(BLENDER_DATAFILES, "assets");
    if (datafiles_path == nullptr) {
      return "";
    }
    return datafiles_path;
  }();
  return path;
}

}  // namespace blender::asset_system
