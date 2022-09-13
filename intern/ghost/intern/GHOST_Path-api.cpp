/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2010 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup GHOST
 */

#include <cstdio>

#include "GHOST_ISystemPaths.h"
#include "GHOST_Path-api.h"
#include "GHOST_Types.h"
#include "intern/GHOST_Debug.h"

GHOST_TSuccess GHOST_CreateSystemPaths(void)
{
  return GHOST_ISystemPaths::create();
}

GHOST_TSuccess GHOST_DisposeSystemPaths(void)
{
  return GHOST_ISystemPaths::dispose();
}

const char *GHOST_getSystemDir(int version, const char *versionstr)
{
  GHOST_ISystemPaths *systemPaths = GHOST_ISystemPaths::get();
  return systemPaths ? systemPaths->getSystemDir(version, versionstr) : nullptr;
}

const char *GHOST_getUserDir(int version, const char *versionstr)
{
  GHOST_ISystemPaths *systemPaths = GHOST_ISystemPaths::get();
  /* Shouldn't be `nullptr`. */
  return systemPaths ? systemPaths->getUserDir(version, versionstr) : nullptr;
}

const char *GHOST_getUserSpecialDir(GHOST_TUserSpecialDirTypes type)
{
  GHOST_ISystemPaths *systemPaths = GHOST_ISystemPaths::get();
  /* Shouldn't be `nullptr`. */
  return systemPaths ? systemPaths->getUserSpecialDir(type) : nullptr;
}

const char *GHOST_getBinaryDir()
{
  GHOST_ISystemPaths *systemPaths = GHOST_ISystemPaths::get();
  /* Shouldn't be `nullptr`. */
  return systemPaths ? systemPaths->getBinaryDir() : nullptr;
}

void GHOST_addToSystemRecentFiles(const char *filepath)
{
  GHOST_ISystemPaths *systemPaths = GHOST_ISystemPaths::get();
  if (systemPaths) {
    systemPaths->addToSystemRecentFiles(filepath);
  }
}
