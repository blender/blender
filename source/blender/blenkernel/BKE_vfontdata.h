/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 * \brief A structure to represent vector fonts,
 *   and to load them from PostScript fonts.
 */

#include "DNA_listBase.h"

#ifdef __cplusplus
extern "C" {
#endif

struct PackedFile;
struct VFont;

typedef struct VFontData {
  struct GHash *characters;
  char name[128];
  float scale;
  /* Calculated from the font. */
  float em_height;
  float ascender;
} VFontData;

typedef struct VChar {
  ListBase nurbsbase;
  unsigned int index;
  float width;
} VChar;

/**
 * Construct a new #VFontData structure from free-type font data in `pf`.
 *
 * \param pf: The font data.
 * \retval A new #VFontData structure, or NULL if unable to load.
 */
VFontData *BKE_vfontdata_from_freetypefont(struct PackedFile *pf);
VFontData *BKE_vfontdata_copy(const VFontData *vfont_src, int flag);

VChar *BKE_vfontdata_char_from_freetypefont(struct VFont *vfont, unsigned long character);
VChar *BKE_vfontdata_char_copy(const VChar *vchar_src);

#ifdef __cplusplus
}
#endif
