/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once

#ifndef WIN32
#  error WIN32 only!
#endif  // WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "GHOST_SystemPaths.hh"

/**
 * WIN32 Implementation of GHOST_SystemPaths class.
 * \see GHOST_SystemPaths.
 */
class GHOST_SystemPathsWin32 : public GHOST_SystemPaths {
 public:
  /**
   * Constructor.
   */
  GHOST_SystemPathsWin32();

  /**
   * Destructor.
   */
  ~GHOST_SystemPathsWin32();

  /**
   * Determine the base dir in which shared resources are located. It will first try to use
   * "unpack and run" path, then look for properly installed path, including versioning.
   * \return Unsigned char string pointing to system dir (eg /usr/share/).
   */
  const char *getSystemDir(int version, const char *versionstr) const;

  /**
   * Determine the base dir in which user configuration is stored, including versioning.
   * If needed, it will create the base directory.
   * \return Unsigned char string pointing to user dir (eg ~/).
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
