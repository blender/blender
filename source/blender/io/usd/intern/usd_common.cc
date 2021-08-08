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
 *
 * The Original Code is Copyright (C) 2021 Blender Foundation.
 * All rights reserved.
 */

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
