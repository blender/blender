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
#include <sys/stat.h>
#include <sys/types.h>
#include <wctype.h>

#include "MEM_guardedalloc.h"

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_string_cursor_utf8.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_constraint_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_text_types.h"
#include "DNA_userdef_types.h"

#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_text.h"

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
 *
 * The st->top determines at what line the top of the text is displayed.
 * If the user moves the cursor the st containing that cursor should
 * be popped ... other st's retain their own top location.
 */

/* -------------------------------------------------------------------- */
/** \name Prototypes
 * \{ */

static void txt_pop_first(Text *text);
static void txt_pop_last(Text *text);
static void txt_delete_line(Text *text, TextLine *line);
static void txt_delete_sel(Text *text);
static void txt_make_dirty(Text *text);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Text Data-Block
 * \{ */

static void text_init_data(ID *id)
{
  Text *text = (Text *)id;
  TextLine *tmp;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(text, id));

  text->name = NULL;

  text->nlines = 1;
  text->flags = TXT_ISDIRTY | TXT_ISMEM;
  if ((U.flag & USER_TXT_TABSTOSPACES_DISABLE) == 0) {
    text->flags |= TXT_TABSTOSPACES;
  }

  BLI_listbase_clear(&text->lines);

  tmp = (TextLine *)MEM_mallocN(sizeof(TextLine), "textline");
  tmp->line = (char *)MEM_mallocN(1, "textline_string");
  tmp->format = NULL;

  tmp->line[0] = 0;
  tmp->len = 0;

  tmp->next = NULL;
  tmp->prev = NULL;

  BLI_addhead(&text->lines, tmp);

  text->curl = text->lines.first;
  text->curc = 0;
  text->sell = text->lines.first;
  text->selc = 0;
}

/**
 * Only copy internal data of Text ID from source
 * to already allocated/initialized destination.
 * You probably never want to use that directly,
 * use #BKE_id_copy or #BKE_id_copy_ex for typical needs.
 *
 * WARNING! This function will not handle ID user count!
 *
 * \param flag: Copying options (see BKE_lib_id.h's LIB_ID_COPY_... flags for more).
 */
static void text_copy_data(Main *UNUSED(bmain),
                           ID *id_dst,
                           const ID *id_src,
                           const int UNUSED(flag))
{
  Text *text_dst = (Text *)id_dst;
  const Text *text_src = (Text *)id_src;

  /* File name can be NULL. */
  if (text_src->name) {
    text_dst->name = BLI_strdup(text_src->name);
  }

  text_dst->flags |= TXT_ISDIRTY;

  BLI_listbase_clear(&text_dst->lines);
  text_dst->curl = text_dst->sell = NULL;
  text_dst->compiled = NULL;

  /* Walk down, reconstructing. */
  LISTBASE_FOREACH (TextLine *, line_src, &text_src->lines) {
    TextLine *line_dst = MEM_mallocN(sizeof(*line_dst), __func__);

    line_dst->line = BLI_strdup(line_src->line);
    line_dst->format = NULL;
    line_dst->len = line_src->len;

    BLI_addtail(&text_dst->lines, line_dst);
  }

  text_dst->curl = text_dst->sell = text_dst->lines.first;
  text_dst->curc = text_dst->selc = 0;
}

/** Free (or release) any data used by this text (does not free the text itself). */
static void text_free_data(ID *id)
{
  /* No animdata here. */
  Text *text = (Text *)id;

  BKE_text_free_lines(text);

  MEM_SAFE_FREE(text->name);
#ifdef WITH_PYTHON
  BPY_text_free_code(text);
#endif
}

IDTypeInfo IDType_ID_TXT = {
    .id_code = ID_TXT,
    .id_filter = FILTER_ID_TXT,
    .main_listbase_index = INDEX_ID_TXT,
    .struct_size = sizeof(Text),
    .name = "Text",
    .name_plural = "texts",
    .translation_context = BLT_I18NCONTEXT_ID_TEXT,
    .flags = 0,

    .init_data = text_init_data,
    .copy_data = text_copy_data,
    .free_data = text_free_data,
    .make_local = NULL,
    .foreach_id = NULL,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Text Add, Free, Validation
 * \{ */

/**
 * \note caller must handle `compiled` member.
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

Text *BKE_text_add(Main *bmain, const char *name)
{
  Text *ta;

  ta = BKE_libblock_alloc(bmain, ID_TXT, name, 0);
  /* Texts always have 'real' user (see also read code). */
  id_us_ensure_real(&ta->id);

  text_init_data(&ta->id);

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
  BLI_path_abs(filepath_abs, ID_BLEND_PATH_FROM_GLOBAL(&text->id));

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
    return NULL;
  }

  ta = BKE_libblock_alloc(bmain, ID_TXT, BLI_path_basename(filepath_abs), 0);
  /* Texts always have 'real' user (see also read code). */
  id_us_ensure_real(&ta->id);

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

Text *BKE_text_copy(Main *bmain, const Text *ta)
{
  Text *ta_copy;
  BKE_id_copy(bmain, &ta->id, (ID **)&ta_copy);
  return ta_copy;
}

void BKE_text_clear(Text *text) /* called directly from rna */
{
  txt_sel_all(text);
  txt_delete_sel(text);
  txt_make_dirty(text);
}

void BKE_text_write(Text *text, const char *str) /* called directly from rna */
{
  txt_insert_buf(text, str);
  txt_move_eof(text, 0);
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
  BLI_path_abs(file, ID_BLEND_PATH_FROM_GLOBAL(&text->id));

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
  BLI_path_abs(file, ID_BLEND_PATH_FROM_GLOBAL(&text->id));

  if (!BLI_exists(file)) {
    return;
  }

  result = BLI_stat(file, &st);

  if (result == -1 || (st.st_mode & S_IFMT) != S_IFREG) {
    return;
  }

  text->mtime = st.st_mtime;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Editing Utility Functions
 * \{ */

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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cursor Utility Functions
 * \{ */

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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cursor Movement Functions
 * \{ */

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
    int column = BLI_str_utf8_offset_to_column((*linep)->line, *charp);
    *linep = (*linep)->prev;
    *charp = BLI_str_utf8_offset_from_column((*linep)->line, column);
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
    int column = BLI_str_utf8_offset_to_column((*linep)->line, *charp);
    *linep = (*linep)->next;
    *charp = BLI_str_utf8_offset_from_column((*linep)->line, column);
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Text Selection Functions
 * \{ */

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

static void txt_delete_sel(Text *text)
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

void txt_sel_set(Text *text, int startl, int startc, int endl, int endc)
{
  TextLine *froml, *tol;
  int fromllen, tollen;

  /* Support negative indices. */
  if (startl < 0 || endl < 0) {
    int end = BLI_listbase_count(&text->lines) - 1;
    if (startl < 0) {
      startl = end + startl + 1;
    }
    if (endl < 0) {
      endl = end + endl + 1;
    }
  }
  CLAMP_MIN(startl, 0);
  CLAMP_MIN(endl, 0);

  froml = BLI_findlink(&text->lines, startl);
  if (froml == NULL) {
    froml = text->lines.last;
  }
  if (startl == endl) {
    tol = froml;
  }
  else {
    tol = BLI_findlink(&text->lines, endl);
    if (tol == NULL) {
      tol = text->lines.last;
    }
  }

  fromllen = BLI_strlen_utf8(froml->line);
  tollen = BLI_strlen_utf8(tol->line);

  /* Support negative indices. */
  if (startc < 0) {
    startc = fromllen + startc + 1;
  }
  if (endc < 0) {
    endc = tollen + endc + 1;
  }

  CLAMP(startc, 0, fromllen);
  CLAMP(endc, 0, tollen);

  text->curl = froml;
  text->curc = BLI_str_utf8_offset_from_index(froml->line, startc);
  text->sell = tol;
  text->selc = BLI_str_utf8_offset_from_index(tol->line, endc);
}

/* -------------------------------------------------------------------- */
/** \name Buffer Conversion for Undo/Redo
 *
 * Buffer conversion functions that rely on the buffer already being validated.
 *
 * The only requirement for these functions is that they're reverse-able,
 * the undo logic doesn't inspect their content.
 *
 * Currently buffers:
 * - Always ends with a new-line.
 * - Are not null terminated.
 * \{ */

/**
 * Create a buffer, the only requirement is #txt_from_buf_for_undo can decode it.
 */
char *txt_to_buf_for_undo(Text *text, int *r_buf_len)
{
  int buf_len = 0;
  LISTBASE_FOREACH (const TextLine *, l, &text->lines) {
    buf_len += l->len + 1;
  }
  char *buf = MEM_mallocN(buf_len, __func__);
  char *buf_step = buf;
  LISTBASE_FOREACH (const TextLine *, l, &text->lines) {
    memcpy(buf_step, l->line, l->len);
    buf_step += l->len;
    *buf_step++ = '\n';
  }
  *r_buf_len = buf_len;
  return buf;
}

/**
 * Decode a buffer from #txt_to_buf_for_undo.
 */
void txt_from_buf_for_undo(Text *text, const char *buf, int buf_len)
{
  const char *buf_end = buf + buf_len;
  const char *buf_step = buf;

  /* First re-use existing lines.
   * Good for undo since it means in practice many operations re-use all
   * except for the modified line. */
  TextLine *l_src = text->lines.first;
  BLI_listbase_clear(&text->lines);
  while (buf_step != buf_end && l_src) {
    /* New lines are ensured by #txt_to_buf_for_undo. */
    const char *buf_step_next = strchr(buf_step, '\n');
    const int len = buf_step_next - buf_step;

    TextLine *l = l_src;
    l_src = l_src->next;
    if (l->len != len) {
      l->line = MEM_reallocN(l->line, len + 1);
      l->len = len;
    }
    MEM_SAFE_FREE(l->format);

    memcpy(l->line, buf_step, len);
    l->line[len] = '\0';
    BLI_addtail(&text->lines, l);
    buf_step = buf_step_next + 1;
  }

  /* If we have extra lines. */
  while (l_src != NULL) {
    TextLine *l_src_next = l_src->next;
    MEM_freeN(l_src->line);
    if (l_src->format) {
      MEM_freeN(l_src->format);
    }
    MEM_freeN(l_src);
    l_src = l_src_next;
  }

  while (buf_step != buf_end) {
    /* New lines are ensured by #txt_to_buf_for_undo. */
    const char *buf_step_next = strchr(buf_step, '\n');
    const int len = buf_step_next - buf_step;

    TextLine *l = MEM_mallocN(sizeof(TextLine), "textline");
    l->line = MEM_mallocN(len + 1, "textline_string");
    l->len = len;
    l->format = NULL;

    memcpy(l->line, buf_step, len);
    l->line[len] = '\0';
    BLI_addtail(&text->lines, l);
    buf_step = buf_step_next + 1;
  }

  text->curl = text->sell = text->lines.first;
  text->curc = text->selc = 0;

  txt_make_dirty(text);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cut and Paste Functions
 * \{ */

char *txt_to_buf(Text *text, int *r_buf_strlen)
{
  int length;
  TextLine *tmp, *linef, *linel;
  int charf, charl;
  char *buf;

  if (r_buf_strlen) {
    *r_buf_strlen = 0;
  }

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

  if (r_buf_strlen) {
    *r_buf_strlen = length;
  }

  return buf;
}

char *txt_sel_to_buf(Text *text, int *r_buf_strlen)
{
  char *buf;
  int length = 0;
  TextLine *tmp, *linef, *linel;
  int charf, charl;

  if (r_buf_strlen) {
    *r_buf_strlen = 0;
  }

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

  if (r_buf_strlen) {
    *r_buf_strlen = length;
  }

  return buf;
}

void txt_insert_buf(Text *text, const char *in_buffer)
{
  int l = 0, len;
  size_t i = 0, j;
  TextLine *add;
  char *buffer;

  if (!in_buffer) {
    return;
  }

  txt_delete_sel(text);

  len = strlen(in_buffer);
  buffer = BLI_strdupn(in_buffer, len);
  len += txt_extended_ascii_as_utf8(&buffer);

  /* Read the first line (or as close as possible */
  while (buffer[i] && buffer[i] != '\n') {
    txt_add_raw_char(text, BLI_str_utf8_as_unicode_step(buffer, &i));
  }

  if (buffer[i] == '\n') {
    txt_split_curline(text);
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
          txt_add_raw_char(text, BLI_str_utf8_as_unicode_step(buffer, &j));
        }
        break;
      }
    }
  }

  MEM_freeN(buffer);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Find String in Text
 * \{ */

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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Line Editing Functions
 * \{ */

void txt_split_curline(Text *text)
{
  TextLine *ins;
  char *left, *right;

  if (!text->curl) {
    return;
  }

  txt_delete_sel(text);

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

void txt_duplicate_line(Text *text)
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
  }
}

void txt_delete_char(Text *text)
{
  unsigned int c = '\n';

  if (!text->curl) {
    return;
  }

  if (txt_has_sel(text)) { /* deleting a selection */
    txt_delete_sel(text);
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
    UNUSED_VARS(c);

    memmove(text->curl->line + text->curc,
            text->curl->line + text->curc + c_len,
            text->curl->len - text->curc - c_len + 1);

    text->curl->len -= c_len;

    txt_pop_sel(text);
  }

  txt_make_dirty(text);
  txt_clean_text(text);
}

void txt_delete_word(Text *text)
{
  txt_jump_right(text, true, true);
  txt_delete_sel(text);
  txt_make_dirty(text);
}

void txt_backspace_char(Text *text)
{
  unsigned int c = '\n';

  if (!text->curl) {
    return;
  }

  if (txt_has_sel(text)) { /* deleting a selection */
    txt_delete_sel(text);
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
    UNUSED_VARS(c);

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
}

void txt_backspace_word(Text *text)
{
  txt_jump_left(text, true, true);
  txt_delete_sel(text);
  txt_make_dirty(text);
}

/* Max spaces to replace a tab with, currently hardcoded to TXT_TABSIZE = 4.
 * Used by txt_convert_tab_to_spaces, indent and unindent.
 * Remember to change this string according to max tab size */
static char tab_to_spaces[] = "    ";

static void txt_convert_tab_to_spaces(Text *text)
{
  /* sb aims to pad adjust the tab-width needed so that the right number of spaces
   * is added so that the indention of the line is the right width (i.e. aligned
   * to multiples of TXT_TABSIZE)
   */
  const char *sb = &tab_to_spaces[text->curc % TXT_TABSIZE];
  txt_insert_buf(text, sb);
}

static bool txt_add_char_intern(Text *text, unsigned int add, bool replace_tabs)
{
  char *tmp, ch[BLI_UTF8_MAX];
  size_t add_len;

  if (!text->curl) {
    return 0;
  }

  if (add == '\n') {
    txt_split_curline(text);
    return true;
  }

  /* insert spaces rather than tabs */
  if (add == '\t' && replace_tabs) {
    txt_convert_tab_to_spaces(text);
    return true;
  }

  txt_delete_sel(text);

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

bool txt_add_char(Text *text, unsigned int add)
{
  return txt_add_char_intern(text, add, (text->flags & TXT_TABSTOSPACES) != 0);
}

bool txt_add_raw_char(Text *text, unsigned int add)
{
  return txt_add_char_intern(text, add, 0);
}

void txt_delete_selected(Text *text)
{
  txt_delete_sel(text);
  txt_make_dirty(text);
}

bool txt_replace_char(Text *text, unsigned int add)
{
  unsigned int del;
  size_t del_size = 0, add_size;
  char ch[BLI_UTF8_MAX];

  if (!text->curl) {
    return false;
  }

  /* If text is selected or we're at the end of the line just use txt_add_char */
  if (text->curc == text->curl->len || txt_has_sel(text) || add == '\n') {
    return txt_add_char(text, add);
  }

  del = BLI_str_utf8_as_unicode_and_size(text->curl->line + text->curc, &del_size);
  UNUSED_VARS(del);
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
  return true;
}

/**
 * Generic prefix operation, use for comment & indent.
 *
 * \note caller must handle undo.
 */
static void txt_select_prefix(Text *text, const char *add, bool skip_blank_lines)
{
  int len, num, curc_old, selc_old;
  char *tmp;

  const int indentlen = strlen(add);

  BLI_assert(!ELEM(NULL, text->curl, text->sell));

  curc_old = text->curc;
  selc_old = text->selc;

  num = 0;
  while (true) {

    /* don't indent blank lines */
    if ((text->curl->len != 0) || (skip_blank_lines == 0)) {
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
      if (text->curl->len != 0) {
        text->selc += indentlen;
      }
      break;
    }
    else {
      text->curl = text->curl->next;
      num++;
    }
  }

  while (num > 0) {
    text->curl = text->curl->prev;
    num--;
  }

  /* Keep the cursor left aligned if we don't have a selection. */
  if (curc_old == 0 && !(text->curl == text->sell && curc_old == selc_old)) {
    if (text->curl == text->sell) {
      if (text->curc == text->selc) {
        text->selc = 0;
      }
    }
    text->curc = 0;
  }
  else {
    if (text->curl->len != 0) {
      text->curc = curc_old + indentlen;
    }
  }
}

/**
 * Generic un-prefix operation, use for comment & indent.
 *
 * \param r_line_index_mask: List of lines that are already at indent level 0,
 * to store them later into the undo buffer.
 * \param require_all: When true, all non-empty lines must have this prefix.
 * Needed for comments where we might want to un-comment a block which contains some comments.
 *
 * \note caller must handle undo.
 */
static bool txt_select_unprefix(Text *text, const char *remove, const bool require_all)
{
  int num = 0;
  const int indentlen = strlen(remove);
  bool unindented_first = false;
  bool changed_any = false;

  BLI_assert(!ELEM(NULL, text->curl, text->sell));

  if (require_all) {
    /* Check all non-empty lines use this 'remove',
     * so the operation is applied equally or not at all. */
    TextLine *l = text->curl;
    while (true) {
      if (STREQLEN(l->line, remove, indentlen)) {
        /* pass */
      }
      else {
        /* Blank lines or whitespace can be skipped. */
        for (int i = 0; i < l->len; i++) {
          if (!ELEM(l->line[i], '\t', ' ')) {
            return false;
          }
        }
      }
      if (l == text->sell) {
        break;
      }
      l = l->next;
    }
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
      changed_any = true;
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
  return changed_any;
}

void txt_comment(Text *text)
{
  const char *prefix = "#";

  if (ELEM(NULL, text->curl, text->sell)) {
    return;
  }

  const bool skip_blank_lines = txt_has_sel(text);
  txt_select_prefix(text, prefix, skip_blank_lines);
}

bool txt_uncomment(Text *text)
{
  const char *prefix = "#";

  if (ELEM(NULL, text->curl, text->sell)) {
    return false;
  }

  return txt_select_unprefix(text, prefix, true);
}

void txt_indent(Text *text)
{
  const char *prefix = (text->flags & TXT_TABSTOSPACES) ? tab_to_spaces : "\t";

  if (ELEM(NULL, text->curl, text->sell)) {
    return;
  }

  txt_select_prefix(text, prefix, true);
}

bool txt_unindent(Text *text)
{
  const char *prefix = (text->flags & TXT_TABSTOSPACES) ? tab_to_spaces : "\t";

  if (ELEM(NULL, text->curl, text->sell)) {
    return false;
  }

  return txt_select_unprefix(text, prefix, false);
}

void txt_move_lines(struct Text *text, const int direction)
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
    // we only count those tabs/spaces that are before any text or before the curs;
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Character Queries
 * \{ */

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

/** \} */
