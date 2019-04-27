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

/** \file
 * \ingroup bke
 */

#include <stdlib.h> /* abort */
#include <string.h> /* strstr */
#include <sys/types.h>
#include <sys/stat.h>
#include <wchar.h>
#include <wctype.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_string_cursor_utf8.h"
#include "BLI_string_utf8.h"
#include "BLI_listbase.h"
#include "BLI_fileops.h"

#include "DNA_constraint_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_text_types.h"
#include "DNA_userdef_types.h"
#include "DNA_object_types.h"
#include "DNA_node_types.h"
#include "DNA_material_types.h"

#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_text.h"
#include "BKE_node.h"

#ifdef WITH_PYTHON
#  include "BPY_extern.h"
#endif

/*
 * How Texts should work
 * --
 * A text should relate to a file as follows -
 * (Text *)->name should be the place where the
 *     file will or has been saved.
 *
 * (Text *)->flags has the following bits
 *     TXT_ISDIRTY - should always be set if the file in mem. differs from
 *                     the file on disk, or if there is no file on disk.
 *     TXT_ISMEM - should always be set if the Text has not been mapped to
 *                     a file, in which case (Text *)->name may be NULL or garbage.
 *     TXT_ISEXT - should always be set if the Text is not to be written into
 *                     the .blend
 *     TXT_ISSCRIPT - should be set if the user has designated the text
 *                     as a script. (NEW: this was unused, but now it is needed by
 *                     space handler script links (see header_view3d.c, for example)
 *
 * ->>> see also: /makesdna/DNA_text_types.h
 *
 * Display
 * --
 * The st->top determines at what line the top of the text is displayed.
 * If the user moves the cursor the st containing that cursor should
 * be popped ... other st's retain their own top location.
 *
 * Undo
 * --
 * Undo/Redo works by storing
 * events in a queue, and a pointer
 * to the current position in the
 * queue...
 *
 * Events are stored using an
 * arbitrary op-code system
 * to keep track of
 * a) the two cursors (normal and selected)
 * b) input (visible and control (ie backspace))
 *
 * input data is stored as its
 * ASCII value, the opcodes are
 * then selected to not conflict.
 *
 * opcodes with data in between are
 * written at the beginning and end
 * of the data to allow undo and redo
 * to simply check the code at the current
 * undo position
 */

/* Undo opcodes */

enum {
  /* Complex editing */
  /* 1 - opcode is followed by 1 byte for ascii character and opcode (repeat)) */
  /* 2 - opcode is followed by 2 bytes for utf-8 character and opcode (repeat)) */
  /* 3 - opcode is followed by 3 bytes for utf-8 character and opcode (repeat)) */
  /* 4 - opcode is followed by 4 bytes for unicode character and opcode (repeat)) */
  UNDO_INSERT_1 = 013,
  UNDO_INSERT_2 = 014,
  UNDO_INSERT_3 = 015,
  UNDO_INSERT_4 = 016,

  UNDO_BS_1 = 017,
  UNDO_BS_2 = 020,
  UNDO_BS_3 = 021,
  UNDO_BS_4 = 022,

  UNDO_DEL_1 = 023,
  UNDO_DEL_2 = 024,
  UNDO_DEL_3 = 025,
  UNDO_DEL_4 = 026,

  /* Text block (opcode is followed
   * by 4 character length ID + the text
   * block itself + the 4 character length
   * ID (repeat) and opcode (repeat)) */
  UNDO_DBLOCK = 027, /* Delete block */
  UNDO_IBLOCK = 030, /* Insert block */

  /* Misc */
  UNDO_INDENT = 032,
  UNDO_UNINDENT = 033,
  UNDO_COMMENT = 034,
  UNDO_UNCOMMENT = 035,

  UNDO_MOVE_LINES_UP = 036,
  UNDO_MOVE_LINES_DOWN = 037,

  UNDO_DUPLICATE = 040,
};

/***/

static void txt_pop_first(Text *text);
static void txt_pop_last(Text *text);
static void txt_undo_add_blockop(Text *text, TextUndoBuf *utxt, int op, const char *buf);
static void txt_delete_line(Text *text, TextLine *line);
static void txt_delete_sel(Text *text, TextUndoBuf *utxt);
static void txt_make_dirty(Text *text);

/***/

/**
 * Set to true when undoing (so we don't generate undo steps while undoing).
 *
 * Also use to disable undo entirely.
 */
static bool undoing;

/**
 * \note caller must handle `undo_buf` and `compiled` members.
 */
void BKE_text_free_lines(Text *text)
{
  for (TextLine *tmp = text->lines.first, *tmp_next; tmp; tmp = tmp_next) {
    tmp_next = tmp->next;
    MEM_freeN(tmp->line);
    if (tmp->format) {
      MEM_freeN(tmp->format);
    }
    MEM_freeN(tmp);
  }

  BLI_listbase_clear(&text->lines);

  text->curl = text->sell = NULL;
}

/** Free (or release) any data used by this text (does not free the text itself). */
void BKE_text_free(Text *text)
{
  /* No animdata here. */

  BKE_text_free_lines(text);

  MEM_SAFE_FREE(text->name);
#ifdef WITH_PYTHON
  BPY_text_free_code(text);
#endif
}

void BKE_text_init(Text *ta)
{
  TextLine *tmp;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(ta, id));

  ta->name = NULL;

  ta->nlines = 1;
  ta->flags = TXT_ISDIRTY | TXT_ISMEM;
  if ((U.flag & USER_TXT_TABSTOSPACES_DISABLE) == 0) {
    ta->flags |= TXT_TABSTOSPACES;
  }

  BLI_listbase_clear(&ta->lines);

  tmp = (TextLine *)MEM_mallocN(sizeof(TextLine), "textline");
  tmp->line = (char *)MEM_mallocN(1, "textline_string");
  tmp->format = NULL;

  tmp->line[0] = 0;
  tmp->len = 0;

  tmp->next = NULL;
  tmp->prev = NULL;

  BLI_addhead(&ta->lines, tmp);

  ta->curl = ta->lines.first;
  ta->curc = 0;
  ta->sell = ta->lines.first;
  ta->selc = 0;
}

Text *BKE_text_add(Main *bmain, const char *name)
{
  Text *ta;

  ta = BKE_libblock_alloc(bmain, ID_TXT, name, 0);

  BKE_text_init(ta);

  return ta;
}

/* this function replaces extended ascii characters */
/* to a valid utf-8 sequences */
int txt_extended_ascii_as_utf8(char **str)
{
  ptrdiff_t bad_char, i = 0;
  const ptrdiff_t length = (ptrdiff_t)strlen(*str);
  int added = 0;

  while ((*str)[i]) {
    if ((bad_char = BLI_utf8_invalid_byte(*str + i, length - i)) == -1) {
      break;
    }

    added++;
    i += bad_char + 1;
  }

  if (added != 0) {
    char *newstr = MEM_mallocN(length + added + 1, "text_line");
    ptrdiff_t mi = 0;
    i = 0;

    while ((*str)[i]) {
      if ((bad_char = BLI_utf8_invalid_byte((*str) + i, length - i)) == -1) {
        memcpy(newstr + mi, (*str) + i, length - i + 1);
        break;
      }

      memcpy(newstr + mi, (*str) + i, bad_char);

      BLI_str_utf8_from_unicode((*str)[i + bad_char], newstr + mi + bad_char);
      i += bad_char + 1;
      mi += bad_char + 2;
    }
    newstr[length + added] = '\0';
    MEM_freeN(*str);
    *str = newstr;
  }

  return added;
}

// this function removes any control characters from
// a textline and fixes invalid utf-8 sequences

static void cleanup_textline(TextLine *tl)
{
  int i;

  for (i = 0; i < tl->len; i++) {
    if (tl->line[i] < ' ' && tl->line[i] != '\t') {
      memmove(tl->line + i, tl->line + i + 1, tl->len - i);
      tl->len--;
      i--;
    }
  }
  tl->len += txt_extended_ascii_as_utf8(&tl->line);
}

/**
 * used for load and reload (unlike txt_insert_buf)
 * assumes all fields are empty
 */
static void text_from_buf(Text *text, const unsigned char *buffer, const int len)
{
  int i, llen;

  BLI_assert(BLI_listbase_is_empty(&text->lines));

  text->nlines = 0;
  llen = 0;
  for (i = 0; i < len; i++) {
    if (buffer[i] == '\n') {
      TextLine *tmp;

      tmp = (TextLine *)MEM_mallocN(sizeof(TextLine), "textline");
      tmp->line = (char *)MEM_mallocN(llen + 1, "textline_string");
      tmp->format = NULL;

      if (llen) {
        memcpy(tmp->line, &buffer[i - llen], llen);
      }
      tmp->line[llen] = 0;
      tmp->len = llen;

      cleanup_textline(tmp);

      BLI_addtail(&text->lines, tmp);
      text->nlines++;

      llen = 0;
      continue;
    }
    llen++;
  }

  /* create new line in cases:
   * - rest of line (if last line in file hasn't got \n terminator).
   *   in this case content of such line would be used to fill text line buffer
   * - file is empty. in this case new line is needed to start editing from.
   * - last character in buffer is \n. in this case new line is needed to
   *   deal with newline at end of file. (see [#28087]) (sergey) */
  if (llen != 0 || text->nlines == 0 || buffer[len - 1] == '\n') {
    TextLine *tmp;

    tmp = (TextLine *)MEM_mallocN(sizeof(TextLine), "textline");
    tmp->line = (char *)MEM_mallocN(llen + 1, "textline_string");
    tmp->format = NULL;

    if (llen) {
      memcpy(tmp->line, &buffer[i - llen], llen);
    }

    tmp->line[llen] = 0;
    tmp->len = llen;

    cleanup_textline(tmp);

    BLI_addtail(&text->lines, tmp);
    text->nlines++;
  }

  text->curl = text->sell = text->lines.first;
  text->curc = text->selc = 0;
}

bool BKE_text_reload(Text *text)
{
  unsigned char *buffer;
  size_t buffer_len;
  char filepath_abs[FILE_MAX];
  BLI_stat_t st;

  if (!text->name) {
    return false;
  }

  BLI_strncpy(filepath_abs, text->name, FILE_MAX);
  BLI_path_abs(filepath_abs, BKE_main_blendfile_path_from_global());

  buffer = BLI_file_read_text_as_mem(filepath_abs, 0, &buffer_len);
  if (buffer == NULL) {
    return false;
  }

  /* free memory: */
  BKE_text_free_lines(text);
  txt_make_dirty(text);

  /* clear undo buffer */
  if (BLI_stat(filepath_abs, &st) != -1) {
    text->mtime = st.st_mtime;
  }
  else {
    text->mtime = 0;
  }

  text_from_buf(text, buffer, buffer_len);

  MEM_freeN(buffer);
  return true;
}

Text *BKE_text_load_ex(Main *bmain, const char *file, const char *relpath, const bool is_internal)
{
  unsigned char *buffer;
  size_t buffer_len;
  Text *ta;
  char filepath_abs[FILE_MAX];
  BLI_stat_t st;

  BLI_strncpy(filepath_abs, file, FILE_MAX);
  if (relpath) { /* can be NULL (bg mode) */
    BLI_path_abs(filepath_abs, relpath);
  }

  buffer = BLI_file_read_text_as_mem(filepath_abs, 0, &buffer_len);
  if (buffer == NULL) {
    return false;
  }

  ta = BKE_libblock_alloc(bmain, ID_TXT, BLI_path_basename(filepath_abs), 0);
  ta->id.us = 0;

  BLI_listbase_clear(&ta->lines);
  ta->curl = ta->sell = NULL;

  if ((U.flag & USER_TXT_TABSTOSPACES_DISABLE) == 0) {
    ta->flags = TXT_TABSTOSPACES;
  }

  if (is_internal == false) {
    ta->name = MEM_mallocN(strlen(file) + 1, "text_name");
    strcpy(ta->name, file);
  }
  else {
    ta->flags |= TXT_ISMEM | TXT_ISDIRTY;
  }

  /* clear undo buffer */
  if (BLI_stat(filepath_abs, &st) != -1) {
    ta->mtime = st.st_mtime;
  }
  else {
    ta->mtime = 0;
  }

  text_from_buf(ta, buffer, buffer_len);

  MEM_freeN(buffer);

  return ta;
}

Text *BKE_text_load(Main *bmain, const char *file, const char *relpath)
{
  return BKE_text_load_ex(bmain, file, relpath, false);
}

/**
 * Only copy internal data of Text ID from source
 * to already allocated/initialized destination.
 * You probably never want to use that directly,
 * use #BKE_id_copy or #BKE_id_copy_ex for typical needs.
 *
 * WARNING! This function will not handle ID user count!
 *
 * \param flag: Copying options (see BKE_library.h's LIB_ID_COPY_... flags for more).
 */
void BKE_text_copy_data(Main *UNUSED(bmain),
                        Text *ta_dst,
                        const Text *ta_src,
                        const int UNUSED(flag))
{
  /* file name can be NULL */
  if (ta_src->name) {
    ta_dst->name = BLI_strdup(ta_src->name);
  }

  ta_dst->flags |= TXT_ISDIRTY;

  BLI_listbase_clear(&ta_dst->lines);
  ta_dst->curl = ta_dst->sell = NULL;
  ta_dst->compiled = NULL;

  /* Walk down, reconstructing */
  for (TextLine *line_src = ta_src->lines.first; line_src; line_src = line_src->next) {
    TextLine *line_dst = MEM_mallocN(sizeof(*line_dst), __func__);

    line_dst->line = BLI_strdup(line_src->line);
    line_dst->format = NULL;
    line_dst->len = line_src->len;

    BLI_addtail(&ta_dst->lines, line_dst);
  }

  ta_dst->curl = ta_dst->sell = ta_dst->lines.first;
  ta_dst->curc = ta_dst->selc = 0;
}

Text *BKE_text_copy(Main *bmain, const Text *ta)
{
  Text *ta_copy;
  BKE_id_copy(bmain, &ta->id, (ID **)&ta_copy);
  return ta_copy;
}

void BKE_text_make_local(Main *bmain, Text *text, const bool lib_local)
{
  BKE_id_make_local_generic(bmain, &text->id, true, lib_local);
}

void BKE_text_clear(Text *text, TextUndoBuf *utxt) /* called directly from rna */
{
  const bool undoing_orig = undoing;
  undoing = (utxt == NULL);

  txt_sel_all(text);
  txt_delete_sel(text, utxt);

  undoing = undoing_orig;

  txt_make_dirty(text);
}

void BKE_text_write(Text *text, TextUndoBuf *utxt, const char *str) /* called directly from rna */
{
  const bool undoing_orig = undoing;
  undoing = (utxt == NULL);

  txt_insert_buf(text, utxt, str);
  txt_move_eof(text, 0);

  undoing = undoing_orig;

  txt_make_dirty(text);
}

/* returns 0 if file on disk is the same or Text is in memory only
 * returns 1 if file has been modified on disk since last local edit
 * returns 2 if file on disk has been deleted
 * -1 is returned if an error occurs */

int BKE_text_file_modified_check(Text *text)
{
  BLI_stat_t st;
  int result;
  char file[FILE_MAX];

  if (!text->name) {
    return 0;
  }

  BLI_strncpy(file, text->name, FILE_MAX);
  BLI_path_abs(file, BKE_main_blendfile_path_from_global());

  if (!BLI_exists(file)) {
    return 2;
  }

  result = BLI_stat(file, &st);

  if (result == -1) {
    return -1;
  }

  if ((st.st_mode & S_IFMT) != S_IFREG) {
    return -1;
  }

  if (st.st_mtime > text->mtime) {
    return 1;
  }

  return 0;
}

void BKE_text_file_modified_ignore(Text *text)
{
  BLI_stat_t st;
  int result;
  char file[FILE_MAX];

  if (!text->name) {
    return;
  }

  BLI_strncpy(file, text->name, FILE_MAX);
  BLI_path_abs(file, BKE_main_blendfile_path_from_global());

  if (!BLI_exists(file)) {
    return;
  }

  result = BLI_stat(file, &st);

  if (result == -1 || (st.st_mode & S_IFMT) != S_IFREG) {
    return;
  }

  text->mtime = st.st_mtime;
}

/*****************************/
/* Editing utility functions */
/*****************************/

static void make_new_line(TextLine *line, char *newline)
{
  if (line->line) {
    MEM_freeN(line->line);
  }
  if (line->format) {
    MEM_freeN(line->format);
  }

  line->line = newline;
  line->len = strlen(newline);
  line->format = NULL;
}

static TextLine *txt_new_line(const char *str)
{
  TextLine *tmp;

  if (!str) {
    str = "";
  }

  tmp = (TextLine *)MEM_mallocN(sizeof(TextLine), "textline");
  tmp->line = MEM_mallocN(strlen(str) + 1, "textline_string");
  tmp->format = NULL;

  strcpy(tmp->line, str);

  tmp->len = strlen(str);
  tmp->next = tmp->prev = NULL;

  return tmp;
}

static TextLine *txt_new_linen(const char *str, int n)
{
  TextLine *tmp;

  tmp = (TextLine *)MEM_mallocN(sizeof(TextLine), "textline");
  tmp->line = MEM_mallocN(n + 1, "textline_string");
  tmp->format = NULL;

  BLI_strncpy(tmp->line, (str) ? str : "", n + 1);

  tmp->len = strlen(tmp->line);
  tmp->next = tmp->prev = NULL;

  return tmp;
}

void txt_clean_text(Text *text)
{
  TextLine **top, **bot;

  if (!text->lines.first) {
    if (text->lines.last) {
      text->lines.first = text->lines.last;
    }
    else {
      text->lines.first = text->lines.last = txt_new_line(NULL);
    }
  }

  if (!text->lines.last) {
    text->lines.last = text->lines.first;
  }

  top = (TextLine **)&text->lines.first;
  bot = (TextLine **)&text->lines.last;

  while ((*top)->prev) {
    *top = (*top)->prev;
  }
  while ((*bot)->next) {
    *bot = (*bot)->next;
  }

  if (!text->curl) {
    if (text->sell) {
      text->curl = text->sell;
    }
    else {
      text->curl = text->lines.first;
    }
    text->curc = 0;
  }

  if (!text->sell) {
    text->sell = text->curl;
    text->selc = 0;
  }
}

int txt_get_span(TextLine *from, TextLine *to)
{
  int ret = 0;
  TextLine *tmp = from;

  if (!to || !from) {
    return 0;
  }
  if (from == to) {
    return 0;
  }

  /* Look forwards */
  while (tmp) {
    if (tmp == to) {
      return ret;
    }
    ret++;
    tmp = tmp->next;
  }

  /* Look backwards */
  if (!tmp) {
    tmp = from;
    ret = 0;
    while (tmp) {
      if (tmp == to) {
        break;
      }
      ret--;
      tmp = tmp->prev;
    }
    if (!tmp) {
      ret = 0;
    }
  }

  return ret;
}

static void txt_make_dirty(Text *text)
{
  text->flags |= TXT_ISDIRTY;
#ifdef WITH_PYTHON
  if (text->compiled) {
    BPY_text_free_code(text);
  }
#endif
}

/****************************/
/* Cursor utility functions */
/****************************/

static void txt_curs_cur(Text *text, TextLine ***linep, int **charp)
{
  *linep = &text->curl;
  *charp = &text->curc;
}

static void txt_curs_sel(Text *text, TextLine ***linep, int **charp)
{
  *linep = &text->sell;
  *charp = &text->selc;
}

bool txt_cursor_is_line_start(Text *text)
{
  return (text->selc == 0);
}

bool txt_cursor_is_line_end(Text *text)
{
  return (text->selc == text->sell->len);
}

/*****************************/
/* Cursor movement functions */
/*****************************/

int txt_utf8_offset_to_index(const char *str, int offset)
{
  int index = 0, pos = 0;
  while (pos != offset) {
    pos += BLI_str_utf8_size(str + pos);
    index++;
  }
  return index;
}

int txt_utf8_index_to_offset(const char *str, int index)
{
  int offset = 0, pos = 0;
  while (pos != index) {
    offset += BLI_str_utf8_size(str + offset);
    pos++;
  }
  return offset;
}

int txt_utf8_offset_to_column(const char *str, int offset)
{
  int column = 0, pos = 0;
  while (pos < offset) {
    column += BLI_str_utf8_char_width_safe(str + pos);
    pos += BLI_str_utf8_size_safe(str + pos);
  }
  return column;
}

int txt_utf8_column_to_offset(const char *str, int column)
{
  int offset = 0, pos = 0, col;
  while (*(str + offset) && pos < column) {
    col = BLI_str_utf8_char_width_safe(str + offset);
    if (pos + col > column) {
      break;
    }
    offset += BLI_str_utf8_size_safe(str + offset);
    pos += col;
  }
  return offset;
}

void txt_move_up(Text *text, const bool sel)
{
  TextLine **linep;
  int *charp;

  if (sel) {
    txt_curs_sel(text, &linep, &charp);
  }
  else {
    txt_pop_first(text);
    txt_curs_cur(text, &linep, &charp);
  }
  if (!*linep) {
    return;
  }

  if ((*linep)->prev) {
    int column = txt_utf8_offset_to_column((*linep)->line, *charp);
    *linep = (*linep)->prev;
    *charp = txt_utf8_column_to_offset((*linep)->line, column);
  }
  else {
    txt_move_bol(text, sel);
  }

  if (!sel) {
    txt_pop_sel(text);
  }
}

void txt_move_down(Text *text, const bool sel)
{
  TextLine **linep;
  int *charp;

  if (sel) {
    txt_curs_sel(text, &linep, &charp);
  }
  else {
    txt_pop_last(text);
    txt_curs_cur(text, &linep, &charp);
  }
  if (!*linep) {
    return;
  }

  if ((*linep)->next) {
    int column = txt_utf8_offset_to_column((*linep)->line, *charp);
    *linep = (*linep)->next;
    *charp = txt_utf8_column_to_offset((*linep)->line, column);
  }
  else {
    txt_move_eol(text, sel);
  }

  if (!sel) {
    txt_pop_sel(text);
  }
}

int txt_calc_tab_left(TextLine *tl, int ch)
{
  /* do nice left only if there are only spaces */

  int tabsize = (ch < TXT_TABSIZE) ? ch : TXT_TABSIZE;

  for (int i = 0; i < ch; i++) {
    if (tl->line[i] != ' ') {
      tabsize = 0;
      break;
    }
  }

  /* if in the middle of the space-tab */
  if (tabsize && ch % TXT_TABSIZE != 0) {
    tabsize = (ch % TXT_TABSIZE);
  }
  return tabsize;
}

int txt_calc_tab_right(TextLine *tl, int ch)
{
  if (tl->line[ch] == ' ') {
    int i;
    for (i = 0; i < ch; i++) {
      if (tl->line[i] != ' ') {
        return 0;
      }
    }

    int tabsize = (ch) % TXT_TABSIZE + 1;
    for (i = ch + 1; tl->line[i] == ' ' && tabsize < TXT_TABSIZE; i++) {
      tabsize++;
    }

    return i - ch;
  }
  else {
    return 0;
  }
}

void txt_move_left(Text *text, const bool sel)
{
  TextLine **linep;
  int *charp;
  int tabsize = 0;

  if (sel) {
    txt_curs_sel(text, &linep, &charp);
  }
  else {
    txt_pop_first(text);
    txt_curs_cur(text, &linep, &charp);
  }
  if (!*linep) {
    return;
  }

  if (*charp == 0) {
    if ((*linep)->prev) {
      txt_move_up(text, sel);
      *charp = (*linep)->len;
    }
  }
  else {
    /* do nice left only if there are only spaces */
    // TXT_TABSIZE hardcoded in DNA_text_types.h
    if (text->flags & TXT_TABSTOSPACES) {
      tabsize = txt_calc_tab_left(*linep, *charp);
    }

    if (tabsize) {
      (*charp) -= tabsize;
    }
    else {
      const char *prev = BLI_str_prev_char_utf8((*linep)->line + *charp);
      *charp = prev - (*linep)->line;
    }
  }

  if (!sel) {
    txt_pop_sel(text);
  }
}

void txt_move_right(Text *text, const bool sel)
{
  TextLine **linep;
  int *charp;

  if (sel) {
    txt_curs_sel(text, &linep, &charp);
  }
  else {
    txt_pop_last(text);
    txt_curs_cur(text, &linep, &charp);
  }
  if (!*linep) {
    return;
  }

  if (*charp == (*linep)->len) {
    if ((*linep)->next) {
      txt_move_down(text, sel);
      *charp = 0;
    }
  }
  else {
    /* do nice right only if there are only spaces */
    /* spaces hardcoded in DNA_text_types.h */
    int tabsize = 0;

    if (text->flags & TXT_TABSTOSPACES) {
      tabsize = txt_calc_tab_right(*linep, *charp);
    }

    if (tabsize) {
      (*charp) += tabsize;
    }
    else {
      (*charp) += BLI_str_utf8_size((*linep)->line + *charp);
    }
  }

  if (!sel) {
    txt_pop_sel(text);
  }
}

void txt_jump_left(Text *text, const bool sel, const bool use_init_step)
{
  TextLine **linep;
  int *charp;

  if (sel) {
    txt_curs_sel(text, &linep, &charp);
  }
  else {
    txt_pop_first(text);
    txt_curs_cur(text, &linep, &charp);
  }
  if (!*linep) {
    return;
  }

  BLI_str_cursor_step_utf8(
      (*linep)->line, (*linep)->len, charp, STRCUR_DIR_PREV, STRCUR_JUMP_DELIM, use_init_step);

  if (!sel) {
    txt_pop_sel(text);
  }
}

void txt_jump_right(Text *text, const bool sel, const bool use_init_step)
{
  TextLine **linep;
  int *charp;

  if (sel) {
    txt_curs_sel(text, &linep, &charp);
  }
  else {
    txt_pop_last(text);
    txt_curs_cur(text, &linep, &charp);
  }
  if (!*linep) {
    return;
  }

  BLI_str_cursor_step_utf8(
      (*linep)->line, (*linep)->len, charp, STRCUR_DIR_NEXT, STRCUR_JUMP_DELIM, use_init_step);

  if (!sel) {
    txt_pop_sel(text);
  }
}

void txt_move_bol(Text *text, const bool sel)
{
  TextLine **linep;
  int *charp;

  if (sel) {
    txt_curs_sel(text, &linep, &charp);
  }
  else {
    txt_curs_cur(text, &linep, &charp);
  }
  if (!*linep) {
    return;
  }

  *charp = 0;

  if (!sel) {
    txt_pop_sel(text);
  }
}

void txt_move_eol(Text *text, const bool sel)
{
  TextLine **linep;
  int *charp;

  if (sel) {
    txt_curs_sel(text, &linep, &charp);
  }
  else {
    txt_curs_cur(text, &linep, &charp);
  }
  if (!*linep) {
    return;
  }

  *charp = (*linep)->len;

  if (!sel) {
    txt_pop_sel(text);
  }
}

void txt_move_bof(Text *text, const bool sel)
{
  TextLine **linep;
  int *charp;

  if (sel) {
    txt_curs_sel(text, &linep, &charp);
  }
  else {
    txt_curs_cur(text, &linep, &charp);
  }
  if (!*linep) {
    return;
  }

  *linep = text->lines.first;
  *charp = 0;

  if (!sel) {
    txt_pop_sel(text);
  }
}

void txt_move_eof(Text *text, const bool sel)
{
  TextLine **linep;
  int *charp;

  if (sel) {
    txt_curs_sel(text, &linep, &charp);
  }
  else {
    txt_curs_cur(text, &linep, &charp);
  }
  if (!*linep) {
    return;
  }

  *linep = text->lines.last;
  *charp = (*linep)->len;

  if (!sel) {
    txt_pop_sel(text);
  }
}

void txt_move_toline(Text *text, unsigned int line, const bool sel)
{
  txt_move_to(text, line, 0, sel);
}

/* Moves to a certain byte in a line, not a certain utf8-character! */
void txt_move_to(Text *text, unsigned int line, unsigned int ch, const bool sel)
{
  TextLine **linep;
  int *charp;
  unsigned int i;

  if (sel) {
    txt_curs_sel(text, &linep, &charp);
  }
  else {
    txt_curs_cur(text, &linep, &charp);
  }
  if (!*linep) {
    return;
  }

  *linep = text->lines.first;
  for (i = 0; i < line; i++) {
    if ((*linep)->next) {
      *linep = (*linep)->next;
    }
    else {
      break;
    }
  }
  if (ch > (unsigned int)((*linep)->len)) {
    ch = (unsigned int)((*linep)->len);
  }
  *charp = ch;

  if (!sel) {
    txt_pop_sel(text);
  }
}

/****************************/
/* Text selection functions */
/****************************/

static void txt_curs_swap(Text *text)
{
  TextLine *tmpl;
  int tmpc;

  tmpl = text->curl;
  text->curl = text->sell;
  text->sell = tmpl;

  tmpc = text->curc;
  text->curc = text->selc;
  text->selc = tmpc;
}

static void txt_pop_first(Text *text)
{
  if (txt_get_span(text->curl, text->sell) < 0 ||
      (text->curl == text->sell && text->curc > text->selc)) {
    txt_curs_swap(text);
  }

  txt_pop_sel(text);
}

static void txt_pop_last(Text *text)
{
  if (txt_get_span(text->curl, text->sell) > 0 ||
      (text->curl == text->sell && text->curc < text->selc)) {
    txt_curs_swap(text);
  }

  txt_pop_sel(text);
}

void txt_pop_sel(Text *text)
{
  text->sell = text->curl;
  text->selc = text->curc;
}

void txt_order_cursors(Text *text, const bool reverse)
{
  if (!text->curl) {
    return;
  }
  if (!text->sell) {
    return;
  }

  /* Flip so text->curl is before/after text->sell */
  if (reverse == false) {
    if ((txt_get_span(text->curl, text->sell) < 0) ||
        (text->curl == text->sell && text->curc > text->selc)) {
      txt_curs_swap(text);
    }
  }
  else {
    if ((txt_get_span(text->curl, text->sell) > 0) ||
        (text->curl == text->sell && text->curc < text->selc)) {
      txt_curs_swap(text);
    }
  }
}

bool txt_has_sel(Text *text)
{
  return ((text->curl != text->sell) || (text->curc != text->selc));
}

static void txt_delete_sel(Text *text, TextUndoBuf *utxt)
{
  TextLine *tmpl;
  char *buf;

  if (!text->curl) {
    return;
  }
  if (!text->sell) {
    return;
  }

  if (!txt_has_sel(text)) {
    return;
  }

  txt_order_cursors(text, false);

  if (!undoing) {
    buf = txt_sel_to_buf(text);
    txt_undo_add_blockop(text, utxt, UNDO_DBLOCK, buf);
    MEM_freeN(buf);
  }

  buf = MEM_mallocN(text->curc + (text->sell->len - text->selc) + 1, "textline_string");

  strncpy(buf, text->curl->line, text->curc);
  strcpy(buf + text->curc, text->sell->line + text->selc);
  buf[text->curc + (text->sell->len - text->selc)] = 0;

  make_new_line(text->curl, buf);

  tmpl = text->sell;
  while (tmpl != text->curl) {
    tmpl = tmpl->prev;
    if (!tmpl) {
      break;
    }

    txt_delete_line(text, tmpl->next);
  }

  text->sell = text->curl;
  text->selc = text->curc;
}

void txt_sel_all(Text *text)
{
  text->curl = text->lines.first;
  text->curc = 0;

  text->sell = text->lines.last;
  text->selc = text->sell->len;
}

/**
 * Reverse of #txt_pop_sel
 * Clears the selection and ensures the cursor is located
 * at the selection (where the cursor is visually while editing).
 */
void txt_sel_clear(Text *text)
{
  if (text->sell) {
    text->curl = text->sell;
    text->curc = text->selc;
  }
}

void txt_sel_line(Text *text)
{
  if (!text->curl) {
    return;
  }

  text->curc = 0;
  text->sell = text->curl;
  text->selc = text->sell->len;
}

/***************************/
/* Cut and paste functions */
/***************************/

char *txt_to_buf(Text *text)
{
  int length;
  TextLine *tmp, *linef, *linel;
  int charf, charl;
  char *buf;

  if (!text->curl) {
    return NULL;
  }
  if (!text->sell) {
    return NULL;
  }
  if (!text->lines.first) {
    return NULL;
  }

  linef = text->lines.first;
  charf = 0;

  linel = text->lines.last;
  charl = linel->len;

  if (linef == text->lines.last) {
    length = charl - charf;

    buf = MEM_mallocN(length + 2, "text buffer");

    BLI_strncpy(buf, linef->line + charf, length + 1);
    buf[length] = 0;
  }
  else {
    length = linef->len - charf;
    length += charl;
    length += 2; /* For the 2 '\n' */

    tmp = linef->next;
    while (tmp && tmp != linel) {
      length += tmp->len + 1;
      tmp = tmp->next;
    }

    buf = MEM_mallocN(length + 1, "cut buffer");

    strncpy(buf, linef->line + charf, linef->len - charf);
    length = linef->len - charf;

    buf[length++] = '\n';

    tmp = linef->next;
    while (tmp && tmp != linel) {
      strncpy(buf + length, tmp->line, tmp->len);
      length += tmp->len;

      buf[length++] = '\n';

      tmp = tmp->next;
    }
    strncpy(buf + length, linel->line, charl);
    length += charl;

    /* python compiler wants an empty end line */
    buf[length++] = '\n';
    buf[length] = 0;
  }

  return buf;
}

int txt_find_string(Text *text, const char *findstr, int wrap, int match_case)
{
  TextLine *tl, *startl;
  const char *s = NULL;

  if (!text->curl || !text->sell) {
    return 0;
  }

  txt_order_cursors(text, false);

  tl = startl = text->sell;

  if (match_case) {
    s = strstr(&tl->line[text->selc], findstr);
  }
  else {
    s = BLI_strcasestr(&tl->line[text->selc], findstr);
  }
  while (!s) {
    tl = tl->next;
    if (!tl) {
      if (wrap) {
        tl = text->lines.first;
      }
      else {
        break;
      }
    }

    if (match_case) {
      s = strstr(tl->line, findstr);
    }
    else {
      s = BLI_strcasestr(tl->line, findstr);
    }
    if (tl == startl) {
      break;
    }
  }

  if (s) {
    int newl = txt_get_span(text->lines.first, tl);
    int newc = (int)(s - tl->line);
    txt_move_to(text, newl, newc, 0);
    txt_move_to(text, newl, newc + strlen(findstr), 1);
    return 1;
  }
  else {
    return 0;
  }
}

char *txt_sel_to_buf(Text *text)
{
  char *buf;
  int length = 0;
  TextLine *tmp, *linef, *linel;
  int charf, charl;

  if (!text->curl) {
    return NULL;
  }
  if (!text->sell) {
    return NULL;
  }

  if (text->curl == text->sell) {
    linef = linel = text->curl;

    if (text->curc < text->selc) {
      charf = text->curc;
      charl = text->selc;
    }
    else {
      charf = text->selc;
      charl = text->curc;
    }
  }
  else if (txt_get_span(text->curl, text->sell) < 0) {
    linef = text->sell;
    linel = text->curl;

    charf = text->selc;
    charl = text->curc;
  }
  else {
    linef = text->curl;
    linel = text->sell;

    charf = text->curc;
    charl = text->selc;
  }

  if (linef == linel) {
    length = charl - charf;

    buf = MEM_mallocN(length + 1, "sel buffer");

    BLI_strncpy(buf, linef->line + charf, length + 1);
  }
  else {
    length += linef->len - charf;
    length += charl;
    length++; /* For the '\n' */

    tmp = linef->next;
    while (tmp && tmp != linel) {
      length += tmp->len + 1;
      tmp = tmp->next;
    }

    buf = MEM_mallocN(length + 1, "sel buffer");

    strncpy(buf, linef->line + charf, linef->len - charf);
    length = linef->len - charf;

    buf[length++] = '\n';

    tmp = linef->next;
    while (tmp && tmp != linel) {
      strncpy(buf + length, tmp->line, tmp->len);
      length += tmp->len;

      buf[length++] = '\n';

      tmp = tmp->next;
    }
    strncpy(buf + length, linel->line, charl);
    length += charl;

    buf[length] = 0;
  }

  return buf;
}

void txt_insert_buf(Text *text, TextUndoBuf *utxt, const char *in_buffer)
{
  const bool undoing_orig = undoing;
  int l = 0, len;
  size_t i = 0, j;
  TextLine *add;
  char *buffer;

  if (!in_buffer) {
    return;
  }

  txt_delete_sel(text, utxt);

  len = strlen(in_buffer);
  buffer = BLI_strdupn(in_buffer, len);
  len += txt_extended_ascii_as_utf8(&buffer);

  if (!undoing) {
    txt_undo_add_blockop(text, utxt, UNDO_IBLOCK, buffer);
  }
  undoing = true;

  /* Read the first line (or as close as possible */
  while (buffer[i] && buffer[i] != '\n') {
    txt_add_raw_char(text, utxt, BLI_str_utf8_as_unicode_step(buffer, &i));
  }

  if (buffer[i] == '\n') {
    txt_split_curline(text, utxt);
    i++;

    while (i < len) {
      l = 0;

      while (buffer[i] && buffer[i] != '\n') {
        i++;
        l++;
      }

      if (buffer[i] == '\n') {
        add = txt_new_linen(buffer + (i - l), l);
        BLI_insertlinkbefore(&text->lines, text->curl, add);
        i++;
      }
      else {
        for (j = i - l; j < i && j < len;) {
          txt_add_raw_char(text, utxt, BLI_str_utf8_as_unicode_step(buffer, &j));
        }
        break;
      }
    }
  }

  MEM_freeN(buffer);
  undoing = undoing_orig;
}

/******************/
/* Undo functions */
/******************/

static bool max_undo_test(TextUndoBuf *utxt, int x)
{
  /* Normally over-allocating is preferred,
   * however in this case the buffer is small enough and re-allocation
   * fast enough for each undo step that it's not a problem to allocate each time.
   * This also saves on some memory when we have many text buffers
   * that would have an empty undo memory allocated.
   */

  /* Add one for the null terminator. */
  utxt->len = utxt->pos + x + 1;
  if (utxt->len > TXT_MAX_UNDO) {
    /* XXX error("Undo limit reached, buffer cleared\n"); */
    MEM_freeN(utxt->buf);
    return false;
  }
  else {
    /* Small reallocations on each undo step is fine. */
    utxt->buf = MEM_recallocN(utxt->buf, utxt->len);
  }
  return true;
}

static void txt_undo_end(Text *UNUSED(text), TextUndoBuf *utxt)
{
  int undo_pos_end = utxt->pos + 1;
  BLI_assert(undo_pos_end + 1 == utxt->len);
  utxt->buf[undo_pos_end] = '\0';
}

/* Call once undo is done. */
#ifndef NDEBUG

#endif

#if 0 /* UNUSED */
static void dump_buffer(TextUndoBuf *utxt)
{
  int i = 0;

  while (i++ < utxt->undo_pos)
    printf("%d: %d %c\n", i, utxt->buf[i], utxt->buf[i]);
}

/* Note: this function is outdated and must be updated if needed for future use */
void txt_print_undo(Text *text)
{
  int i = 0;
  int op;
  const char *ops;
  int linep, charp;

  dump_buffer(text);

  printf("---< Undo Buffer >---\n");

  printf("UndoPosition is %d\n", utxt->pos);

  while (i <= utxt->pos) {
    op = utxt->buf[i];

    if (op == UNDO_INSERT_1) {
      ops = "Insert ascii ";
    }
    else if (op == UNDO_INSERT_2) {
      ops = "Insert 2 bytes ";
    }
    else if (op == UNDO_INSERT_3) {
      ops = "Insert 3 bytes ";
    }
    else if (op == UNDO_INSERT_4) {
      ops = "Insert unicode ";
    }
    else if (op == UNDO_BS_1) {
      ops = "Backspace for ascii ";
    }
    else if (op == UNDO_BS_2) {
      ops = "Backspace for 2 bytes ";
    }
    else if (op == UNDO_BS_3) {
      ops = "Backspace for 3 bytes ";
    }
    else if (op == UNDO_BS_4) {
      ops = "Backspace for unicode ";
    }
    else if (op == UNDO_DEL_1) {
      ops = "Delete ascii ";
    }
    else if (op == UNDO_DEL_2) {
      ops = "Delete 2 bytes ";
    }
    else if (op == UNDO_DEL_3) {
      ops = "Delete 3 bytes ";
    }
    else if (op == UNDO_DEL_4) {
      ops = "Delete unicode ";
    }
    else if (op == UNDO_DBLOCK) {
      ops = "Delete text block";
    }
    else if (op == UNDO_IBLOCK) {
      ops = "Insert text block";
    }
    else if (op == UNDO_INDENT) {
      ops = "Indent ";
    }
    else if (op == UNDO_UNINDENT) {
      ops = "Unindent ";
    }
    else if (op == UNDO_COMMENT) {
      ops = "Comment ";
    }
    else if (op == UNDO_UNCOMMENT) {
      ops = "Uncomment ";
    }
    else {
      ops = "Unknown";
    }

    printf("Op (%o) at %d = %s", op, i, ops);
    if (op >= UNDO_INSERT_1 && op <= UNDO_DEL_4) {
      i++;
      printf(" - Char is ");
      switch (op) {
        case UNDO_INSERT_1:
        case UNDO_BS_1:
        case UNDO_DEL_1:
          printf("%c", utxt->buf[i]);
          i++;
          break;
        case UNDO_INSERT_2:
        case UNDO_BS_2:
        case UNDO_DEL_2:
          printf("%c%c", utxt->buf[i], utxt->buf[i + 1]);
          i += 2;
          break;
        case UNDO_INSERT_3:
        case UNDO_BS_3:
        case UNDO_DEL_3:
          printf("%c%c%c", utxt->buf[i], utxt->buf[i + 1], utxt->buf[i + 2]);
          i += 3;
          break;
        case UNDO_INSERT_4:
        case UNDO_BS_4:
        case UNDO_DEL_4: {
          unsigned int uc;
          char c[BLI_UTF8_MAX + 1];
          size_t c_len;
          uc = utxt->buf[i];
          i++;
          uc = uc + (utxt->buf[i] << 8);
          i++;
          uc = uc + (utxt->buf[i] << 16);
          i++;
          uc = uc + (utxt->buf[i] << 24);
          i++;
          c_len = BLI_str_utf8_from_unicode(uc, c);
          c[c_len] = '\0';
          puts(c);
          break;
        }
      }
    }
    else if (op == UNDO_DBLOCK || op == UNDO_IBLOCK) {
      i++;

      linep = utxt->buf[i];
      i++;
      linep = linep + (utxt->buf[i] << 8);
      i++;
      linep = linep + (utxt->buf[i] << 16);
      i++;
      linep = linep + (utxt->buf[i] << 24);
      i++;

      printf(" (length %d) <", linep);

      while (linep > 0) {
        putchar(utxt->buf[i]);
        linep--;
        i++;
      }

      linep = utxt->buf[i];
      i++;
      linep = linep + (utxt->buf[i] << 8);
      i++;
      linep = linep + (utxt->buf[i] << 16);
      i++;
      linep = linep + (utxt->buf[i] << 24);
      i++;
      printf("> (%d)", linep);
    }
    else if (op == UNDO_INDENT || op == UNDO_UNINDENT) {
      i++;

      charp = utxt->buf[i];
      i++;
      charp = charp + (utxt->buf[i] << 8);
      i++;

      linep = utxt->buf[i];
      i++;
      linep = linep + (utxt->buf[i] << 8);
      i++;
      linep = linep + (utxt->buf[i] << 16);
      i++;
      linep = linep + (utxt->buf[i] << 24);
      i++;

      printf("to <%d, %d> ", linep, charp);

      charp = utxt->buf[i];
      i++;
      charp = charp + (utxt->buf[i] << 8);
      i++;

      linep = utxt->buf[i];
      i++;
      linep = linep + (utxt->buf[i] << 8);
      i++;
      linep = linep + (utxt->buf[i] << 16);
      i++;
      linep = linep + (utxt->buf[i] << 24);
      i++;

      printf("from <%d, %d>", linep, charp);
    }

    printf(" %d\n", i);
    i++;
  }
}
#endif

static void txt_undo_store_uint16(char *undo_buf, int *undo_pos, unsigned short value)
{
  undo_buf[*undo_pos] = (value)&0xff;
  (*undo_pos)++;
  undo_buf[*undo_pos] = (value >> 8) & 0xff;
  (*undo_pos)++;
}

static void txt_undo_store_uint32(char *undo_buf, int *undo_pos, unsigned int value)
{
  undo_buf[*undo_pos] = (value)&0xff;
  (*undo_pos)++;
  undo_buf[*undo_pos] = (value >> 8) & 0xff;
  (*undo_pos)++;
  undo_buf[*undo_pos] = (value >> 16) & 0xff;
  (*undo_pos)++;
  undo_buf[*undo_pos] = (value >> 24) & 0xff;
  (*undo_pos)++;
}

/* store the cur cursor to the undo buffer (6 bytes)*/
static void txt_undo_store_cur(Text *text, TextUndoBuf *utxt)
{
  txt_undo_store_uint16(utxt->buf, &utxt->pos, text->curc);
  txt_undo_store_uint32(utxt->buf, &utxt->pos, txt_get_span(text->lines.first, text->curl));
}

/* store the sel cursor to the undo buffer (6 bytes) */
static void txt_undo_store_sel(Text *text, TextUndoBuf *utxt)
{
  txt_undo_store_uint16(utxt->buf, &utxt->pos, text->selc);
  txt_undo_store_uint32(utxt->buf, &utxt->pos, txt_get_span(text->lines.first, text->sell));
}

/* store both cursors to the undo buffer (12 bytes) */
static void txt_undo_store_cursors(Text *text, TextUndoBuf *utxt)
{
  txt_undo_store_cur(text, utxt);
  txt_undo_store_sel(text, utxt);
}

/* store an operator along with a block of data */
static void txt_undo_add_blockop(Text *text, TextUndoBuf *utxt, int op, const char *buf)
{
  unsigned int length = strlen(buf);

  if (!max_undo_test(utxt, 2 + 12 + 4 + length + 4 + 1)) {
    return;
  }
  /* 2 bytes */
  utxt->pos++;
  utxt->buf[utxt->pos] = op;
  utxt->pos++;
  /* 12 bytes */
  txt_undo_store_cursors(text, utxt);
  /* 4 bytes */
  txt_undo_store_uint32(utxt->buf, &utxt->pos, length);
  /* 'length' bytes */
  memcpy(utxt->buf + utxt->pos, buf, length);
  utxt->pos += length;
  /* 4 bytes */
  txt_undo_store_uint32(utxt->buf, &utxt->pos, length);
  /* 1 byte */
  utxt->buf[utxt->pos] = op;

  txt_undo_end(text, utxt);
}

/* store a regular operator */
void txt_undo_add_op(Text *text, TextUndoBuf *utxt, int op)
{
  if (!max_undo_test(utxt, 2 + 12 + 1)) {
    return;
  }

  /* 2 bytes */
  utxt->pos++;
  utxt->buf[utxt->pos] = op;
  utxt->pos++;
  /* 12 bytes */
  txt_undo_store_cursors(text, utxt);
  /* 1 byte */
  utxt->buf[utxt->pos] = op;

  txt_undo_end(text, utxt);
}

/* store an operator for a single character */
static void txt_undo_add_charop(Text *text, TextUndoBuf *utxt, int op_start, unsigned int c)
{
  char utf8[BLI_UTF8_MAX];
  size_t i, utf8_size = BLI_str_utf8_from_unicode(c, utf8);

  if (utf8_size < 4 && 0) {
    if (!max_undo_test(utxt, 2 + 6 + utf8_size + 1)) {
      return;
    }
    /* 2 bytes */
    utxt->pos++;
    utxt->buf[utxt->pos] = op_start + utf8_size - 1;
    utxt->pos++;
    /* 6 bytes */
    txt_undo_store_cur(text, utxt);
    /* 'utf8_size' bytes */
    for (i = 0; i < utf8_size; i++) {
      utxt->buf[utxt->pos] = utf8[i];
      utxt->pos++;
    }
    /* 1 byte */
    utxt->buf[utxt->pos] = op_start + utf8_size - 1;
  }
  else {
    if (!max_undo_test(utxt, 2 + 6 + 4 + 1)) {
      return;
    }
    /* 2 bytes */
    utxt->pos++;
    utxt->buf[utxt->pos] = op_start + 3;
    utxt->pos++;
    /* 6 bytes */
    txt_undo_store_cur(text, utxt);
    /* 4 bytes */
    txt_undo_store_uint32(utxt->buf, &utxt->pos, c);
    /* 1 byte */
    utxt->buf[utxt->pos] = op_start + 3;
  }

  txt_undo_end(text, utxt);
}

/* extends Link */
struct LinkInt {
  struct LinkInt *next, *prev;
  int value;
};

/**
 * UnindentLines points to a #ListBase composed of #LinkInt elements, listing the numbers
 * of the lines that should not be indented back.
 */
static void txt_undo_add_unprefix_op(Text *text,
                                     TextUndoBuf *utxt,
                                     char undo_op,
                                     const ListBase *line_index_mask,
                                     const int line_index_mask_len)
{
  struct LinkInt *idata;

  BLI_assert(BLI_listbase_count(line_index_mask) == line_index_mask_len);

  /* OP byte + u32 count + counted u32 line numbers + u32 count + 12-bytes selection + OP byte. */
  if (!max_undo_test(utxt, 2 + 4 + (line_index_mask_len * 4) + 4 + 12 + 1)) {
    return;
  }

  /* 2 bytes */
  utxt->pos++;
  utxt->buf[utxt->pos] = undo_op;
  utxt->pos++;
  /* Adding number of line numbers to read
   * 4 bytes */
  txt_undo_store_uint32(utxt->buf, &utxt->pos, line_index_mask_len);

  /* Adding linenumbers of lines that shall not be indented if undoing.
   * 'line_index_mask_len * 4' bytes */
  for (idata = line_index_mask->first; idata; idata = idata->next) {
    txt_undo_store_uint32(utxt->buf, &utxt->pos, idata->value);
  }

  /* Adding number of line numbers to read again.
   * 4 bytes */
  txt_undo_store_uint32(utxt->buf, &utxt->pos, line_index_mask_len);
  /* Adding current selection.
   * 12 bytes */
  txt_undo_store_cursors(text, utxt);
  /* Closing with OP (same as above).
   * 1 byte */
  utxt->buf[utxt->pos] = undo_op;
  /* Marking as last undo operation */
  txt_undo_end(text, utxt);
}

static unsigned short txt_undo_read_uint16(const char *undo_buf, int *undo_pos)
{
  unsigned short val;
  val = undo_buf[*undo_pos];
  (*undo_pos)--;
  val = (val << 8) + undo_buf[*undo_pos];
  (*undo_pos)--;
  return val;
}

static unsigned int txt_undo_read_uint32(const char *undo_buf, int *undo_pos)
{
  unsigned int val;
  val = undo_buf[*undo_pos];
  (*undo_pos)--;
  val = (val << 8) + undo_buf[*undo_pos];
  (*undo_pos)--;
  val = (val << 8) + undo_buf[*undo_pos];
  (*undo_pos)--;
  val = (val << 8) + undo_buf[*undo_pos];
  (*undo_pos)--;
  return val;
}

/* read the cur cursor from the undo buffer */
static void txt_undo_read_cur(const char *undo_buf,
                              int *undo_pos,
                              unsigned int *curln,
                              unsigned short *curc)
{
  *curln = txt_undo_read_uint32(undo_buf, undo_pos);
  *curc = txt_undo_read_uint16(undo_buf, undo_pos);
}

/* read the sel cursor from the undo buffer */
static void txt_undo_read_sel(const char *undo_buf,
                              int *undo_pos,
                              unsigned int *selln,
                              unsigned short *selc)
{
  *selln = txt_undo_read_uint32(undo_buf, undo_pos);
  *selc = txt_undo_read_uint16(undo_buf, undo_pos);
}

/* read both cursors from the undo buffer */
static void txt_undo_read_cursors(const char *undo_buf,
                                  int *undo_pos,
                                  unsigned int *curln,
                                  unsigned short *curc,
                                  unsigned int *selln,
                                  unsigned short *selc)
{
  txt_undo_read_sel(undo_buf, undo_pos, selln, selc);
  txt_undo_read_cur(undo_buf, undo_pos, curln, curc);
}

static unsigned int txt_undo_read_unicode(const char *undo_buf, int *undo_pos, short bytes)
{
  unsigned int unicode;
  char utf8[BLI_UTF8_MAX + 1];

  switch (bytes) {
    case 1: /* ascii */
      unicode = undo_buf[*undo_pos];
      (*undo_pos)--;
      break;
    case 2: /* 2-byte symbol */
      utf8[2] = '\0';
      utf8[1] = undo_buf[*undo_pos];
      (*undo_pos)--;
      utf8[0] = undo_buf[*undo_pos];
      (*undo_pos)--;
      unicode = BLI_str_utf8_as_unicode(utf8);
      break;
    case 3: /* 3-byte symbol */
      utf8[3] = '\0';
      utf8[2] = undo_buf[*undo_pos];
      (*undo_pos)--;
      utf8[1] = undo_buf[*undo_pos];
      (*undo_pos)--;
      utf8[0] = undo_buf[*undo_pos];
      (*undo_pos)--;
      unicode = BLI_str_utf8_as_unicode(utf8);
      break;
    case 4: /* 32-bit unicode symbol */
      unicode = txt_undo_read_uint32(undo_buf, undo_pos);
      break;
    default:
      /* should never happen */
      BLI_assert(0);
      unicode = 0;
      break;
  }

  return unicode;
}

static unsigned short txt_redo_read_uint16(const char *undo_buf, int *undo_pos)
{
  unsigned short val;
  val = undo_buf[*undo_pos];
  (*undo_pos)++;
  val = val + (undo_buf[*undo_pos] << 8);
  (*undo_pos)++;
  return val;
}

static unsigned int txt_redo_read_uint32(const char *undo_buf, int *undo_pos)
{
  unsigned int val;
  val = undo_buf[*undo_pos];
  (*undo_pos)++;
  val = val + (undo_buf[*undo_pos] << 8);
  (*undo_pos)++;
  val = val + (undo_buf[*undo_pos] << 16);
  (*undo_pos)++;
  val = val + (undo_buf[*undo_pos] << 24);
  (*undo_pos)++;
  return val;
}

/* redo read cur cursor from the undo buffer */
static void txt_redo_read_cur(const char *undo_buf,
                              int *undo_pos,
                              unsigned int *curln,
                              unsigned short *curc)
{
  *curc = txt_redo_read_uint16(undo_buf, undo_pos);
  *curln = txt_redo_read_uint32(undo_buf, undo_pos);
}

/* redo read sel cursor from the undo buffer */
static void txt_redo_read_sel(const char *undo_buf,
                              int *undo_pos,
                              unsigned int *selln,
                              unsigned short *selc)
{
  *selc = txt_redo_read_uint16(undo_buf, undo_pos);
  *selln = txt_redo_read_uint32(undo_buf, undo_pos);
}

/* redo read both cursors from the undo buffer */
static void txt_redo_read_cursors(const char *undo_buf,
                                  int *undo_pos,
                                  unsigned int *curln,
                                  unsigned short *curc,
                                  unsigned int *selln,
                                  unsigned short *selc)
{
  txt_redo_read_cur(undo_buf, undo_pos, curln, curc);
  txt_redo_read_sel(undo_buf, undo_pos, selln, selc);
}

static unsigned int txt_redo_read_unicode(const char *undo_buf, int *undo_pos, short bytes)
{
  unsigned int unicode;
  char utf8[BLI_UTF8_MAX + 1];

  switch (bytes) {
    case 1: /* ascii */
      unicode = undo_buf[*undo_pos];
      (*undo_pos)++;
      break;
    case 2: /* 2-byte symbol */
      utf8[0] = undo_buf[*undo_pos];
      (*undo_pos)++;
      utf8[1] = undo_buf[*undo_pos];
      (*undo_pos)++;
      utf8[2] = '\0';
      unicode = BLI_str_utf8_as_unicode(utf8);
      break;
    case 3: /* 3-byte symbol */
      utf8[0] = undo_buf[*undo_pos];
      (*undo_pos)++;
      utf8[1] = undo_buf[*undo_pos];
      (*undo_pos)++;
      utf8[2] = undo_buf[*undo_pos];
      (*undo_pos)++;
      utf8[3] = '\0';
      unicode = BLI_str_utf8_as_unicode(utf8);
      break;
    case 4: /* 32-bit unicode symbol */
      unicode = txt_redo_read_uint32(undo_buf, undo_pos);
      break;
    default:
      /* should never happen */
      BLI_assert(0);
      unicode = 0;
      break;
  }

  return unicode;
}

void txt_do_undo(Text *text, TextUndoBuf *utxt)
{
  int op = utxt->buf[utxt->pos];
  int prev_flags;
  unsigned int linep;
  unsigned int uni_char;
  unsigned int curln, selln;
  unsigned short curc, selc;
  unsigned short charp;
  char *buf;

  if (utxt->pos < 0) {
    return;
  }

  utxt->pos--;

  undoing = 1;

  switch (op) {
    case UNDO_INSERT_1:
    case UNDO_INSERT_2:
    case UNDO_INSERT_3:
    case UNDO_INSERT_4:
      utxt->pos -= op - UNDO_INSERT_1 + 1;

      /* get and restore the cursors */
      txt_undo_read_cur(utxt->buf, &utxt->pos, &curln, &curc);
      txt_move_to(text, curln, curc, 0);
      txt_move_to(text, curln, curc, 1);

      txt_delete_char(text, utxt);

      utxt->pos--;
      break;

    case UNDO_BS_1:
    case UNDO_BS_2:
    case UNDO_BS_3:
    case UNDO_BS_4:
      charp = op - UNDO_BS_1 + 1;
      uni_char = txt_undo_read_unicode(utxt->buf, &utxt->pos, charp);

      /* get and restore the cursors */
      txt_undo_read_cur(utxt->buf, &utxt->pos, &curln, &curc);
      txt_move_to(text, curln, curc, 0);
      txt_move_to(text, curln, curc, 1);

      txt_add_char(text, utxt, uni_char);

      utxt->pos--;
      break;

    case UNDO_DEL_1:
    case UNDO_DEL_2:
    case UNDO_DEL_3:
    case UNDO_DEL_4:
      charp = op - UNDO_DEL_1 + 1;
      uni_char = txt_undo_read_unicode(utxt->buf, &utxt->pos, charp);

      /* get and restore the cursors */
      txt_undo_read_cur(utxt->buf, &utxt->pos, &curln, &curc);
      txt_move_to(text, curln, curc, 0);
      txt_move_to(text, curln, curc, 1);

      txt_add_char(text, utxt, uni_char);

      txt_move_left(text, 0);

      utxt->pos--;
      break;

    case UNDO_DBLOCK: {
      int i;
      /* length of the string in the buffer */
      linep = txt_undo_read_uint32(utxt->buf, &utxt->pos);

      buf = MEM_mallocN(linep + 1, "dblock buffer");
      for (i = 0; i < linep; i++) {
        buf[(linep - 1) - i] = utxt->buf[utxt->pos];
        utxt->pos--;
      }
      buf[i] = 0;

      /* skip over the length that was stored again */
      utxt->pos -= 4;

      /* Get the cursor positions */
      txt_undo_read_cursors(utxt->buf, &utxt->pos, &curln, &curc, &selln, &selc);

      /* move cur to location that needs buff inserted */
      txt_move_to(text, curln, curc, 0);

      txt_insert_buf(text, utxt, buf);
      MEM_freeN(buf);

      /* restore the cursors */
      txt_move_to(text, curln, curc, 0);
      txt_move_to(text, selln, selc, 1);

      utxt->pos--;

      break;
    }
    case UNDO_IBLOCK: {
      int i;
      /* length of the string in the buffer */
      linep = txt_undo_read_uint32(utxt->buf, &utxt->pos);

      /* txt_backspace_char removes utf8-characters, not bytes */
      buf = MEM_mallocN(linep + 1, "iblock buffer");
      for (i = 0; i < linep; i++) {
        buf[(linep - 1) - i] = utxt->buf[utxt->pos];
        utxt->pos--;
      }
      buf[i] = 0;
      linep = BLI_strlen_utf8(buf);
      MEM_freeN(buf);

      /* skip over the length that was stored again */
      utxt->pos -= 4;

      /* get and restore the cursors */
      txt_undo_read_cursors(utxt->buf, &utxt->pos, &curln, &curc, &selln, &selc);

      txt_move_to(text, curln, curc, 0);
      txt_move_to(text, selln, selc, 1);

      if ((curln == selln) && (curc == selc)) {
        /* disable tabs to spaces since moving right may involve skipping multiple spaces */
        prev_flags = text->flags;
        text->flags &= ~TXT_TABSTOSPACES;

        for (i = 0; i < linep; i++) {
          txt_move_right(text, 1);
        }

        text->flags = prev_flags;
      }

      txt_delete_selected(text, utxt);

      utxt->pos--;
      break;
    }
    case UNDO_INDENT:
    case UNDO_COMMENT:
    case UNDO_DUPLICATE:
    case UNDO_MOVE_LINES_UP:
    case UNDO_MOVE_LINES_DOWN:
      /* get and restore the cursors */
      txt_undo_read_cursors(utxt->buf, &utxt->pos, &curln, &curc, &selln, &selc);
      txt_move_to(text, curln, curc, 0);
      txt_move_to(text, selln, selc, 1);

      if (op == UNDO_INDENT) {
        txt_unindent(text, utxt);
      }
      else if (op == UNDO_COMMENT) {
        txt_uncomment(text, utxt);
      }
      else if (op == UNDO_DUPLICATE) {
        txt_delete_line(text, text->curl->next);
      }
      else if (op == UNDO_MOVE_LINES_UP) {
        txt_move_lines(text, utxt, TXT_MOVE_LINE_DOWN);
      }
      else if (op == UNDO_MOVE_LINES_DOWN) {
        txt_move_lines(text, utxt, TXT_MOVE_LINE_UP);
      }

      utxt->pos--;
      break;
    case UNDO_UNINDENT:
    case UNDO_UNCOMMENT: {
      void (*txt_prefix_fn)(Text *, TextUndoBuf *);
      void (*txt_unprefix_fn)(Text *, TextUndoBuf *);
      int count;
      int i;
      /* Get and restore the cursors */
      txt_undo_read_cursors(utxt->buf, &utxt->pos, &curln, &curc, &selln, &selc);
      txt_move_to(text, curln, curc, 0);
      txt_move_to(text, selln, selc, 1);

      /* Un-unindent */
      if (op == UNDO_UNINDENT) {
        txt_prefix_fn = txt_indent;
        txt_unprefix_fn = txt_unindent;
      }
      else {
        txt_prefix_fn = txt_comment;
        txt_unprefix_fn = txt_uncomment;
      }

      txt_prefix_fn(text, utxt);

      /* Get the count */
      count = txt_undo_read_uint32(utxt->buf, &utxt->pos);
      /* Iterate! */
      txt_pop_sel(text);

      for (i = 0; i < count; i++) {
        txt_move_to(text, txt_undo_read_uint32(utxt->buf, &utxt->pos), 0, 0);
        /* Un-un-unindent/comment */
        txt_unprefix_fn(text, utxt);
      }
      /* Restore selection */
      txt_move_to(text, curln, curc, 0);
      txt_move_to(text, selln, selc, 1);
      /* Jumo over count */
      txt_undo_read_uint32(utxt->buf, &utxt->pos);
      /* Jump over closing OP byte */
      utxt->pos--;
      break;
    }
    default:
      //XXX error("Undo buffer error - resetting");
      utxt->pos = -1;

      break;
  }

  undoing = 0;
}

void txt_do_redo(Text *text, TextUndoBuf *utxt)
{
  char op;
  char *buf;
  unsigned int linep;
  unsigned short charp;
  unsigned int uni_uchar;
  unsigned int curln, selln;
  unsigned short curc, selc;

  utxt->pos++;
  op = utxt->buf[utxt->pos];

  if (!op) {
    utxt->pos--;
    return;
  }

  undoing = 1;

  switch (op) {
    case UNDO_INSERT_1:
    case UNDO_INSERT_2:
    case UNDO_INSERT_3:
    case UNDO_INSERT_4:
      utxt->pos++;

      /* get and restore the cursors */
      txt_redo_read_cur(utxt->buf, &utxt->pos, &curln, &curc);
      txt_move_to(text, curln, curc, 0);
      txt_move_to(text, curln, curc, 1);

      charp = op - UNDO_INSERT_1 + 1;
      uni_uchar = txt_redo_read_unicode(utxt->buf, &utxt->pos, charp);

      txt_add_char(text, utxt, uni_uchar);
      break;

    case UNDO_BS_1:
    case UNDO_BS_2:
    case UNDO_BS_3:
    case UNDO_BS_4:
      utxt->pos++;

      /* get and restore the cursors */
      txt_redo_read_cur(utxt->buf, &utxt->pos, &curln, &curc);
      txt_move_to(text, curln, curc, 0);
      txt_move_to(text, curln, curc, 1);

      utxt->pos += op - UNDO_BS_1 + 1;

      /* move right so we backspace the correct char */
      txt_move_right(text, 0);
      txt_backspace_char(text, utxt);

      break;

    case UNDO_DEL_1:
    case UNDO_DEL_2:
    case UNDO_DEL_3:
    case UNDO_DEL_4:
      utxt->pos++;

      /* get and restore the cursors */
      txt_redo_read_cur(utxt->buf, &utxt->pos, &curln, &curc);
      txt_move_to(text, curln, curc, 0);
      txt_move_to(text, curln, curc, 1);

      utxt->pos += op - UNDO_DEL_1 + 1;

      txt_delete_char(text, utxt);

      break;

    case UNDO_DBLOCK:
      utxt->pos++;

      /* get and restore the cursors */
      txt_redo_read_cursors(utxt->buf, &utxt->pos, &curln, &curc, &selln, &selc);
      txt_move_to(text, curln, curc, 0);
      txt_move_to(text, selln, selc, 1);

      /* length of the block */
      linep = txt_redo_read_uint32(utxt->buf, &utxt->pos);

      utxt->pos += linep;

      /* skip over the length that was stored again */
      utxt->pos += 4;

      txt_delete_sel(text, utxt);

      break;

    case UNDO_IBLOCK:
      utxt->pos++;

      /* get and restore the cursors */
      txt_redo_read_cursors(utxt->buf, &utxt->pos, &curln, &curc, &selln, &selc);
      txt_move_to(text, curln, curc, 0);
      txt_move_to(text, curln, curc, 1);

      /* length of the block */
      linep = txt_redo_read_uint32(utxt->buf, &utxt->pos);

      buf = MEM_mallocN(linep + 1, "iblock buffer");
      memcpy(buf, &utxt->buf[utxt->pos], linep);
      utxt->pos += linep;
      buf[linep] = 0;

      txt_insert_buf(text, utxt, buf);
      MEM_freeN(buf);

      /* skip over the length that was stored again */
      utxt->pos += 4;

      break;

    case UNDO_INDENT:
    case UNDO_COMMENT:
    case UNDO_UNCOMMENT:
    case UNDO_DUPLICATE:
    case UNDO_MOVE_LINES_UP:
    case UNDO_MOVE_LINES_DOWN:
      utxt->pos++;

      /* get and restore the cursors */
      txt_redo_read_cursors(utxt->buf, &utxt->pos, &curln, &curc, &selln, &selc);
      txt_move_to(text, curln, curc, 0);
      txt_move_to(text, selln, selc, 1);

      if (op == UNDO_INDENT) {
        txt_indent(text, utxt);
      }
      else if (op == UNDO_COMMENT) {
        txt_comment(text, utxt);
      }
      else if (op == UNDO_UNCOMMENT) {
        txt_uncomment(text, utxt);
      }
      else if (op == UNDO_DUPLICATE) {
        txt_duplicate_line(text, utxt);
      }
      else if (op == UNDO_MOVE_LINES_UP) {
        /* offset the cursor by + 1 */
        txt_move_to(text, curln + 1, curc, 0);
        txt_move_to(text, selln + 1, selc, 1);

        txt_move_lines(text, utxt, TXT_MOVE_LINE_UP);
      }
      else if (op == UNDO_MOVE_LINES_DOWN) {
        /* offset the cursor by - 1 */
        txt_move_to(text, curln - 1, curc, 0);
        txt_move_to(text, selln - 1, selc, 1);

        txt_move_lines(text, utxt, TXT_MOVE_LINE_DOWN);
      }

      /* re-restore the cursors since they got moved when redoing */
      txt_move_to(text, curln, curc, 0);
      txt_move_to(text, selln, selc, 1);

      break;
    case UNDO_UNINDENT: {
      int count;
      int i;

      utxt->pos++;
      /* Scan all the stuff described in txt_undo_add_unindent_op */
      count = txt_redo_read_uint32(utxt->buf, &utxt->pos);
      for (i = 0; i < count; i++) {
        txt_redo_read_uint32(utxt->buf, &utxt->pos);
      }
      /* Count again */
      txt_redo_read_uint32(utxt->buf, &utxt->pos);
      /* Get the selection and re-unindent */
      txt_redo_read_cursors(utxt->buf, &utxt->pos, &curln, &curc, &selln, &selc);
      txt_move_to(text, curln, curc, 0);
      txt_move_to(text, selln, selc, 1);
      txt_unindent(text, utxt);
      break;
    }
    default:
      //XXX error("Undo buffer error - resetting");
      utxt->pos = -1;

      break;
  }

  undoing = 0;
}

/**************************/
/* Line editing functions */
/**************************/

void txt_split_curline(Text *text, TextUndoBuf *utxt)
{
  TextLine *ins;
  char *left, *right;

  if (!text->curl) {
    return;
  }

  txt_delete_sel(text, utxt);

  if (!undoing) {
    txt_undo_add_charop(text, utxt, UNDO_INSERT_1, '\n');
  }

  /* Make the two half strings */

  left = MEM_mallocN(text->curc + 1, "textline_string");
  if (text->curc) {
    memcpy(left, text->curl->line, text->curc);
  }
  left[text->curc] = 0;

  right = MEM_mallocN(text->curl->len - text->curc + 1, "textline_string");
  memcpy(right, text->curl->line + text->curc, text->curl->len - text->curc + 1);

  MEM_freeN(text->curl->line);
  if (text->curl->format) {
    MEM_freeN(text->curl->format);
  }

  /* Make the new TextLine */

  ins = MEM_mallocN(sizeof(TextLine), "textline");
  ins->line = left;
  ins->format = NULL;
  ins->len = text->curc;

  text->curl->line = right;
  text->curl->format = NULL;
  text->curl->len = text->curl->len - text->curc;

  BLI_insertlinkbefore(&text->lines, text->curl, ins);

  text->curc = 0;

  txt_make_dirty(text);
  txt_clean_text(text);

  txt_pop_sel(text);
}

static void txt_delete_line(Text *text, TextLine *line)
{
  if (!text->curl) {
    return;
  }

  BLI_remlink(&text->lines, line);

  if (line->line) {
    MEM_freeN(line->line);
  }
  if (line->format) {
    MEM_freeN(line->format);
  }

  MEM_freeN(line);

  txt_make_dirty(text);
  txt_clean_text(text);
}

static void txt_combine_lines(Text *text, TextLine *linea, TextLine *lineb)
{
  char *tmp, *s;

  if (!linea || !lineb) {
    return;
  }

  tmp = MEM_mallocN(linea->len + lineb->len + 1, "textline_string");

  s = tmp;
  s += BLI_strcpy_rlen(s, linea->line);
  s += BLI_strcpy_rlen(s, lineb->line);
  (void)s;

  make_new_line(linea, tmp);

  txt_delete_line(text, lineb);

  txt_make_dirty(text);
  txt_clean_text(text);
}

void txt_duplicate_line(Text *text, TextUndoBuf *utxt)
{
  TextLine *textline;

  if (!text->curl) {
    return;
  }

  if (text->curl == text->sell) {
    textline = txt_new_line(text->curl->line);
    BLI_insertlinkafter(&text->lines, text->curl, textline);

    txt_make_dirty(text);
    txt_clean_text(text);

    if (!undoing) {
      txt_undo_add_op(text, utxt, UNDO_DUPLICATE);
    }
  }
}

void txt_delete_char(Text *text, TextUndoBuf *utxt)
{
  unsigned int c = '\n';

  if (!text->curl) {
    return;
  }

  if (txt_has_sel(text)) { /* deleting a selection */
    txt_delete_sel(text, utxt);
    txt_make_dirty(text);
    return;
  }
  else if (text->curc == text->curl->len) { /* Appending two lines */
    if (text->curl->next) {
      txt_combine_lines(text, text->curl, text->curl->next);
      txt_pop_sel(text);
    }
    else {
      return;
    }
  }
  else { /* Just deleting a char */
    size_t c_len = 0;
    c = BLI_str_utf8_as_unicode_and_size(text->curl->line + text->curc, &c_len);

    memmove(text->curl->line + text->curc,
            text->curl->line + text->curc + c_len,
            text->curl->len - text->curc - c_len + 1);

    text->curl->len -= c_len;

    txt_pop_sel(text);
  }

  txt_make_dirty(text);
  txt_clean_text(text);

  if (!undoing) {
    txt_undo_add_charop(text, utxt, UNDO_DEL_1, c);
  }
}

void txt_delete_word(Text *text, TextUndoBuf *utxt)
{
  txt_jump_right(text, true, true);
  txt_delete_sel(text, utxt);
  txt_make_dirty(text);
}

void txt_backspace_char(Text *text, TextUndoBuf *utxt)
{
  unsigned int c = '\n';

  if (!text->curl) {
    return;
  }

  if (txt_has_sel(text)) { /* deleting a selection */
    txt_delete_sel(text, utxt);
    txt_make_dirty(text);
    return;
  }
  else if (text->curc == 0) { /* Appending two lines */
    if (!text->curl->prev) {
      return;
    }

    text->curl = text->curl->prev;
    text->curc = text->curl->len;

    txt_combine_lines(text, text->curl, text->curl->next);
    txt_pop_sel(text);
  }
  else { /* Just backspacing a char */
    size_t c_len = 0;
    const char *prev = BLI_str_prev_char_utf8(text->curl->line + text->curc);
    c = BLI_str_utf8_as_unicode_and_size(prev, &c_len);

    /* source and destination overlap, don't use memcpy() */
    memmove(text->curl->line + text->curc - c_len,
            text->curl->line + text->curc,
            text->curl->len - text->curc + 1);

    text->curl->len -= c_len;
    text->curc -= c_len;

    txt_pop_sel(text);
  }

  txt_make_dirty(text);
  txt_clean_text(text);

  if (!undoing) {
    txt_undo_add_charop(text, utxt, UNDO_BS_1, c);
  }
}

void txt_backspace_word(Text *text, TextUndoBuf *utxt)
{
  txt_jump_left(text, true, true);
  txt_delete_sel(text, utxt);
  txt_make_dirty(text);
}

/* Max spaces to replace a tab with, currently hardcoded to TXT_TABSIZE = 4.
 * Used by txt_convert_tab_to_spaces, indent and unindent.
 * Remember to change this string according to max tab size */
static char tab_to_spaces[] = "    ";

static void txt_convert_tab_to_spaces(Text *text, TextUndoBuf *utxt)
{
  /* sb aims to pad adjust the tab-width needed so that the right number of spaces
   * is added so that the indention of the line is the right width (i.e. aligned
   * to multiples of TXT_TABSIZE)
   */
  const char *sb = &tab_to_spaces[text->curc % TXT_TABSIZE];
  txt_insert_buf(text, utxt, sb);
}

static bool txt_add_char_intern(Text *text, TextUndoBuf *utxt, unsigned int add, bool replace_tabs)
{
  char *tmp, ch[BLI_UTF8_MAX];
  size_t add_len;

  if (!text->curl) {
    return 0;
  }

  if (add == '\n') {
    txt_split_curline(text, utxt);
    return true;
  }

  /* insert spaces rather than tabs */
  if (add == '\t' && replace_tabs) {
    txt_convert_tab_to_spaces(text, utxt);
    return true;
  }

  txt_delete_sel(text, utxt);

  if (!undoing) {
    txt_undo_add_charop(text, utxt, UNDO_INSERT_1, add);
  }

  add_len = BLI_str_utf8_from_unicode(add, ch);

  tmp = MEM_mallocN(text->curl->len + add_len + 1, "textline_string");

  memcpy(tmp, text->curl->line, text->curc);
  memcpy(tmp + text->curc, ch, add_len);
  memcpy(
      tmp + text->curc + add_len, text->curl->line + text->curc, text->curl->len - text->curc + 1);

  make_new_line(text->curl, tmp);

  text->curc += add_len;

  txt_pop_sel(text);

  txt_make_dirty(text);
  txt_clean_text(text);

  return 1;
}

bool txt_add_char(Text *text, TextUndoBuf *utxt, unsigned int add)
{
  return txt_add_char_intern(text, utxt, add, (text->flags & TXT_TABSTOSPACES) != 0);
}

bool txt_add_raw_char(Text *text, TextUndoBuf *utxt, unsigned int add)
{
  return txt_add_char_intern(text, utxt, add, 0);
}

void txt_delete_selected(Text *text, TextUndoBuf *utxt)
{
  txt_delete_sel(text, utxt);
  txt_make_dirty(text);
}

bool txt_replace_char(Text *text, TextUndoBuf *utxt, unsigned int add)
{
  unsigned int del;
  size_t del_size = 0, add_size;
  char ch[BLI_UTF8_MAX];

  if (!text->curl) {
    return false;
  }

  /* If text is selected or we're at the end of the line just use txt_add_char */
  if (text->curc == text->curl->len || txt_has_sel(text) || add == '\n') {
    return txt_add_char(text, utxt, add);
  }

  del = BLI_str_utf8_as_unicode_and_size(text->curl->line + text->curc, &del_size);
  add_size = BLI_str_utf8_from_unicode(add, ch);

  if (add_size > del_size) {
    char *tmp = MEM_mallocN(text->curl->len + add_size - del_size + 1, "textline_string");
    memcpy(tmp, text->curl->line, text->curc);
    memcpy(tmp + text->curc + add_size,
           text->curl->line + text->curc + del_size,
           text->curl->len - text->curc - del_size + 1);
    MEM_freeN(text->curl->line);
    text->curl->line = tmp;
  }
  else if (add_size < del_size) {
    char *tmp = text->curl->line;
    memmove(tmp + text->curc + add_size,
            tmp + text->curc + del_size,
            text->curl->len - text->curc - del_size + 1);
  }

  memcpy(text->curl->line + text->curc, ch, add_size);
  text->curc += add_size;
  text->curl->len += add_size - del_size;

  txt_pop_sel(text);
  txt_make_dirty(text);
  txt_clean_text(text);

  /* Should probably create a new op for this */
  if (!undoing) {
    txt_undo_add_charop(text, utxt, UNDO_INSERT_1, add);
    text->curc -= add_size;
    txt_pop_sel(text);
    txt_undo_add_charop(text, utxt, UNDO_DEL_1, del);
    text->curc += add_size;
    txt_pop_sel(text);
  }
  return true;
}

/**
 * Generic prefix operation, use for comment & indent.
 *
 * \note caller must handle undo.
 */
static void txt_select_prefix(Text *text, const char *add)
{
  int len, num, curc_old;
  char *tmp;

  const int indentlen = strlen(add);

  BLI_assert(!ELEM(NULL, text->curl, text->sell));

  curc_old = text->curc;

  num = 0;
  while (true) {

    /* don't indent blank lines */
    if (text->curl->len != 0) {
      tmp = MEM_mallocN(text->curl->len + indentlen + 1, "textline_string");

      text->curc = 0;
      if (text->curc) {
        memcpy(tmp, text->curl->line, text->curc); /* XXX never true, check prev line */
      }
      memcpy(tmp + text->curc, add, indentlen);

      len = text->curl->len - text->curc;
      if (len > 0) {
        memcpy(tmp + text->curc + indentlen, text->curl->line + text->curc, len);
      }
      tmp[text->curl->len + indentlen] = 0;

      make_new_line(text->curl, tmp);

      text->curc += indentlen;

      txt_make_dirty(text);
      txt_clean_text(text);
    }

    if (text->curl == text->sell) {
      text->selc += indentlen;
      break;
    }
    else {
      text->curl = text->curl->next;
      num++;
    }
  }
  if (!curc_old) {
    text->curc = 0;
  }
  else {
    text->curc = curc_old + indentlen;
  }

  while (num > 0) {
    text->curl = text->curl->prev;
    num--;
  }

  /* caller must handle undo */
}

/**
 * Generic un-prefix operation, use for comment & indent.
 *
 * \param r_line_index_mask: List of lines that are already at indent level 0,
 * to store them later into the undo buffer.
 *
 * \note caller must handle undo.
 */
static void txt_select_unprefix(Text *text,
                                const char *remove,
                                ListBase *r_line_index_mask,
                                int *r_line_index_mask_len)
{
  int num = 0;
  const int indentlen = strlen(remove);
  bool unindented_first = false;

  int curl_span_init = 0;

  BLI_assert(!ELEM(NULL, text->curl, text->sell));

  BLI_listbase_clear(r_line_index_mask);
  *r_line_index_mask_len = 0;

  if (!undoing) {
    curl_span_init = txt_get_span(text->lines.first, text->curl);
  }

  while (true) {
    bool changed = false;
    if (STREQLEN(text->curl->line, remove, indentlen)) {
      if (num == 0) {
        unindented_first = true;
      }
      text->curl->len -= indentlen;
      memmove(text->curl->line, text->curl->line + indentlen, text->curl->len + 1);
      changed = true;
    }
    else {
      if (!undoing) {
        /* Create list element for 0 indent line */
        struct LinkInt *idata = MEM_mallocN(sizeof(struct LinkInt), __func__);
        idata->value = curl_span_init + num;
        BLI_assert(idata->value == txt_get_span(text->lines.first, text->curl));
        BLI_addtail(r_line_index_mask, idata);
        (*r_line_index_mask_len) += 1;
      }
    }

    txt_make_dirty(text);
    txt_clean_text(text);

    if (text->curl == text->sell) {
      if (changed) {
        text->selc = MAX2(text->selc - indentlen, 0);
      }
      break;
    }
    else {
      text->curl = text->curl->next;
      num++;
    }
  }

  if (unindented_first) {
    text->curc = MAX2(text->curc - indentlen, 0);
  }

  while (num > 0) {
    text->curl = text->curl->prev;
    num--;
  }

  /* caller must handle undo */
}

void txt_comment(Text *text, TextUndoBuf *utxt)
{
  const char *prefix = "#";

  if (ELEM(NULL, text->curl, text->sell)) {
    return;
  }

  txt_select_prefix(text, prefix);

  if (!undoing) {
    txt_undo_add_op(text, utxt, UNDO_COMMENT);
  }
}

void txt_uncomment(Text *text, TextUndoBuf *utxt)
{
  const char *prefix = "#";
  ListBase line_index_mask;
  int line_index_mask_len;

  if (ELEM(NULL, text->curl, text->sell)) {
    return;
  }

  txt_select_unprefix(text, prefix, &line_index_mask, &line_index_mask_len);

  if (!undoing) {
    txt_undo_add_unprefix_op(text, utxt, UNDO_UNCOMMENT, &line_index_mask, line_index_mask_len);
  }

  BLI_freelistN(&line_index_mask);
}

void txt_indent(Text *text, TextUndoBuf *utxt)
{
  const char *prefix = (text->flags & TXT_TABSTOSPACES) ? tab_to_spaces : "\t";

  if (ELEM(NULL, text->curl, text->sell)) {
    return;
  }

  txt_select_prefix(text, prefix);

  if (!undoing) {
    txt_undo_add_op(text, utxt, UNDO_INDENT);
  }
}

void txt_unindent(Text *text, TextUndoBuf *utxt)
{
  const char *prefix = (text->flags & TXT_TABSTOSPACES) ? tab_to_spaces : "\t";
  ListBase line_index_mask;
  int line_index_mask_len;

  if (ELEM(NULL, text->curl, text->sell)) {
    return;
  }

  txt_select_unprefix(text, prefix, &line_index_mask, &line_index_mask_len);

  if (!undoing) {
    txt_undo_add_unprefix_op(text, utxt, UNDO_UNINDENT, &line_index_mask, line_index_mask_len);
  }

  BLI_freelistN(&line_index_mask);
}

void txt_move_lines(struct Text *text, TextUndoBuf *utxt, const int direction)
{
  TextLine *line_other;

  BLI_assert(ELEM(direction, TXT_MOVE_LINE_UP, TXT_MOVE_LINE_DOWN));

  if (!text->curl || !text->sell) {
    return;
  }

  txt_order_cursors(text, false);

  line_other = (direction == TXT_MOVE_LINE_DOWN) ? text->sell->next : text->curl->prev;

  if (!line_other) {
    return;
  }

  BLI_remlink(&text->lines, line_other);

  if (direction == TXT_MOVE_LINE_DOWN) {
    BLI_insertlinkbefore(&text->lines, text->curl, line_other);
  }
  else {
    BLI_insertlinkafter(&text->lines, text->sell, line_other);
  }

  txt_make_dirty(text);
  txt_clean_text(text);

  if (!undoing) {
    txt_undo_add_op(
        text, utxt, (direction == TXT_MOVE_LINE_DOWN) ? UNDO_MOVE_LINES_DOWN : UNDO_MOVE_LINES_UP);
  }
}

int txt_setcurr_tab_spaces(Text *text, int space)
{
  int i = 0;
  int test = 0;
  const char *word = ":";
  const char *comm = "#";
  const char indent = (text->flags & TXT_TABSTOSPACES) ? ' ' : '\t';
  static const char *back_words[] = {"return", "break", "continue", "pass", "yield", NULL};

  if (!text->curl) {
    return 0;
  }

  while (text->curl->line[i] == indent) {
    //we only count those tabs/spaces that are before any text or before the curs;
    if (i == text->curc) {
      return i;
    }
    else {
      i++;
    }
  }
  if (strstr(text->curl->line, word)) {
    /* if we find a ':' on this line, then add a tab but not if it is:
     * 1) in a comment
     * 2) within an identifier
     * 3) after the cursor (text->curc), i.e. when creating space before a function def [#25414]
     */
    int a;
    bool is_indent = false;
    for (a = 0; (a < text->curc) && (text->curl->line[a] != '\0'); a++) {
      char ch = text->curl->line[a];
      if (ch == '#') {
        break;
      }
      else if (ch == ':') {
        is_indent = 1;
      }
      else if (ch != ' ' && ch != '\t') {
        is_indent = 0;
      }
    }
    if (is_indent) {
      i += space;
    }
  }

  for (test = 0; back_words[test]; test++) {
    /* if there are these key words then remove a tab because we are done with the block */
    if (strstr(text->curl->line, back_words[test]) && i > 0) {
      if (strcspn(text->curl->line, back_words[test]) < strcspn(text->curl->line, comm)) {
        i -= space;
      }
    }
  }
  return i;
}

/*******************************/
/* Character utility functions */
/*******************************/

int text_check_bracket(const char ch)
{
  int a;
  char opens[] = "([{";
  char close[] = ")]}";

  for (a = 0; a < (sizeof(opens) - 1); a++) {
    if (ch == opens[a]) {
      return a + 1;
    }
    else if (ch == close[a]) {
      return -(a + 1);
    }
  }
  return 0;
}

/* TODO, have a function for operators -
 * http://docs.python.org/py3k/reference/lexical_analysis.html#operators */
bool text_check_delim(const char ch)
{
  int a;
  char delims[] = "():\"\' ~!%^&*-+=[]{};/<>|.#\t,@";

  for (a = 0; a < (sizeof(delims) - 1); a++) {
    if (ch == delims[a]) {
      return true;
    }
  }
  return false;
}

bool text_check_digit(const char ch)
{
  if (ch < '0') {
    return false;
  }
  if (ch <= '9') {
    return true;
  }
  return false;
}

bool text_check_identifier(const char ch)
{
  if (ch < '0') {
    return false;
  }
  if (ch <= '9') {
    return true;
  }
  if (ch < 'A') {
    return false;
  }
  if (ch <= 'Z' || ch == '_') {
    return true;
  }
  if (ch < 'a') {
    return false;
  }
  if (ch <= 'z') {
    return true;
  }
  return false;
}

bool text_check_identifier_nodigit(const char ch)
{
  if (ch <= '9') {
    return false;
  }
  if (ch < 'A') {
    return false;
  }
  if (ch <= 'Z' || ch == '_') {
    return true;
  }
  if (ch < 'a') {
    return false;
  }
  if (ch <= 'z') {
    return true;
  }
  return false;
}

#ifndef WITH_PYTHON
int text_check_identifier_unicode(const unsigned int ch)
{
  return (ch < 255 && text_check_identifier((unsigned int)ch));
}

int text_check_identifier_nodigit_unicode(const unsigned int ch)
{
  return (ch < 255 && text_check_identifier_nodigit((char)ch));
}
#endif /* WITH_PYTHON */

bool text_check_whitespace(const char ch)
{
  if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
    return true;
  }
  return false;
}

int text_find_identifier_start(const char *str, int i)
{
  if (UNLIKELY(i <= 0)) {
    return 0;
  }

  while (i--) {
    if (!text_check_identifier(str[i])) {
      break;
    }
  }
  i++;
  return i;
}
