/* SPDX-FileCopyrightText: 2010 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once

#ifndef __APPLE__
#  error Apple OSX only!
#endif  // __APPLE__

#include <optional>
#include <string>

#include "GHOST_SystemPaths.hh"

class GHOST_SystemPathsCocoa : public GHOST_SystemPaths {
 public:
  /**
   * Constructor.
   */
  GHOST_SystemPathsCocoa() = default;

  /**
   * Destructor.
   */
  ~GHOST_SystemPathsCocoa() override = default;

  /**
   * Determine the base directory in which shared resources are located. It will first try to use
   * "unpack and run" path, then look for properly installed path, including versioning.
   * \return Unsigned char string pointing to system directory (eg `/usr/share/blender/`).
   */
  const char *getSystemDir(int version, const char *versionstr) const override;

  /**
   * Determine the base directory in which user configuration is stored, including versioning.
   * If needed, it will create the base directory.
   * \return Unsigned char string pointing to user directory (eg `~/.blender/`).
   */
  const char *getUserDir(int version, const char *versionstr) const override;

  /**
   * Determine a special ("well known") and easy to reach user directory.
   * \return If successfull, a string containing the user directory path (eg `~/Documents/`).
   */
  std::optional<std::string> getUserSpecialDir(GHOST_TUserSpecialDirTypes type) const override;

  /**
   * Determine the directory of the current binary.
   * \return Unsigned char string pointing to the binary directory.
   */
  const char *getBinaryDir() const override;

  /**
   * Add the file to the operating system most recently used files
   */
  void addToSystemRecentFiles(const char *filepath) const override;
};
