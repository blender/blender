/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

static int blf_load_font_default(const char *filename, const bool unique)
{
  const char *dir = BKE_appdir_folder_id(BLENDER_DATAFILES, BLF_DATAFILES_FONTS_DIR);
  if (dir == nullptr) {
    fprintf(stderr,
            "%s: 'fonts' data path not found for '%s', will not be able to display text\n",
            __func__,
            filename);
    return -1;
  }

  char filepath[FILE_MAX];
  BLI_path_join(filepath, sizeof(filepath), dir, filename);

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

static void blf_load_datafiles_dir()
{
  const char *datafiles_fonts_dir = BLF_DATAFILES_FONTS_DIR SEP_STR;
  const char *path = BKE_appdir_folder_id(BLENDER_DATAFILES, datafiles_fonts_dir);
  if (UNLIKELY(!path)) {
    fprintf(stderr, "Font data directory \"%s\" could not be detected!\n", datafiles_fonts_dir);
    return;
  }
  if (UNLIKELY(!BLI_exists(path))) {
    fprintf(stderr, "Font data directory \"%s\" does not exist!\n", path);
    return;
  }

  direntry *file_list;
  uint file_list_num = BLI_filelist_dir_contents(path, &file_list);
  for (int i = 0; i < file_list_num; i++) {
    if (S_ISDIR(file_list[i].s.st_mode)) {
      continue;
    }

    const char *filepath = file_list[i].path;
    if (!BLI_path_extension_check_n(
            filepath, ".ttf", ".ttc", ".otf", ".otc", ".woff", ".woff2", nullptr))
    {
      continue;
    }
    if (BLF_is_loaded(filepath)) {
      continue;
    }

    /* Attempt to load the font. */
    int font_id = BLF_load(filepath);
    if (font_id == -1) {
      fprintf(stderr, "Unable to load font: %s\n", filepath);
      continue;
    }

    BLF_enable(font_id, BLF_DEFAULT);
  }
  BLI_filelist_free(file_list, file_list_num);
}

void BLF_load_font_stack()
{
  /* Load these if not already, might have been replaced by user custom. */
  BLF_load_default(false);
  BLF_load_mono_default(false);
  blf_load_datafiles_dir();
}
