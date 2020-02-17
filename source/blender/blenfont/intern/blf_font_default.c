/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 */

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
