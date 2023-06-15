/* SPDX-FileCopyrightText: 2010 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once

#include "../GHOST_Types.h"
#include "GHOST_SystemPaths.hh"

class GHOST_SystemPathsUnix : public GHOST_SystemPaths {
 public:
  /**
   * Constructor
   * this class should only be instantiated by GHOST_ISystem.
   */
  GHOST_SystemPathsUnix();

  /**
   * Destructor.
   */
  ~GHOST_SystemPathsUnix();

  /**
   * Determine the base dir in which shared resources are located. It will first try to use
   * "unpack and run" path, then look for properly installed path, including versioning.
   * \return Unsigned char string pointing to system dir (eg `/usr/share/blender/`).
   */
  const char *getSystemDir(int version, const char *versionstr) const;

  /**
   * Determine the base dir in which user configuration is stored, including versioning.
   * If needed, it will create the base directory.
   * \return Unsigned char string pointing to user dir (eg `~/.config/.blender/`).
   */
  const char *getUserDir(int version, const char *versionstr) const;

  /**
   * Determine a special ("well known") and easy to reach user directory.
   * \return Unsigned char string pointing to user dir (eg `~/Documents/`).
   */
  const char *getUserSpecialDir(GHOST_TUserSpecialDirTypes type) const;

  /**
   * Determine the directory of the current binary
   * \return Unsigned char string pointing to the binary dir
   */
  const char *getBinaryDir() const;

  /**
   * Add the file to the operating system most recently used files
   */
  void addToSystemRecentFiles(const char *filepath) const;
};
