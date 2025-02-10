/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once

#include "GHOST_ISystemPaths.hh"

class GHOST_SystemPaths : public GHOST_ISystemPaths {
 protected:
  /**
   * Constructor.
   * Protected default constructor to force use of static createSystem member.
   */
  GHOST_SystemPaths() {}

  /**
   * Destructor.
   * Protected default constructor to force use of static dispose member.
   */
  ~GHOST_SystemPaths() override = default;

 public:
  /**
   * Determine the base directory in which shared resources are located. It will first try to use
   * "unpack and run" path, then look for properly installed path, including versioning.
   * \return Unsigned char string pointing to system directory (eg `/usr/share/blender/`).
   */
  const char *getSystemDir(int version, const char *versionstr) const override = 0;

  /**
   * Determine the base directory in which user configuration is stored, including versioning.
   * If needed, it will create the base directory.
   * \return Unsigned char string pointing to user directory (eg `~/.blender/`).
   */
  const char *getUserDir(int version, const char *versionstr) const override = 0;

  /**
   * Determine the directory of the current binary.
   * \return Unsigned char string pointing to the binary directory.
   */
  const char *getBinaryDir() const override = 0;

  /**
   * Add the file to the operating system most recently used files
   */
  void addToSystemRecentFiles(const char *filepath) const override = 0;
};
