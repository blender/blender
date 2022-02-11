/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation. All rights reserved. */

#include "usd_common.h"

#include <pxr/base/plug/registry.h>

#include "BKE_appdir.h"

namespace blender::io::usd {

void ensure_usd_plugin_path_registered()
{
  static bool plugin_path_registered = false;
  if (plugin_path_registered) {
    return;
  }
  plugin_path_registered = true;

  /* Tell USD which directory to search for its JSON files. If 'datafiles/usd'
   * does not exist, the USD library will not be able to read or write any files. */
  const std::string blender_usd_datafiles = BKE_appdir_folder_id(BLENDER_DATAFILES, "usd");
  /* The trailing slash indicates to the USD library that the path is a directory. */
  pxr::PlugRegistry::GetInstance().RegisterPlugins(blender_usd_datafiles + "/");
}

}  // namespace blender::io::usd
