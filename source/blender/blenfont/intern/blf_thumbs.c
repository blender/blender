/* SPDX-License-Identifier: GPL-2.0-or-later */

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

#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "blf_internal.h"
#include "blf_internal_types.h"

#include "BLF_api.h"
#include "BLT_translation.h"

#include "BLI_strict_flags.h"

void BLF_thumb_preview(const char *filepath,
                       const char **draw_str,
                       const char **i18n_draw_str,
                       const uchar draw_str_lines,
                       const float font_color[4],
                       const int font_size,
                       uchar *buf,
                       const int w,
                       const int h,
                       const int channels)
{
  const uint dpi = 72;
  const int font_size_min = 6;
  int font_size_curr;
  /* shrink 1/th each line */
  int font_shrink = 4;

  /* While viewing thumbnails in font directories this function can be called simultaneously from a
   * greater number of threads than we want the FreeType cache to keep open at a time. Therefore
   * pass own FT_Library to font creation so that it is not managed by the FreeType cache system.
   */

  FT_Library ft_library = NULL;
  if (FT_Init_FreeType(&ft_library) != FT_Err_Ok) {
    return;
  }

  FontBLF *font = blf_font_new_ex("thumb_font", filepath, NULL, 0, ft_library);
  if (!font) {
    printf("Info: Can't load font '%s', no preview possible\n", filepath);
    FT_Done_FreeType(ft_library);
    return;
  }

  /* Would be done via the BLF API, but we're not using a fontid here */
  font->buf_info.cbuf = buf;
  font->buf_info.ch = channels;
  font->buf_info.dims[0] = w;
  font->buf_info.dims[1] = h;

  /* Always create the image with a white font,
   * the caller can theme how it likes */
  memcpy(font->buf_info.col_init, font_color, sizeof(font->buf_info.col_init));
  font->pos[1] = h;

  font_size_curr = font_size;

  blf_draw_buffer__start(font);

  for (int i = 0; i < draw_str_lines; i++) {
    const char *draw_str_i18n = i18n_draw_str[i] != NULL ? i18n_draw_str[i] : draw_str[i];
    const size_t draw_str_i18n_len = strlen(draw_str_i18n);
    int draw_str_i18_count = 0;

    CLAMP_MIN(font_size_curr, font_size_min);
    if (!blf_font_size(font, (float)font_size_curr, dpi)) {
      break;
    }

    /* decrease font size each time */
    font_size_curr -= (font_size_curr / font_shrink);
    font_shrink += 1;

    font->pos[1] -= (int)((float)blf_font_ascender(font) * 1.1f);

    /* We fallback to default English strings in case not enough chars are available in current
     * font for given translated string (useful in non-Latin i18n context, like Chinese,
     * since many fonts will then show nothing but ugly 'missing char' in their preview).
     * Does not handle all cases, but much better than nothing.
     */
    if (blf_font_count_missing_chars(font, draw_str_i18n, draw_str_i18n_len, &draw_str_i18_count) >
        (draw_str_i18_count / 2)) {
      blf_font_draw_buffer(font, draw_str[i], strlen(draw_str[i]), NULL);
    }
    else {
      blf_font_draw_buffer(font, draw_str_i18n, draw_str_i18n_len, NULL);
    }
  }

  blf_draw_buffer__end();
  blf_font_free(font);
  FT_Done_FreeType(ft_library);
}
