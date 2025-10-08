/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */

/**
 * This file is a template to use as base when switching to a new version of Blender.
 *
 * DO NOT add this file to CMakeList.txt.
 *
 * When initializing a new version of Blender in main, after branching out the current one into
 * its release branch:
 * - Copy that file and rename it to the proper new version number (e.g. `versioning_510.cc`).
 * - Rename the two functions below by replacing the `xxx` with the matching new version number.
 * - Add the new file to CMakeList.txt
 * - Add matching calls in #do_versions_after_linking and #do_versions, in `readfile.cc` and update
 * declarations in`readfile.hh`.
 */

#define DNA_DEPRECATED_ALLOW

#include "DNA_ID.h"

#include "BLI_sys_types.h"

#include "BKE_main.hh"

#include "readfile.hh"

#include "versioning_common.hh"

// #include "CLG_log.h"
// static CLG_LogRef LOG = {"blend.doversion"};

void do_versions_after_linking_xxx(FileData * /*fd*/, Main * /*bmain*/)
{
  /**
   * Always bump subversion in BKE_blender_version.h when adding versioning
   * code here, and wrap it inside a MAIN_VERSION_FILE_ATLEAST check.
   *
   * \note Keep this message at the bottom of the function.
   */
}

void blo_do_versions_xxx(FileData * /*fd*/, Library * /*lib*/, Main * /*bmain*/)
{
  /**
   * Always bump subversion in BKE_blender_version.h when adding versioning
   * code here, and wrap it inside a MAIN_VERSION_FILE_ATLEAST check.
   *
   * \note Keep this message at the bottom of the function.
   */
}
