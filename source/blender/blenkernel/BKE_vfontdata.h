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
