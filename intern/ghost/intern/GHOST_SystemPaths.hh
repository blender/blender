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
  /** \copydoc #GHOST_SystemPaths::getSystemDir */
  const char *getSystemDir(int version, const char *versionstr) const override = 0;
  /** \copydoc #GHOST_SystemPaths::getUserDir */
  const char *getUserDir(int version, const char *versionstr) const override = 0;
  /** \copydoc #GHOST_SystemPaths::getBinaryDir */
  const char *getBinaryDir() const override = 0;
  /** \copydoc #GHOST_SystemPaths::addToSystemRecentFiles */
  void addToSystemRecentFiles(const char *filepath) const override = 0;
};
