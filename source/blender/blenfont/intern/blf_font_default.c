/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2011 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup blf
 *
 * API for loading default font files.
 */

#include <stdio.h>

#include "BLF_api.h"

#include "BLI_fileops.h"
#include "BLI_path_util.h"

#include "BKE_appdir.h"

static int blf_load_font_default(const char *filename, const bool unique)
{
  const char *dir = BKE_appdir_folder_id(BLENDER_DATAFILES, BLF_DATAFILES_FONTS_DIR);
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
  int font_id = blf_load_font_default(BLF_DEFAULT_PROPORTIONAL_FONT, unique);
  BLF_enable(font_id, BLF_DEFAULT);
  return font_id;
}

int BLF_load_mono_default(const bool unique)
{
  int font_id = blf_load_font_default(BLF_DEFAULT_MONOSPACED_FONT, unique);
  BLF_enable(font_id, BLF_MONOSPACED | BLF_DEFAULT);
  return font_id;
}

void BLF_load_font_stack()
{
  /* Load these if not already, might have been replaced by user custom. */
  BLF_load_default(false);
  BLF_load_mono_default(false);

  const char *path = BKE_appdir_folder_id(BLENDER_DATAFILES, BLF_DATAFILES_FONTS_DIR SEP_STR);
  if (path && BLI_exists(path)) {
    struct direntry *dir;
    uint num_files = BLI_filelist_dir_contents(path, &dir);
    for (int f = 0; f < num_files; f++) {
      if (!FILENAME_IS_CURRPAR(dir[f].relname) && !BLI_is_dir(dir[f].path)) {
        if (!BLF_is_loaded(dir[f].path)) {
          int font_id = BLF_load(dir[f].path);
          if (font_id == -1) {
            fprintf(stderr, "Unable to load font: %s\n", dir[f].path);
          }
          else {
            BLF_enable(font_id, BLF_DEFAULT);
            /* TODO: FontBLF will later load FT_Face on demand.  When this is in
             * place we can drop this face now since we have all needed data. */
          }
        }
      }
    }
    BLI_filelist_free(dir, num_files);
  }
  else {
    fprintf(stderr, "Fonts not found at %s\n", path);
  }
}
