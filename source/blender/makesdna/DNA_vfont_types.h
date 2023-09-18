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

#define FO_BUILTIN_NAME "<builtin>"
