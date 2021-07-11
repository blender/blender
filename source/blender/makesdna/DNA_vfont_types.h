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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup DNA
 *
 * Vector Fonts used for text in the 3D Viewport
 * (unrelated to text used to render the GUI).
 */

#pragma once

#include "DNA_ID.h"

#ifdef __cplusplus
extern "C" {
#endif

struct PackedFile;
struct VFontData;

typedef struct VFont {
  ID id;

  /** 1024 = FILE_MAX. */
  char filepath[1024];

  struct VFontData *data;
  struct PackedFile *packedfile;

  /* runtime only, holds memory for freetype to read from
   * TODO: replace this with #blf_font_new() style loading. */
  struct PackedFile *temp_pf;
} VFont;

/* *************** FONT ****************** */
#define FO_EDIT 0
#define FO_CURS 1
#define FO_CURSUP 2
#define FO_CURSDOWN 3
#define FO_DUPLI 4
#define FO_PAGEUP 8
#define FO_PAGEDOWN 9
#define FO_SELCHANGE 10

/* BKE_vfont_to_curve will move the cursor in these cases */
#define FO_CURS_IS_MOTION(mode) (ELEM(mode, FO_CURSUP, FO_CURSDOWN, FO_PAGEUP, FO_PAGEDOWN))

#define FO_BUILTIN_NAME "<builtin>"

#ifdef __cplusplus
}
#endif
