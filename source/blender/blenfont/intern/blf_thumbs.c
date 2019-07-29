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
 */

/** \file
 * \ingroup blf
 *
 * Utility function to generate font preview images.
 *
 * Isolate since this needs to be called by #ImBuf code (bad level call).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ft2build.h>

#include FT_FREETYPE_H

#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_threads.h"

#include "blf_internal.h"
#include "blf_internal_types.h"

#include "BLF_api.h"
#include "BLT_translation.h"

#include "BLI_strict_flags.h"

/**
 * This function is used for generating thumbnail previews.
 *
 * \note called from a thread, so it bypasses the normal BLF_* api (which isn't thread-safe).
 */
void BLF_thumb_preview(const char *filename,
                       const char **draw_str,
                       const char **i18n_draw_str,
                       const unsigned char draw_str_lines,
                       const float font_color[4],
                       const int font_size,
                       unsigned char *buf,
                       int w,
                       int h,
                       int channels)
{
  const unsigned int dpi = 72;
  const int font_size_min = 6;
  int font_size_curr;
  /* shrink 1/th each line */
  int font_shrink = 4;

  FontBLF *font;
  int i;

  /* Create a new blender font obj and fill it with default values */
  font = blf_font_new("thumb_font", filename);
  if (!font) {
    printf("Info: Can't load font '%s', no preview possible\n", filename);
    return;
  }

  /* Would be done via the BLF API, but we're not using a fontid here */
  font->buf_info.cbuf = buf;
  font->buf_info.ch = channels;
  font->buf_info.w = w;
  font->buf_info.h = h;

  /* Always create the image with a white font,
   * the caller can theme how it likes */
  memcpy(font->buf_info.col_init, font_color, sizeof(font->buf_info.col_init));
  font->pos[1] = (float)h;

  font_size_curr = font_size;

  blf_draw_buffer__start(font);

  for (i = 0; i < draw_str_lines; i++) {
    const char *draw_str_i18n = i18n_draw_str[i] != NULL ? i18n_draw_str[i] : draw_str[i];
    const size_t draw_str_i18n_len = strlen(draw_str_i18n);
    int draw_str_i18n_nbr = 0;

    blf_font_size(font, (unsigned int)MAX2(font_size_min, font_size_curr), dpi);

    /* font->glyph_cache remains NULL if blf_font_size() failed to set font size */
    if (!font->glyph_cache) {
      break;
    }

    /* decrease font size each time */
    font_size_curr -= (font_size_curr / font_shrink);
    font_shrink += 1;

    font->pos[1] -= font->glyph_cache->ascender * 1.1f;

    /* We fallback to default english strings in case not enough chars are available in current
     * font for given translated string (useful in non-latin i18n context, like chinese,
     * since many fonts will then show nothing but ugly 'missing char' in their preview).
     * Does not handle all cases, but much better than nothing.
     */
    if (blf_font_count_missing_chars(font, draw_str_i18n, draw_str_i18n_len, &draw_str_i18n_nbr) >
        (draw_str_i18n_nbr / 2)) {
      blf_font_draw_buffer(font, draw_str[i], strlen(draw_str[i]), NULL);
    }
    else {
      blf_font_draw_buffer(font, draw_str_i18n, draw_str_i18n_len, NULL);
    }
  }

  blf_draw_buffer__end();
  blf_font_free(font);
}
