/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
