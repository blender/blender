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
  virtual ~GHOST_SystemPaths() {}

 public:
  /**
   * Determine the base dir in which shared resources are located. It will first try to use
   * "unpack and run" path, then look for properly installed path, including versioning.
   * \return Unsigned char string pointing to system dir (eg /usr/share/blender/).
   */
  virtual const char *getSystemDir(int version, const char *versionstr) const = 0;

  /**
   * Determine the base dir in which user configuration is stored, including versioning.
   * If needed, it will create the base directory.
   * \return Unsigned char string pointing to user dir (eg ~/.blender/).
   */
  virtual const char *getUserDir(int version, const char *versionstr) const = 0;

  /**
   * Determine the directory of the current binary
   * \return Unsigned char string pointing to the binary dir
   */
  virtual const char *getBinaryDir() const = 0;

  /**
   * Add the file to the operating system most recently used files
   */
  virtual void addToSystemRecentFiles(const char *filepath) const = 0;
};
