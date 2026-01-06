/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "DNA_listBase.h"

#include "BLI_math_vector_types.hh"
#include "BLI_sys_types.h"

namespace blender {

/** \file
 * \ingroup bke
 */

struct CharInfo;
struct Curve;
struct Main;
struct Object;
struct VFont;
struct Nurb;

struct CharTrans {
  float2 offset;
  float rotate;
  short linenr, charnr;

  uint do_break : 1;
  uint is_overflow : 1;
  uint is_wrap : 1;
  uint is_smallcaps : 1;
};

struct EditFontSelBox {
  float x, y, w, h;
  float rotate;
};

/**
 * Edit data for #Curve (a text curve, with an #Object::type of `OB_FONT`).
 */
struct EditFont {
  /** Array of UTF32 code-points. */
  char32_t *textbuf;
  /** Text style info (aligned with `textbuf`). */
  CharInfo *textbufinfo;

  /** The font size from the last evaluated text layout. */
  float font_size_eval;

  /** Array of rectangles & rotation. */
  float2 textcurs[4];
  EditFontSelBox *selboxes;
  int selboxes_len;

  /* Positional vars relative to the `textbuf` (not UTF8 bytes)
   * a copy of these is kept in Curve, but use these in edit-mode. */

  /** Length of `textbuf`. */
  int len;
  /** Cursor position of (aligned with `textbuf`). */
  int pos;
  /** Text selection start/end, see #BKE_vfont_select_get. */
  int selstart, selend;

  /**
   * Combined styles from #CharInfo.flag for the selected range selected
   * (only including values from #CU_CHINFO_STYLE_ALL).
   * A flag will be set only if ALL characters in the selected string have it.
   */
  int select_char_info_flag;

  /**
   * ID data is older than edit-mode data.
   * Set #Main.is_memfile_undo_flush_needed when enabling.
   */
  char needs_flush_to_id;
};

enum eEditFontMode {
  FO_EDIT = 0,
  FO_CURS = 1,
  FO_CURSUP = 2,
  FO_CURSDOWN = 3,
  FO_DUPLI = 4,
  FO_PAGEUP = 8,
  FO_PAGEDOWN = 9,
  FO_LINE_BEGIN = 10,
  FO_LINE_END = 11,
  FO_SELCHANGE = 12,
};

/** #BKE_vfont_to_curve will move the cursor in these cases. */
#define FO_CURS_IS_MOTION(mode) \
  (ELEM(mode, FO_CURSUP, FO_CURSDOWN, FO_PAGEUP, FO_PAGEDOWN, FO_LINE_BEGIN, FO_LINE_END))

/* -------------------------------------------------------------------- */
/** \name VFont API
 *
 * See `vfont.c`.
 * \{ */

bool BKE_vfont_is_builtin(const VFont *vfont);
void BKE_vfont_builtin_register(const void *mem, int size);
/**
 * Return the built-in #VFont, without adding a user (the user-count may be zero).
 * The caller is responsible for adding a user.
 */
VFont *BKE_vfont_builtin_ensure();

void BKE_vfont_data_ensure(VFont *vfont);
void BKE_vfont_data_free(VFont *vfont);

VFont *BKE_vfont_load(Main *bmain, const char *filepath);
VFont *BKE_vfont_load_exists_ex(Main *bmain, const char *filepath, bool *r_exists);
VFont *BKE_vfont_load_exists(Main *bmain, const char *filepath);

int BKE_vfont_select_get(const Curve *cu, int *r_start, int *r_end);
void BKE_vfont_select_clamp(Curve *cu);

void BKE_vfont_clipboard_free();
void BKE_vfont_clipboard_set(const char32_t *text_buf, const CharInfo *info_buf, size_t len);
void BKE_vfont_clipboard_get(char32_t **r_text_buf,
                             CharInfo **r_info_buf,
                             size_t *r_len_utf8,
                             size_t *r_len_utf32);

/** \} */

/* -------------------------------------------------------------------- */
/** \name VFont Curve & Text Layout API
 *
 * See `vfont_curve.c`.
 * \{ */

int BKE_vfont_cursor_to_text_index(Object *ob, const float2 &cursor_location);

/**
 * \warning Expects to have access to evaluated data (i.e. passed object should be evaluated one).
 */
bool BKE_vfont_to_curve(Object *ob, eEditFontMode mode);
void BKE_vfont_char_build(const Curve &cu,
                          ListBaseT<Nurb> *nubase,
                          unsigned int charcode,
                          const CharInfo *info,
                          bool is_smallcaps,
                          const float2 &offset,
                          float rotate,
                          int charidx,
                          float fsize);

bool BKE_vfont_to_curve_ex(Object *ob,
                           const Curve &cu,
                           eEditFontMode mode,
                           ListBaseT<Nurb> *r_nubase,
                           const char32_t **r_text,
                           int *r_text_len,
                           bool *r_text_free,
                           CharTrans **r_chartransdata,
                           float *r_font_size_eval);
bool BKE_vfont_to_curve_nubase(Object *ob, eEditFontMode mode, ListBaseT<Nurb> *r_nubase);

/** \} */

}  // namespace blender
