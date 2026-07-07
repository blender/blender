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

#include "BLI_map.hh"

namespace blender {

struct GHash;
struct Nurb;
struct PackedFile;
struct VFont;

/**
 * Font metric values explained:
 *
 * Baseline: Line where the text "rests", used as the origin vertical position for the glyphs.
 * Em height: Space most glyphs should fit within.
 * Ascent: the recommended distance above the baseline to fit most characters.
 * Descent: the recommended distance below the baseline to fit most characters.
 *
 * We obtain ascent and descent from the font itself (`FT_Face->ascender / face->height`).
 * And in some cases it is even the same value as `FT_Face->bbox.yMax / yMin`.
 * (font top and bottom respectively).
 *
 * The `em_ratio` here is relative to `FT_Face->bbox`.
 */
struct VFontData_Metrics {
  float scale;
  /* Calculated from the font. */
  float em_ratio;
  float ascend_ratio;
};

struct VFontData {
  /**
   * A hash that maps `uint -> VChar` (code-points to character outlines).
   *
   * \note values may be null when the character does not exist in the font.
   * This is done to differentiate characters known not to exist from
   * characters that have not yet been loaded.
   */
  Map<uint, struct VChar *> *characters;
  char name[128];

  VFontData_Metrics metrics;
};

struct VChar {
  ListBaseT<Nurb> nurbsbase;
  float width;
};

/**
 * Default metrics to use when the font wont load.
 */
void BKE_vfontdata_metrics_get_defaults(VFontData_Metrics *metrics);

/**
 * Construct a new #VFontData structure from free-type font data in `pf`.
 *
 * \param pf: The font data.
 * \retval A new #VFontData structure, or NULL if unable to load.
 */
VFontData *BKE_vfontdata_from_freetypefont(PackedFile *pf);
VFontData *BKE_vfontdata_copy(const VFontData *vfont_src, int flag);

VChar *BKE_vfontdata_char_from_freetypefont(VFont *vfont, unsigned int character);
VChar *BKE_vfontdata_char_copy(const VChar *vchar_src);

}  // namespace blender
