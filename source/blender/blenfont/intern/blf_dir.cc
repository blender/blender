/* SPDX-FileCopyrightText: 2009 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blf
 *
 * Manage search paths for font files.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <ft2build.h>

#include FT_FREETYPE_H
#include FT_GLYPH_H

#include "MEM_guardedalloc.h"

#include "DNA_vec_types.h"

#include "BLI_fileops.h"
#include "BLI_string.h"

#include "BLF_api.h"
#include "blf_internal.h"

char *blf_dir_metrics_search(const char *filepath)
{
  char *mfile;
  char *s;

  mfile = BLI_strdup(filepath);
  s = strrchr(mfile, '.');
  if (s) {
    if (BLI_strnlen(s, 4) < 4) {
      MEM_freeN(mfile);
      return nullptr;
    }
    s++;
    s[0] = 'a';
    s[1] = 'f';
    s[2] = 'm';

    /* First check `.afm`. */
    if (BLI_exists(mfile)) {
      return mfile;
    }

    /* And now check `.pfm`. */
    s[0] = 'p';

    if (BLI_exists(mfile)) {
      return mfile;
    }
  }
  MEM_freeN(mfile);
  return nullptr;
}
