/* SPDX-FileCopyrightText: 2010 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#include <cstdio>

#include "GHOST_ISystemPaths.hh"
#include "GHOST_Path-api.hh"
#include "GHOST_Types.h"
#include "intern/GHOST_Debug.hh"

GHOST_TSuccess GHOST_CreateSystemPaths()
{
  return GHOST_ISystemPaths::create();
}

GHOST_TSuccess GHOST_DisposeSystemPaths()
{
  return GHOST_ISystemPaths::dispose();
}

const char *GHOST_getSystemDir(int version, const char *versionstr)
{
  const GHOST_ISystemPaths *systemPaths = GHOST_ISystemPaths::get();
  return systemPaths ? systemPaths->getSystemDir(version, versionstr) : nullptr;
}

const char *GHOST_getUserDir(int version, const char *versionstr)
{
  const GHOST_ISystemPaths *systemPaths = GHOST_ISystemPaths::get();
  /* Shouldn't be `nullptr`. */
  return systemPaths ? systemPaths->getUserDir(version, versionstr) : nullptr;
}

const char *GHOST_getUserSpecialDir(GHOST_TUserSpecialDirTypes type)
{
  const GHOST_ISystemPaths *systemPaths = GHOST_ISystemPaths::get();
  /* Shouldn't be `nullptr`. */
  return systemPaths ? systemPaths->getUserSpecialDir(type) : nullptr;
}

const char *GHOST_getBinaryDir()
{
  const GHOST_ISystemPaths *systemPaths = GHOST_ISystemPaths::get();
  /* Shouldn't be `nullptr`. */
  return systemPaths ? systemPaths->getBinaryDir() : nullptr;
}

void GHOST_addToSystemRecentFiles(const char *filepath)
{
  const GHOST_ISystemPaths *systemPaths = GHOST_ISystemPaths::get();
  if (systemPaths) {
    systemPaths->addToSystemRecentFiles(filepath);
  }
}
