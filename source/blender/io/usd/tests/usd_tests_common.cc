/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

#include "usd_tests_common.h"

#include "testing/testing.h"

#include <pxr/base/plug/registry.h>

#include "BLI_path_util.h"
#include "BLI_utildefines.h"

#include "BKE_appdir.h"

namespace blender::io::usd {

std::string register_usd_plugins_for_tests()
{
  static char usd_datafiles_dir[FILE_MAX] = {'\0'};
  static bool plugin_path_registered = false;
  if (plugin_path_registered) {
    return usd_datafiles_dir;
  }
  plugin_path_registered = true;

  const std::string &release_dir = blender::tests::flags_test_release_dir();
  if (release_dir.empty()) {
    return "";
  }

  const size_t path_len = BLI_path_join(
      usd_datafiles_dir, FILE_MAX, release_dir.c_str(), "datafiles", "usd");

  /* #BLI_path_join removes trailing slashes, but the USD library requires one in order to
   * recognize the path as directory. */
  BLI_assert(path_len + 1 < FILE_MAX);
  usd_datafiles_dir[path_len] = '/';
  usd_datafiles_dir[path_len + 1] = '\0';
  /* If #PXR_PYTHON_SUPPORT_ENABLED is defined, we *must* be dynamic and
   * the plugins are placed relative to the USD shared library hence no
   * hinting is required. */
#ifndef PXR_PYTHON_SUPPORT_ENABLED
  pxr::PlugRegistry::GetInstance().RegisterPlugins(usd_datafiles_dir);
#endif
  return usd_datafiles_dir;
}

}  // namespace blender::io::usd
