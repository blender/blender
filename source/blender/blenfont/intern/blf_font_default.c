/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2011 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup blf
 *
 * API for loading default font files.
 */

#include <stdio.h>

#include "BLF_api.h"

#include "BLI_path_util.h"

#include "BKE_appdir.h"

static int blf_load_font_default(const char *filename, const bool unique)
{
  const char *dir = BKE_appdir_folder_id(BLENDER_DATAFILES, "fonts");
  if (dir == NULL) {
    fprintf(stderr,
            "%s: 'fonts' data path not found for '%s', will not be able to display text\n",
            __func__,
            filename);
    return -1;
  }

  char filepath[FILE_MAX];
  BLI_join_dirfile(filepath, sizeof(filepath), dir, filename);

  return (unique) ? BLF_load_unique(filepath) : BLF_load(filepath);
}

int BLF_load_default(const bool unique)
{
  return blf_load_font_default("droidsans.ttf", unique);
}

int BLF_load_mono_default(const bool unique)
{
  return blf_load_font_default("bmonofont-i18n.ttf", unique);
}
