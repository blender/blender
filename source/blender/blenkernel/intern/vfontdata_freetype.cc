/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * The Original Code is written by Rob Haarsma (phase). All rights reserved. */

/** \file
 * \ingroup bke
 *
 * This code parses the FreeType font outline data to chains of Blender's bezier-triples.
 * Additional information can be found at the bottom of this file.
 *
 * Code that uses exotic character maps is present but commented out.
 */

#include "MEM_guardedalloc.h"

#include "BLF_api.hh"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "BKE_curve.hh"
#include "BKE_vfont.hh"
#include "BKE_vfontdata.hh"

#include "DNA_packedFile_types.h"
#include "DNA_vfont_types.h"

extern const void *builtin_font_data;
extern int builtin_font_size;

void BKE_vfontdata_metrics_get_defaults(VFontData_Metrics *metrics)
{
  metrics->scale = BLF_VFONT_METRICS_SCALE_DEFAULT;
  metrics->em_ratio = BLF_VFONT_METRICS_EM_RATIO_DEFAULT;
  metrics->ascend_ratio = BLF_VFONT_METRICS_ASCEND_RATIO_DEFAULT;
}

VFontData *BKE_vfontdata_from_freetypefont(PackedFile *pf)
{
  int fontid = BLF_load_mem("FTVFont", static_cast<const uchar *>(pf->data), pf->size);
  if (fontid == -1) {
    return nullptr;
  }

  /* allocate blender font */
  VFontData *vfd = static_cast<VFontData *>(MEM_callocN(sizeof(*vfd), "FTVFontData"));

  /* Get the font name. */
  char *name = BLF_display_name_from_id(fontid);
  STRNCPY(vfd->name, name);
  /* BLF_display_name result must be freed. */
  MEM_freeN(name);

  BLI_str_utf8_invalid_strip(vfd->name, ARRAY_SIZE(vfd->name));

  if (!BLF_get_vfont_metrics(
          fontid, &vfd->metrics.ascend_ratio, &vfd->metrics.em_ratio, &vfd->metrics.scale))
  {
    BKE_vfontdata_metrics_get_defaults(&vfd->metrics);
  }

  vfd->characters = BLI_ghash_int_new_ex(__func__, 255);

  BLF_unload_id(fontid);

  return vfd;
}

static void *vfontdata_copy_characters_value_cb(const void *src)
{
  return BKE_vfontdata_char_copy(static_cast<const VChar *>(src));
}

VFontData *BKE_vfontdata_copy(const VFontData *vfont_src, const int /*flag*/)
{
  VFontData *vfont_dst = static_cast<VFontData *>(MEM_dupallocN(vfont_src));

  if (vfont_src->characters != nullptr) {
    vfont_dst->characters = BLI_ghash_copy(
        vfont_src->characters, nullptr, vfontdata_copy_characters_value_cb);
  }

  return vfont_dst;
}

VChar *BKE_vfontdata_char_from_freetypefont(VFont *vfont, uint character)
{
  if (!vfont) {
    return nullptr;
  }

  int font_id = -1;
  const bool is_builtin = BKE_vfont_is_builtin(vfont);

  /* If the default font is selected (usually because nothing has been selected)
   * then allow use of the fallback font stack when characters are not found in it.
   * This allows a nice first experience, for testing, and allows the translation of the initial
   * "Text" string. However, when a different specific font then do not use fallback,
   * only allow characters that are included in that font.
   * Do this for predictable control of what is shown when selecting specific fonts,
   * also for consistent output when the UI fonts are changed or updated. */
  const bool use_fallback = is_builtin;

  if (is_builtin) {
    font_id = BLF_load_mem(
        vfont->data->name, static_cast<const uchar *>(builtin_font_data), builtin_font_size);
  }
  else if (vfont->temp_pf) {
    font_id = BLF_load_mem(
        vfont->data->name, static_cast<const uchar *>(vfont->temp_pf->data), vfont->temp_pf->size);
  }

  if (font_id == -1) {
    return nullptr;
  }

  VChar *che = (VChar *)MEM_callocN(sizeof(VChar), "objfnt_char");

  /* need to set a size for embolden, etc. */
  BLF_size(font_id, 16);

  che->width = BLF_character_to_curves(
      font_id, character, &che->nurbsbase, vfont->data->metrics.scale, use_fallback);

  BLI_ghash_insert(vfont->data->characters, POINTER_FROM_UINT(character), che);
  BLF_unload_id(font_id);
  return che;
}

VChar *BKE_vfontdata_char_copy(const VChar *vchar_src)
{
  VChar *vchar_dst = static_cast<VChar *>(MEM_dupallocN(vchar_src));

  BLI_listbase_clear(&vchar_dst->nurbsbase);
  BKE_nurbList_duplicate(&vchar_dst->nurbsbase, &vchar_src->nurbsbase);

  return vchar_dst;
}
