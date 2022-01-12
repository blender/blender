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
 */

#ifdef __cplusplus
extern "C" {
#endif

struct CharInfo;
struct Curve;
struct Main;
struct Object;
struct VFont;

struct CharTrans {
  float xof, yof;
  float rot;
  short linenr, charnr;
  char dobreak;
};

typedef struct EditFontSelBox {
  float x, y, w, h;
  float rot;
} EditFontSelBox;

typedef struct EditFont {
  char32_t *textbuf;
  struct CharInfo *textbufinfo;

  /* array of rectangles & rotation */
  float textcurs[4][2];
  EditFontSelBox *selboxes;
  int selboxes_len;

  /* positional vars relative to the textbuf, textbufinfo (not utf8 bytes)
   * a copy of these is kept in Curve, but use these in editmode */
  int len, pos;
  int selstart, selend;

  /**
   * ID data is older than edit-mode data.
   * Set #Main.is_memfile_undo_flush_needed when enabling.
   */
  char needs_flush_to_id;

} EditFont;

bool BKE_vfont_is_builtin(const struct VFont *vfont);
void BKE_vfont_builtin_register(const void *mem, int size);

void BKE_vfont_free_data(struct VFont *vfont);
struct VFont *BKE_vfont_builtin_get(void);
struct VFont *BKE_vfont_load(struct Main *bmain, const char *filepath);
struct VFont *BKE_vfont_load_exists_ex(struct Main *bmain, const char *filepath, bool *r_exists);
struct VFont *BKE_vfont_load_exists(struct Main *bmain, const char *filepath);

bool BKE_vfont_to_curve_ex(struct Object *ob,
                           struct Curve *cu,
                           int mode,
                           struct ListBase *r_nubase,
                           const char32_t **r_text,
                           int *r_text_len,
                           bool *r_text_free,
                           struct CharTrans **r_chartransdata);
bool BKE_vfont_to_curve_nubase(struct Object *ob, int mode, struct ListBase *r_nubase);
/**
 * \warning Expects to have access to evaluated data (i.e. passed object should be evaluated one).
 */
bool BKE_vfont_to_curve(struct Object *ob, int mode);
void BKE_vfont_build_char(struct Curve *cu,
                          struct ListBase *nubase,
                          unsigned int character,
                          struct CharInfo *info,
                          float ofsx,
                          float ofsy,
                          float rot,
                          int charidx,
                          float fsize);

int BKE_vfont_select_get(struct Object *ob, int *r_start, int *r_end);
void BKE_vfont_select_clamp(struct Object *ob);

void BKE_vfont_clipboard_free(void);
void BKE_vfont_clipboard_set(const char32_t *text_buf,
                             const struct CharInfo *info_buf,
                             const size_t len);
void BKE_vfont_clipboard_get(char32_t **r_text_buf,
                             struct CharInfo **r_info_buf,
                             size_t *r_len_utf8,
                             size_t *r_len_utf32);

#ifdef __cplusplus
}
#endif
