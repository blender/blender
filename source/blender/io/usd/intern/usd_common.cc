/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_common.h"

#include <pxr/base/plug/registry.h>

#include "BKE_appdir.h"

namespace blender::io::usd {

void ensure_usd_plugin_path_registered()
{
  /* If #PXR_PYTHON_SUPPORT_ENABLED is defined, we *must* be dynamic and
   * the plugins are placed relative to the USD shared library hence no
   * hinting is required. */
#ifndef PXR_PYTHON_SUPPORT_ENABLED
  static bool plugin_path_registered = false;
  if (plugin_path_registered) {
    return;
  }
  plugin_path_registered = true;

  /* Tell USD which directory to search for its JSON files. If 'datafiles/usd'
   * does not exist, the USD library will not be able to read or write any files. */
  const char *blender_usd_datafiles = BKE_appdir_folder_id(BLENDER_DATAFILES, "usd");
  if (blender_usd_datafiles) {
    const std::string blender_usd_data_folder = blender_usd_datafiles;
    /* The trailing slash indicates to the USD library that the path is a directory. */
    pxr::PlugRegistry::GetInstance().RegisterPlugins(blender_usd_data_folder + SEP_STR);
  }
#endif
}

}  // namespace blender::io::usd
